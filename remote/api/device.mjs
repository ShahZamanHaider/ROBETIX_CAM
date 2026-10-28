import {wrap,reply,json,body,fail,db,rpc,rate,device,hash,token,eventHeaders,isJPEG,validateConfig,providerConfigured,storage,notify,prune} from '../lib/service.mjs';
import {initializeRecipients} from '../lib/recipients.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 if(req.method!=='POST')fail(405,'POST required');const action=req.query?.action||new URL(req.url,'https://device.invalid').searchParams.get('action');
 if(action==='pair'){
 await rate('pair-ip:'+req.headers['x-forwarded-for'],10,600);const v=await json(req);if(typeof v.code!=='string'||!/^[A-Fa-f0-9]{10}$/.test(v.code)||typeof v.name!=='string'||!v.name.length||Buffer.byteLength(v.name)>64)fail(400,'Invalid pairing request');const credential=token();const id=await rpc('rv_claim_pair',{p_hash:hash(v.code.toUpperCase()),p_token:hash(credential),p_name:v.name});return reply(res,200,{id,token:credential});
 }
 const d=await device(req);
 if(action==='heartbeat'){
 await rate('heartbeat:'+d.id,6,60);const v=await json(req),settings=validateConfig(v);await initializeRecipients(d.config.opt_in?d:{...d,config:{...settings,phone:v.phone,opt_in:v.opt_in}});const name=typeof v.name==='string'&&v.name.length?v.name.slice(0,48):d.name;await db('rv_devices?id=eq.'+d.id,{method:'PATCH',data:{config:settings,name,last_seen:new Date().toISOString()}});const e=await db('rv_events?device=eq.'+d.id+'&order=created_at.desc&limit=1&select=local_event,notification,error');return reply(res,200,{provider_configured:providerConfigured(),notifications_paused:!!d.whatsapp_paused,notification:{local_event:e[0]?.local_event||0,state:e[0]?.notification||'disabled',error:e[0]?.error||''}});
 }
 if(action!=='event')fail(404,'Unknown action');if(!d.config.remote&&!d.config.whatsapp)fail(403,'Cloud access is disabled');await rate('event:'+d.id,10,60);await rate('event-day:'+d.id,1000,86400);const metadata=eventHeaders(req);if(metadata.test)await rate('test:'+d.id,1,60);const bytes=await body(req,131072),jpeg=req.headers['content-type']?.startsWith('image/jpeg');if(jpeg&&!isJPEG(bytes))fail(400,'Invalid JPEG');if(jpeg&&!d.config.remote&&(!d.config.whatsapp||!d.config.photo))fail(403,'Photo uploads are disabled');if(!jpeg&&bytes.toString()!=='{}')fail(400,'Expected JPEG or empty JSON object');
 let rows=await db('rv_events?on_conflict=device,event_key',{method:'POST',headers:{Prefer:'resolution=ignore-duplicates,return=representation'},data:{device:d.id,...Object.fromEntries(Object.entries(metadata).filter(([k])=>k!=='test')),is_test:metadata.test,notification:'uploading'}});
 let e=rows[0]||(await db('rv_events?device=eq.'+d.id+'&event_key=eq.'+encodeURIComponent(metadata.event_key)))[0];if(e.notification!=='uploading')return reply(res,200,{notification:e.notification,deduplicated:true});
 if(jpeg){const path=d.id+'/'+e.id+'.jpg';const uploaded=await storage('object/rv-media/'+path,{method:'POST',headers:{'Content-Type':'image/jpeg','x-upsert':'true'},body:bytes});if(!uploaded.ok)fail(503,'Photo upload failed; retry same event ID');await db('rv_events?id=eq.'+e.id,{method:'PATCH',data:{photo:path}});e.photo=path;}
 e.notification=await rpc('rv_queue',{p_device:d.id,p_event:e.id,p_test:metadata.test});let state=e.notification;if(state==='queued')state=await notify(d,e);await prune(d);reply(res,200,{notification:state});
});
