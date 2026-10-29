import {wrap,reply,json,fail,owner,db,rate,hash,env} from '../lib/service.mjs';
import {appOrigin,getSession,readCookie,sessionName} from '../lib/session.mjs';
import {uuid} from '../lib/recipients.mjs';
import {liveConfigured,liveTicket} from '../lib/live.mjs';
import {command,reads} from '../lib/commands.mjs';
export const config={api:{bodyParser:false}};
async function execute(id,uid,request){
 const body=JSON.stringify(request),proof=liveTicket(id,'rpc',{subject:uid,request_hash:hash(body)});
 let r;try{r=await fetch(env('LIVE_RELAY_URL')+'/rpc/'+id,{method:'POST',headers:{Authorization:'Bearer '+proof.ticket,'Content-Type':'application/json'},body,redirect:'error',signal:AbortSignal.timeout(13000)})}catch{fail(504,'No device confirmation received. Check camera status before trying again.')}
 const reader=r.body.getReader(),parts=[];let count=0;for(;;){const chunk=await reader.read();if(chunk.done)break;count+=chunk.value.byteLength;if(count>8192){await reader.cancel();fail(502,'Camera response is too large')}parts.push(Buffer.from(chunk.value))}let value;try{value=JSON.parse(Buffer.concat(parts).toString())}catch{fail(502,'Camera connection failed')}
 if(!r.ok)fail(r.status>=400&&r.status<600?r.status:502,value.error||'Camera command failed');
 if(value.id!==request.id||value.ok!==true)fail(502,'Camera did not confirm this command');return value.data;
}
export default wrap(async(req,res)=>{
 const uid=await owner(req,res);if(req.method!=='POST')fail(405,'POST required');
 const v=await json(req);if(!uuid(v.device)||typeof v.id!=='string'||!/^[a-f0-9]{32}$/.test(v.id))fail(400,'Choose a camera');
 const params=command(v.action,v.params),d=(await db('rv_devices?id=eq.'+v.device+'&owner=eq.'+uid+'&select=*'))[0];
 if(!d)fail(404,'Camera not found');if(!d.enabled||!d.config.remote)fail(403,'Remote viewing is off on this camera');
 if(!liveConfigured())fail(503,'Live viewing setup is not complete');
 if(d.config.remote_control_protocol!==2)fail(426,'Update this camera to enable remote controls');
 if(!d.last_seen||Date.now()-Date.parse(d.last_seen)>150000)fail(503,'Camera is offline. Check its power and Wi-Fi.');
 await rate('control:'+uid,180,60);if(!reads.has(v.action)&&v.action!=='download')await rate('control-change:'+d.id,12,60);
 if(v.action==='restart'){if(!readCookie(req,sessionName))fail(403,'Sign in again to restart the camera');const s=await getSession(req,res);if(!s.issued_at||Date.now()-s.issued_at>600000)fail(403,'Sign in again to restart the camera')}
 const data=await execute(d.id,uid,{id:v.id,action:v.action==='download'?'file_stat':v.action,params});
 if(v.action==='download'){
  if(!Number.isInteger(data.size)||data.size<1||data.size>4294967295)fail(502,'Invalid file information');
  const proof=liveTicket(d.id,'file',{subject:uid,origin:appOrigin(req),name:params.name,kind:params.kind,file_size:data.size});
  return reply(res,200,{...proof,relay:env('LIVE_RELAY_URL')+'/chunk/'+d.id,name:params.name,size:data.size});
 }
 return reply(res,200,{id:v.id,confirmed:true,data});
});
