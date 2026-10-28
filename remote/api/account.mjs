import {randomBytes} from 'node:crypto';
import {wrap,reply,json,fail,env,fetchBounded,owner,db,rate,hash,storage,prune} from '../lib/service.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 const action=req.query?.action||new URL(req.url,'https://account.invalid').searchParams.get('action');
 if(action==='login'){
 if(req.method!=='POST')fail(405,'POST required');await rate('login:'+req.headers['x-forwarded-for'],10,600);const v=await json(req);if(typeof v.email!=='string'||v.email.length>254||typeof v.password!=='string'||v.password.length>256)fail(400,'Invalid login');const r=await fetchBounded(env('SUPABASE_URL')+'/auth/v1/token?grant_type=password',{method:'POST',headers:{apikey:env('SUPABASE_ANON_KEY'),'Content-Type':'application/json'},body:JSON.stringify({email:v.email,password:v.password})},16384);if(!r.ok)fail(401,'Check your account credentials');const data=r.json();return reply(res,200,{access_token:data.access_token,expires_in:data.expires_in});
 }
 const uid=await owner(req,res);await rate('owner:'+uid,120,60);
 if(action==='pair'){
 if(req.method!=='POST')fail(405,'POST required');await rate('pair-owner:'+uid,5,600);const code=randomBytes(5).toString('hex').toUpperCase();await db('rv_pair_codes?owner=eq.'+uid,{method:'DELETE'});await db('rv_pair_codes',{method:'POST',data:{owner:uid,code_hash:hash(code),expires:new Date(Date.now()+300000).toISOString()}});return reply(res,200,{code,expires_in:300});
 }
 const devices=await db('rv_devices?owner=eq.'+uid+'&select=id,name,enabled,last_seen,config');
 if(action==='devices'&&req.method==='GET')return reply(res,200,{devices:devices.map(d=>({id:d.id,name:d.name,enabled:d.enabled,remote:!!d.config.remote,last_seen:d.last_seen,online:!!d.enabled&&Date.now()-Date.parse(d.last_seen)<150000}))});
 const id=req.query?.id||new URL(req.url,'https://account.invalid').searchParams.get('id'),d=devices.find(d=>d.id===id);if(!d)fail(404,'Camera not found');
 if(action==='revoke'&&req.method==='POST'){await db('rv_devices?id=eq.'+d.id,{method:'PATCH',data:{enabled:false,token_hash:hash(randomBytes(32))}});return reply(res,200,{revoked:true})}
 if(!d.enabled||!d.config.remote)fail(403,'Remote photo access is disabled');
 if(action==='events'&&req.method==='GET'){await prune(d);const events=await db('rv_events?device=eq.'+d.id+'&order=created_at.desc&limit=12&select=id,created_at,epoch,offset_minutes,score,notification,photo');return reply(res,200,{events:events.map(e=>({...e,photo:!!e.photo}))})}
 if(action==='photo'&&req.method==='GET'){
 const event=req.query?.event||new URL(req.url,'https://account.invalid').searchParams.get('event');if(!/^[a-f0-9-]{36}$/.test(event||''))fail(400,'Invalid photo');const rows=await db('rv_events?id=eq.'+event+'&device=eq.'+d.id+'&select=photo,created_at');const e=rows[0];if(!e?.photo||Date.now()-Date.parse(e.created_at)>(d.config.retention_days||7)*86400000)fail(404,'Photo expired');const photo=await storage('object/rv-media/'+e.photo);if(!photo.ok)fail(404,'Photo unavailable');res.setHeader('Cache-Control','no-store');res.setHeader('Content-Type','image/jpeg');res.setHeader('Content-Length',photo.bytes.length);res.statusCode=200;return res.end(photo.bytes);
 }
 fail(404,'Unknown action');
});
