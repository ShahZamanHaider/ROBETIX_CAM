import {timingSafeEqual} from 'node:crypto';
import {wrap,reply,fail,env,db,notify,prune} from '../lib/service.mjs';
import {summarize} from '../lib/recipients.mjs';
export default wrap(async(req,res)=>{
 const provided=Buffer.from(req.headers.authorization||''),expected=Buffer.from('Bearer '+env('CRON_SECRET'));if(provided.length!==expected.length||!timingSafeEqual(provided,expected))fail(401,'Unauthorized');
 // Each run is bounded. Unknown/sending states are never retried automatically.
 const expired=await db('rv_deliveries?state=eq.sending&updated_at=lt.'+encodeURIComponent(new Date(Date.now()-300000).toISOString()),{method:'PATCH',data:{state:'unknown',error:'Worker ended before outcome was saved; not retried',updated_at:new Date().toISOString()}});for(const id of new Set(expired.slice(0,20).map(x=>x.event)))await summarize(id);
 await db('rv_events?notification=eq.sending&created_at=lt.'+encodeURIComponent(new Date(Date.now()-300000).toISOString()),{method:'PATCH',data:{notification:'unknown',error:'Worker ended before outcome was saved; not retried'}});
 const events=await db('rv_events?notification=eq.queued&order=created_at.asc&limit=2');for(const e of events){const d=(await db('rv_devices?id=eq.'+e.device))[0];if(d)await notify(d,e)}
 const devices=await db('rv_devices?order=last_seen.asc.nullsfirst&limit=100');for(const d of devices)if(!d.config?.account_removed)await prune(d);
 await db('rv_pair_codes?expires=lt.'+encodeURIComponent(new Date().toISOString()),{method:'DELETE'});await db('rv_rates?start_at=lt.'+encodeURIComponent(new Date(Date.now()-86400000).toISOString()),{method:'DELETE'});reply(res,200,{processed:events.length});
});
