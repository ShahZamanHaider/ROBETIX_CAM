import {reply} from '../lib/service.mjs';
export default function(req,res){return reply(res,req.method==='GET'?200:405,req.method==='GET'?{service:'Robetix Vision',version:2,status:'running'}:{error:'GET required'})}
