import {wrap,reply,body,fail,env,db,webhookValid} from '../lib/service.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 if(req.method==='GET'){const q=req.query||Object.fromEntries(new URL(req.url,'https://webhook.invalid').searchParams);if(q['hub.mode']!=='subscribe'||q['hub.verify_token']!==env('META_WEBHOOK_VERIFY_TOKEN'))fail(403,'Invalid verification');res.statusCode=200;return res.end(q['hub.challenge'])}
 if(req.method!=='POST')fail(405,'POST required');const raw=await body(req,262144);if(!webhookValid(raw,req.headers['x-hub-signature-256']))fail(401,'Invalid signature');let v;try{v=JSON.parse(raw)}catch{fail(400,'Invalid JSON')}
 for(const entry of (v.entry||[]).slice(0,20))for(const change of (entry.changes||[]).slice(0,20))for(const s of (change.value?.statuses||[]).slice(0,50))if(typeof s.id==='string'&&s.id.length<200&&['sent','delivered','read','failed'].includes(s.status))await db('rv_events?provider_id=eq.'+encodeURIComponent(s.id)+(s.status==='read'?'':s.status==='delivered'?'&notification=neq.read':'&notification=not.in.(delivered,read)'),{method:'PATCH',data:{notification:s.status==='sent'?'submitted':s.status}});reply(res,200,{received:true});
});
