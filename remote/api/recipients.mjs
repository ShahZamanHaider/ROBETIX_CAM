import {wrap,owner,rate,db,rpc,json,fail,reply,providerConfigured,notify,prune} from '../lib/service.mjs';
import {uuid,recipients,publicRecipient,recipientValue,initializeRecipients} from '../lib/recipients.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 const uid=await owner(req,res);await rate('recipient-admin:'+uid,90,60);
 const q=new URL(req.url,'https://recipients.invalid').searchParams,device=q.get('device'),action=q.get('action')||'list';
 if(!uuid(device))fail(400,'Invalid camera');const d=(await db('rv_devices?id=eq.'+device+'&owner=eq.'+uid))[0];if(!d)fail(404,'Camera not found');
 if(action==='list'&&req.method==='GET'){
  const contacts=await recipients(d),result=await Promise.all(contacts.map(async r=>({...publicRecipient(r),last_notification:(await db('rv_deliveries?recipient=eq.'+r.id+'&order=created_at.desc&limit=1&select=state,error,updated_at'))[0]||null})));
  return reply(res,200,{recipients:result,provider_configured:providerConfigured(),paused:!!d.whatsapp_paused,camera_alerts:!!d.enabled&&!!d.config.whatsapp,max_recipients:5});
 }
 if(req.method!=='POST')fail(405,'POST required');if(!req.headers['content-type']?.startsWith('application/json'))fail(415,'JSON required');const v=await json(req);if(!v||typeof v!=='object'||Array.isArray(v))fail(400,'Invalid settings');await initializeRecipients(d);
 if(action==='pause'){if(typeof v.paused!=='boolean')fail(400,'Invalid setting');await db('rv_devices?id=eq.'+d.id,{method:'PATCH',data:{whatsapp_paused:v.paused}});return reply(res,200,{paused:v.paused})}
 if(!['save','remove','test'].includes(action))fail(404,'Unknown action');const id=v.id??null;if(id!==null&&!uuid(id))fail(400,'Invalid recipient');
 if(action==='save'){const result=await rpc('rv_save_recipient',{p_device:d.id,p_id:id,p_value:recipientValue(v)});if(result==='not_found')fail(404,'Recipient not found');if(result==='duplicate')fail(409,'This phone number is already added');if(result==='limit')fail(409,'You can add up to five recipients');if(result!=='saved')fail(502,'Recipient could not be saved');return reply(res,200,{saved:true})}
 if(!id)fail(400,'Invalid recipient');const recipient=(await db('rv_recipients?id=eq.'+id+'&device=eq.'+d.id))[0];if(!recipient)fail(404,'Recipient not found');
 if(action==='remove'){await db('rv_recipients?id=eq.'+id+'&device=eq.'+d.id,{method:'DELETE'});return reply(res,200,{removed:true})}
 if(!recipient.enabled||!recipient.consent)fail(409,'Enable this recipient and confirm consent first');if(!d.enabled||!d.config.whatsapp||d.whatsapp_paused)fail(409,'Enable WhatsApp alerts and resume notifications first');if(!providerConfigured())fail(503,'WhatsApp setup incomplete');
 if(!uuid(v.request_id))fail(400,'Invalid test request');await rate('recipient-test:'+d.id,1,60);
 let photo=null,epoch=0,offset_minutes=0;if(recipient.photo&&d.config.photo){const latest=(await db('rv_events?device=eq.'+d.id+'&photo=not.is.null&created_at=gt.'+encodeURIComponent(new Date(Date.now()-300000).toISOString())+'&order=created_at.desc&limit=1'))[0];if(!latest)fail(409,'A recent camera photo is needed. Use Test Notification on the camera first.');photo=latest.photo;epoch=latest.epoch;offset_minutes=latest.offset_minutes;}
 // The test borrows the image only for this request; no shared object path is persisted twice.
 const eventKey='recipient_test_'+v.request_id;let e=(await db('rv_events?on_conflict=device,event_key',{method:'POST',headers:{Prefer:'resolution=ignore-duplicates,return=representation'},data:{device:d.id,event_key:eventKey,is_test:true,epoch,offset_minutes,photo:null,notification:'queued'}}))[0];if(!e)e=(await db('rv_events?device=eq.'+d.id+'&event_key=eq.'+eventKey))[0];
 const notification=await notify(d,{...e,photo},id);await prune(d);return reply(res,200,{notification});
});
