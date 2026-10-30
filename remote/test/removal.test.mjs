import {test,beforeEach} from 'node:test';
import assert from 'node:assert/strict';
import {Readable} from 'node:stream';
import {randomBytes} from 'node:crypto';
import account from '../api/account.mjs';
import device from '../api/device.mjs';
import live from '../api/live.mjs';
import {hash} from '../lib/service.mjs';
const id='11111111-1111-4111-8111-111111111111',token=randomBytes(32).toString('hex');
Object.assign(process.env,{SUPABASE_URL:'https://db.test',SUPABASE_SERVICE_ROLE_KEY:'fixture-service',SUPABASE_ANON_KEY:'fixture-public',LIVE_RELAY_URL:'https://relay.test',LIVE_RELAY_SECRET:randomBytes(32).toString('hex')});
let row,calls,relayDown,race,failWrite;
beforeEach(()=>{row={id,owner:'owner',enabled:true,recipients_initialized:true,token_hash:hash(token),config:{remote:true},last_seen:new Date().toISOString()};calls=[];relayDown=race=failWrite=false});
globalThis.fetch=async(url,o={})=>{
 const u=new URL(url),body=o.body?JSON.parse(o.body):null;calls.push({path:u.pathname,method:o.method||'GET',body});
 if(u.pathname==='/auth/v1/user')return Response.json({id:o.headers.Authorization==='Bearer '+'o'.repeat(30)?'owner':'stranger'});
 if(u.pathname.endsWith('/rpc/rv_rate'))return Response.json(true);
 if(u.hostname==='relay.test'){if(relayDown)return Response.json({}, {status:503});return Response.json({stopped:true})}
 if(u.pathname.endsWith('/rv_devices')){
  if(u.searchParams.has('owner')&&u.searchParams.get('owner')!=='eq.owner')return Response.json([]);
  if(o.method==='PATCH'){
   if(failWrite)return Response.json([]);
   if(race){row.enabled=false;row.config.account_removed=true;row.token_hash=hash('rotated');race=false}
   if(u.searchParams.get('enabled')==='eq.true'&&!row.enabled||u.searchParams.has('token_hash')&&u.searchParams.get('token_hash')!=='eq.'+row.token_hash)return Response.json([]);
   Object.assign(row,body);return Response.json([row]);
  }
  if(u.searchParams.has('token_hash')&&(!row.enabled||u.searchParams.get('token_hash')!=='eq.'+row.token_hash))return Response.json([]);
  return Response.json([structuredClone(row)]);
 }
 if(u.pathname.endsWith('/rv_events'))return Response.json([]);
 throw Error('Unexpected endpoint '+u.pathname);
};
async function invoke(handler,path,body={},auth='o'.repeat(30),method='POST'){
 const req=Readable.from([Buffer.from(JSON.stringify(body))]);Object.assign(req,{url:path,method,headers:{host:'app.test',origin:'https://app.test','x-forwarded-proto':'https','content-type':'application/json',...(auth?{authorization:'Bearer '+auth}:{})}});
 let value;const res={setHeader(){},end(v){value=JSON.parse(v)}};await handler(req,res);return {status:res.statusCode,value};
}
test('removal requires owner authorization and explicit confirmation',async()=>{
 const path='/api/account?action=remove&id='+id;
 assert.equal((await invoke(account,path,{confirm:true},'')).status,401);
 assert.equal((await invoke(account,path,{confirm:true},'r'.repeat(30))).status,404);
 assert.equal((await invoke(account,path,{})).status,400);
 assert.equal(row.enabled,true);assert(!calls.some(c=>c.method==='PATCH'));
});
test('offline camera removal archives, revokes, pauses alerts and preserves history and hardware ownership',async()=>{
 const r=await invoke(account,'/api/account?action=remove&id='+id,{confirm:true});
 assert.equal(r.status,200);assert.equal(r.value.removed,true);assert.equal(r.value.live_stopped,true);
 assert.equal(row.enabled,false);assert.equal(row.whatsapp_paused,true);assert.notEqual(row.token_hash,hash(token));assert.equal(row.owner,'owner');
 assert(!calls.some(c=>c.method==='DELETE'));assert(!calls.some(c=>c.path.includes('rv_hardware_claims')));
 const list=await invoke(account,'/api/account?action=devices',{},'o'.repeat(30),'GET');assert.deepEqual(list.value.devices,[]);
 assert.equal((await invoke(device,'/api/device?action=heartbeat',{},token)).status,401);
 assert.equal((await invoke(live,'/api/live',{device:id,quality:0})).status,403);
});
test('relay outage is reported honestly; failure to persist removal never reports success',async()=>{
 relayDown=true;let r=await invoke(account,'/api/account?action=remove&id='+id,{confirm:true});assert.equal(r.value.live_stopped,false);assert.equal(r.value.session_expiry_seconds,90);
 failWrite=true;r=await invoke(account,'/api/account?action=remove&id='+id,{confirm:true});assert.equal(r.status,502);assert(!r.value.removed);
});
test('in-flight heartbeat cannot overwrite removal or report a successful reconnection',async()=>{
 race=true;const r=await invoke(device,'/api/device?action=heartbeat',{remote:true,whatsapp:false,photo:false,interval:60},token);
 assert.equal(r.status,401);assert.equal(row.config.account_removed,true);assert.equal(row.enabled,false);
});
