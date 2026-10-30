import {reply} from '../lib/service.mjs';
import {liveConfigured} from '../lib/live.mjs';
import {customerOrigin} from '../lib/origins.mjs';
// Public service metadata only. This does not expose sessions or device state.
export default function(req,res){
 res.setHeader('Vary','Origin');
 if(req.headers.origin==='http://192.168.4.1')res.setHeader('Access-Control-Allow-Origin','http://192.168.4.1');
 if(req.method!=='GET')return reply(res,405,{error:'GET required'});
 const ready=!!(process.env.SUPABASE_URL&&process.env.SUPABASE_ANON_KEY&&process.env.SUPABASE_SERVICE_ROLE_KEY&&liveConfigured());
 return reply(res,200,{service:'Robetix Vision',protocol:1,customer_origin:customerOrigin,environment:process.env.VERCEL_ENV||'development',ready});
}
