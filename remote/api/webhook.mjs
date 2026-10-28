import {wrap,reply,body,fail,env,db,webhookValid} from '../lib/service.mjs';
import {summarize} from '../lib/recipients.mjs';
export const config={api:{bodyParser:false}};
export default wrap(async(req,res)=>{
 if(req.method==='GET'){const q=req.query||Object.fromEntries(new URL(req.url,'https://webhook.invalid').searchParams);if(q['hub.mode']!=='subscribe'||q['hub.verify_token']!==env('META_WEBHOOK_VERIFY_TOKEN'))fail(403,'Invalid verification');res.statusCode=200;return res.end(q['hub.challenge'])}
 if(req.method!=='POST')fail(405,'POST required');const raw=await body(req,262144);if(!webhookValid(raw,req.headers['x-hub-signature-256']))fail(401,'Invalid signature');let v;try{v=JSON.parse(raw)}catch{fail(400,'Invalid JSON')}
 const touched=new Set();let count=0;
 for(const entry of (v.entry||[]).slice(0,20))for(const change of (entry.changes||[]).slice(0,20))for(const s of (change.value?.statuses||[]).slice(0,50))if(count++<50&&typeof s.id==='string'&&s.id.length<200&&['sent','delivered','read','failed'].includes(s.status)){
  const filter=s.status==='read'?'':s.status==='delivered'?'&state=neq.read':'&state=not.in.(delivered,read)';const rows=await db('rv_deliveries?provider_id=eq.'+encodeURIComponent(s.id)+filter,{method:'PATCH',data:{state:s.status,updated_at:new Date().toISOString(),error:s.status==='failed'?'WhatsApp provider rejected the message (code '+Number(s.errors?.[0]?.code||0)+')':null}});for(const r of rows)touched.add(r.event);
  // Preserve receipts for pre-migration single-recipient events.
  if(!rows.length)await db('rv_events?provider_id=eq.'+encodeURIComponent(s.id)+(s.status==='read'?'':s.status==='delivered'?'&notification=neq.read':'&notification=not.in.(delivered,read)'),{method:'PATCH',data:{notification:s.status==='sent'?'submitted':s.status}});
 }
 for(const id of touched)await summarize(id);reply(res,200,{received:true});
});
