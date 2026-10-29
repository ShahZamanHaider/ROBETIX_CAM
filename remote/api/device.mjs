import {wrap,reply,json,body,fail,db,rpc,rate,device,hash,token,eventHeaders,isJPEG,validateConfig,providerConfigured,storage,notify,prune} from '../lib/service.mjs';
import {initializeRecipients} from '../lib/recipients.mjs';
import {pairCamera} from '../lib/pairing.mjs';
import {liveConfigured,liveTicket,stopLive} from '../lib/live.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 if(req.method!=='POST')fail(405,'POST required');const action=req.query?.action||new URL(req.url,'https://device.invalid').searchParams.get('action');
 if(action==='capabilities')return reply(res,200,{protocol:2,setup_ready:process.env.CAMERA_SETUP_ENABLED==='true',live_ready:liveConfigured()});
 if(action==='pair'){
 await rate('pair-ip:'+req.headers['x-forwarded-for'],10,600);const v=await json(req);if(typeof v.code!=='string'||!/^[A-Fa-f0-9]{10}$/.test(v.code)||typeof v.name!=='string'||!v.name.length||Buffer.byteLength(v.name)>64)fail(400,'Invalid pairing request');if(v.hardware_tag!==undefined){if(process.env.CAMERA_SETUP_ENABLED!=='true')fail(503,'Camera setup needs a service update');if(typeof v.hardware_tag!=='string'||!/^[a-f0-9]{32}$/.test(v.hardware_tag))fail(400,'Invalid pairing request')}return reply(res,200,await pairCamera(v));
 }
 const d=await device(req);
 if(action==='factory-reset'){
  await rate('camera-reset:'+d.id,3,60);await json(req);
  // Rotate the device credential without releasing its hardware ownership.
  // Setting enabled=false here would let a reset device be claimed by anyone.
  await db('rv_devices?id=eq.'+d.id,{method:'PATCH',data:{token_hash:hash(token()),last_seen:null}});
  let live_stopped=true;try{await stopLive(d.id)}catch{live_stopped=false}
  return reply(res,200,{credentials_revoked:true,ownership_retained:true,live_stopped});
 }
 if(action==='live'){await rate('camera-live:'+d.id,20,60);if(!d.config.remote)fail(403,'Remote viewing is off on this camera');return reply(res,200,liveTicket(d.id,'camera'));}
 if(action==='heartbeat'){
 await rate('heartbeat:'+d.id,6,60);const v=await json(req),settings=validateConfig(v);if(v.control_protocol===2){settings.remote_control_protocol=2;settings.firmware=typeof v.firmware==='string'?v.firmware.slice(0,40):'remote-cctv-v2';if(v.stream&&typeof v.stream==='object')settings.stream={state:String(v.stream.state||'').slice(0,30),sent:Math.max(0,Number(v.stream.sent)||0),acknowledged:Math.max(0,Number(v.stream.acknowledged)||0),error:String(v.stream.error||'').slice(0,80)}}if(v.hardware_tag!==undefined&&process.env.CAMERA_SETUP_ENABLED==='true'){if(typeof v.hardware_tag!=='string'||!/^[a-f0-9]{32}$/.test(v.hardware_tag))fail(400,'Invalid pairing request');if(!await rpc('rv_bind_hardware',{p_device:d.id,p_hardware:v.hardware_tag}))fail(409,'Camera ownership needs attention');}await initializeRecipients(d.config.opt_in?d:{...d,config:{...settings,phone:v.phone,opt_in:v.opt_in}});const name=typeof v.name==='string'&&v.name.length?v.name.slice(0,48):d.name;await db('rv_devices?id=eq.'+d.id,{method:'PATCH',data:{config:settings,name,...(v.name_changed===true&&process.env.CAMERA_SETUP_ENABLED==='true'?{display_name:name}:{}),last_seen:new Date().toISOString()}});const e=await db('rv_events?device=eq.'+d.id+'&order=created_at.desc&limit=1&select=local_event,notification,error');return reply(res,200,{camera_name:v.name_changed===true?name:d.display_name||name,live:settings.remote&&liveConfigured()?liveTicket(d.id,'camera'):null,provider_configured:providerConfigured(),notifications_paused:!!d.whatsapp_paused,notification:{local_event:e[0]?.local_event||0,state:e[0]?.notification||'disabled',error:e[0]?.error||''}});
 }
 if(action!=='event')fail(404,'Unknown action');if(!d.config.remote&&!d.config.whatsapp)fail(403,'Cloud access is disabled');await rate('event:'+d.id,10,60);await rate('event-day:'+d.id,1000,86400);const metadata=eventHeaders(req);if(metadata.test)await rate('test:'+d.id,1,60);const bytes=await body(req,131072),jpeg=req.headers['content-type']?.startsWith('image/jpeg');if(jpeg&&!isJPEG(bytes))fail(400,'Invalid JPEG');if(jpeg&&!d.config.remote&&(!d.config.whatsapp||!d.config.photo))fail(403,'Photo uploads are disabled');if(!jpeg&&bytes.toString()!=='{}')fail(400,'Expected JPEG or empty JSON object');
 let rows=await db('rv_events?on_conflict=device,event_key',{method:'POST',headers:{Prefer:'resolution=ignore-duplicates,return=representation'},data:{device:d.id,...Object.fromEntries(Object.entries(metadata).filter(([k])=>k!=='test')),is_test:metadata.test,notification:'uploading'}});
 let e=rows[0]||(await db('rv_events?device=eq.'+d.id+'&event_key=eq.'+encodeURIComponent(metadata.event_key)))[0];if(e.notification!=='uploading')return reply(res,200,{notification:e.notification,deduplicated:true});
 if(jpeg){const path=d.id+'/'+e.id+'.jpg';const uploaded=await storage('object/rv-media/'+path,{method:'POST',headers:{'Content-Type':'image/jpeg','x-upsert':'true'},body:bytes});if(!uploaded.ok)fail(503,'Photo upload failed; retry same event ID');await db('rv_events?id=eq.'+e.id,{method:'PATCH',data:{photo:path}});e.photo=path;}
 e.notification=await rpc('rv_queue',{p_device:d.id,p_event:e.id,p_test:metadata.test});let state=e.notification;if(state==='queued')state=await notify(d,e);await prune(d);reply(res,200,{notification:state});
});
