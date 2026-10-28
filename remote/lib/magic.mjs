import {randomBytes,createHash} from 'node:crypto';
import {fail,rate,hash} from './service.mjs';
import {authRequest,appOrigin,recoveryOrigin,seal,unseal,readCookie,newSession,setSession} from './session.mjs';
export const magicName='__Host-rv-magic';
function cookie(res,value,age){const old=res.getHeader?.('Set-Cookie')||[];res.setHeader('Set-Cookie',[...(Array.isArray(old)?old:[old]),magicName+'='+value+'; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age='+age])}
export function clearMagic(res){cookie(res,'',0)}
export async function sendMagic(req,res,v){
 if(typeof v.email!=='string'||v.email.length>254||!/^\S+@\S+\.\S+$/.test(v.email)||typeof v.remember!=='boolean')fail(400,'Enter your registered email address');
 await rate('magic-ip:'+req.headers['x-forwarded-for'],5,3600);await rate('magic-email:'+hash(v.email.trim().toLowerCase()),3,3600);
 // The verifier never reaches JavaScript, URL history, email, or localStorage.
 const verifier=randomBytes(32).toString('base64url'),challenge=createHash('sha256').update(verifier).digest('base64url');
 const state={kind:'magic',aud:appOrigin(req),verifier,remember:v.remember,until:Date.now()+600000};
 let response;try{response=await authRequest('otp?redirect_to='+encodeURIComponent(recoveryOrigin(req)+'/auth/callback'),{method:'POST',data:{email:v.email.trim(),create_user:false,code_challenge:challenge,code_challenge_method:'s256'}})}catch{fail(503,'Connection interrupted. Please try again.')}
 if(response.status>=500)fail(503,'Connection interrupted. Please try again.');
 if(response.status===429)fail(429,'Try again later');
 // Same result for unknown/disabled accounts; registration is never requested.
 cookie(res,seal(state),600);
 return {message:'If this email belongs to an existing account, a sign-in link will arrive. Open it in this browser within 10 minutes.'};
}
export async function exchangeMagic(req,res,v){
 await rate('magic-verify:'+req.headers['x-forwarded-for'],10,600);
 const state=unseal(readCookie(req,magicName));
 if(!state||state.kind!=='magic'||state.aud!==appOrigin(req)||state.until<=Date.now()||typeof state.verifier!=='string'||!/^[A-Za-z0-9_-]{43}$/.test(state.verifier)||typeof v.code!=='string'||!/^[A-Za-z0-9_-]{20,256}$/.test(v.code))fail(401,'This sign-in link expired or was opened in a different browser. Request a new link here.');
 let r;try{r=await authRequest('token?grant_type=pkce',{method:'POST',data:{auth_code:v.code,code_verifier:state.verifier}})}catch{fail(503,'Connection interrupted. Please try again.')}
 if(!r.ok){if(r.status>=500)fail(503,'Connection interrupted. Please try again.');cookie(res,'',0);fail(401,'This sign-in link expired or was opened in a different browser. Request a new link here.')}
 const s=newSession(req,r.json(),state.remember);if(!s.uid)fail(502,'Sign in could not be completed');
 cookie(res,'',0);setSession(req,res,s);return {authenticated:true,csrf:s.csrf,expires_at:s.expires};
}
