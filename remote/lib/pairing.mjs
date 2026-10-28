import {createHmac} from 'node:crypto';
import {db,env,fail,fetchBounded,hash,token} from './service.mjs';
async function claim(name,data){
 const key=env('SUPABASE_SERVICE_ROLE_KEY'),r=await fetchBounded(env('SUPABASE_URL')+'/rest/v1/rpc/'+name,{method:'POST',headers:{apikey:key,Authorization:'Bearer '+key,'Content-Type':'application/json'},body:JSON.stringify(data)},8192);
 if(r.ok)return r.json();let message='';try{message=r.json().message||''}catch{}
 const errors=new Map([['Pairing expired or invalid',[400,'Pairing code expired. Get a new code from your account.']],['Camera already registered; owner must disconnect it first',[409,'This camera already belongs to an account. Disconnect it there first.']],['Camera limit reached',[409,'Your account has reached its camera limit.']]]);
 if(errors.has(message)){const [status,text]=errors.get(message);fail(status,text)}fail(r.status>=500?503:502,'Camera pairing could not be completed. Please try again.');
}
export async function pairCamera(v){
 const code=v.code.toUpperCase(),codeHash=hash(code),version=v.hardware_tag!==undefined,request=v.request_id;
 if(request!==undefined&&(!version||typeof request!=='string'||!/^[a-f0-9]{32}$/.test(request)))fail(400,'Invalid pairing request');
 let credential=token(),marker;
 if(request){
  credential=createHmac('sha256',env('SUPABASE_SERVICE_ROLE_KEY')).update('robetix-pair-credential-v1\0'+code+'\0'+v.hardware_tag+'\0'+request).digest('hex');marker=hash('pair-attempt:'+credential);
  const prior=(await db('rv_rates?key=eq.'+marker+'&select=start_at'))[0];
  if(prior){const started=Date.parse(prior.start_at);if(!Number.isFinite(started)||Date.now()-started>300000||started>Date.now()+10000)fail(400,'Pairing code expired. Get a new code from your account.');const existing=(await db('rv_devices?token_hash=eq.'+hash(credential)+'&enabled=eq.true&select=id'))[0];if(existing)return {id:existing.id,token:credential,protocol:2,resumed:true}}
  else{
   // Never recreate a retry window for a code that has already been consumed.
   const pending=(await db('rv_pair_codes?code_hash=eq.'+codeHash+'&expires=gt.'+encodeURIComponent(new Date().toISOString())+'&select=code_hash&limit=1'))[0];if(!pending)fail(400,'Pairing code expired. Get a new code from your account.');
   await db('rv_rates?on_conflict=key',{method:'POST',headers:{Prefer:'resolution=ignore-duplicates,return=representation'},data:{key:marker,start_at:new Date().toISOString(),count:1}});
  }
 }
 const args={p_hash:codeHash,p_token:hash(credential),p_name:v.name,...(version?{p_hardware:v.hardware_tag}:{})};const id=await claim(version?'rv_claim_pair_v2':'rv_claim_pair',args);return {id,token:credential,protocol:2};
}
