import {reply} from '../lib/service.mjs';
export default function(req,res){return reply(res,req.method==='GET'?200:405,req.method==='GET'?{service:'Robetix Vision',version:6,status:'running',...(process.env.VERCEL_GIT_COMMIT_SHA?{commit:process.env.VERCEL_GIT_COMMIT_SHA}:{})}:{error:'GET required'})}
