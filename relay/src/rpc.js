// Every pending read is bounded; only mutation receipts survive room hibernation.
const READ=new Set(['status','files','events','file_stat','file_chunk']);
const ACTIONS=new Set([...READ,'record','stop','photo','light','light_preferences','options','camera','privacy','restart','delete']);
const response=(status,value)=>Response.json(value,{status,headers:{'Cache-Control':'no-store'}});
const hex=bytes=>[...new Uint8Array(bytes)].map(x=>x.toString(16).padStart(2,'0')).join('');
export async function rpcFetch(room,request,claims){
 await room.expire();
 if(claims.iat<=(await room.ctx.storage.get('stopped')||0))return response(401,{error:'Camera authorization expired'});
 if(!room.pending)room.pending=new Map();
 if(room.pending.size>=3)return response(429,{error:'Camera is busy. Try again shortly.'});
 if(claims.role==='rpc'&&request.method!=='POST'||claims.role==='file'&&request.method!=='GET')return response(405,{error:'Method not allowed'});
 let command;
 if(claims.role==='file'){
  const u=new URL(request.url),offset=Number(u.searchParams.get('offset')||0);
  if(!Number.isInteger(offset)||offset<0||offset>=claims.file_size)return response(416,{error:'Invalid file offset'});
  command={id:crypto.randomUUID().replaceAll('-',''),action:'file_chunk',params:{name:claims.name,kind:claims.kind,offset,limit:8192}};
 }else{
  if(Number(request.headers.get('Content-Length')||0)>2048)return response(413,{error:'Command too large'});
  const reader=request.body?.getReader(),parts=[];let size=0;if(!reader)return response(400,{error:'Invalid command'});for(;;){const item=await reader.read();if(item.done)break;size+=item.value.byteLength;if(size>2048){await reader.cancel();return response(413,{error:'Command too large'})}parts.push(item.value)}const bytes=new Uint8Array(size);let at=0;for(const part of parts){bytes.set(part,at);at+=part.byteLength}const raw=new TextDecoder().decode(bytes);if(hex(await crypto.subtle.digest('SHA-256',bytes))!==claims.request_hash)return response(403,{error:'Command authorization mismatch'});
  try{command=JSON.parse(raw)}catch{return response(400,{error:'Invalid command'})}
 }
 if(!/^[a-f0-9]{32}$/.test(command.id)||!ACTIONS.has(command.action)||!command.params||typeof command.params!=='object'||Array.isArray(command.params))return response(400,{error:'Invalid command'});
 const mutates=!READ.has(command.action),key='c:'+command.id;
 const camera=room.sockets('camera')[0];
 if(!camera)return response(503,{error:'Camera is offline. Check its power and Wi-Fi.'});
 if(mutates){
  const claimed=await room.ctx.storage.transaction(async tx=>{const old=await tx.get(key);if(old)return {old};if((await tx.list({prefix:'c:',limit:65})).size>=64)return {full:true};await tx.put(key,{hash:claims.request_hash,status:'sent',until:Date.now()+300000});return {}});
  if(claimed.full)return response(429,{error:'Too many recent commands. Try again shortly.'});
  if(claimed.old){const old=claimed.old;if(old.hash!==claims.request_hash)return response(409,{error:'Command ID was already used for another operation'});return old.status==='done'?response(200,old.result):response(409,{error:'Command was already sent. Check camera status before trying again.',id:command.id,state:'unconfirmed'})}
 }
 if(room.pending.has(command.id)||room.pending.size>=3)return response(409,{id:command.id,state:'unconfirmed',error:'Camera is busy. Check its current state.'});
 return new Promise(resolve=>{
  const entry={id:command.id,action:command.action,offset:command.params.offset,size:claims.file_size,key,mutates,hash:claims.request_hash,resolve,timer:null};
  entry.timer=setTimeout(()=>{room.pending.delete(command.id);resolve(response(504,{id:command.id,state:'unconfirmed',error:'No device confirmation received. Check camera status before trying again.'}))},10000);
  room.pending.set(command.id,entry);
  if(!room.send(camera,{type:'command',...command})){clearTimeout(entry.timer);room.pending.delete(command.id);resolve(response(503,{id:command.id,error:'Camera connection interrupted'}))}
 });
}
export async function rpcMessage(room,ws,message){
 if(ws.deserializeAttachment()?.role!=='camera')return false;
 if(typeof message==='string'){
  if(message.length>8192){ws.close(1009,'Message too large');return true}
  let value;try{value=JSON.parse(message)}catch{return false}
  if(value.type!=='result')return false;
  const pending=room.pending?.get(value.id);if(!pending)return true;
  if(message.length>8192||typeof value.ok!=='boolean'){return true}
  clearTimeout(pending.timer);room.pending.delete(value.id);
  const result={id:value.id,ok:value.ok,...(value.ok?{data:value.data||{}}:{error:typeof value.error==='string'?value.error.slice(0,200):'Camera rejected the command'})};
  if(pending.mutates)try{await room.ctx.storage.put(pending.key,{hash:pending.hash,status:'done',result,until:Date.now()+300000})}catch{pending.resolve(response(503,{id:value.id,state:'unconfirmed',error:'Command confirmation could not be saved. Check camera status.'}));return true}
  pending.resolve(response(value.ok?200:409,result));return true;
 }
 if(message.byteLength<32)return false;
 const b=new Uint8Array(message),v=new DataView(message);if(String.fromCharCode(...b.slice(0,4))!=='RVF1')return false;
 const id=hex(b.slice(4,20)),pending=room.pending?.get(id);if(!pending||pending.action!=='file_chunk')return true;
 const offset=v.getUint32(20),total=v.getUint32(24),length=v.getUint16(28);
 if(message.byteLength!==32+length||length<1||length>8192||offset!==pending.offset||total!==pending.size||offset+length>total)return true;
 clearTimeout(pending.timer);room.pending.delete(id);pending.resolve(new Response(message.slice(32),{headers:{'Cache-Control':'no-store','Content-Type':'application/octet-stream','X-File-Size':String(total),'X-File-Offset':String(offset)}}));return true;
}
export function rpcDisconnected(room,ws){if(ws.deserializeAttachment()?.role!=='camera')return;for(const pending of room.pending?.values()||[]){clearTimeout(pending.timer);pending.resolve(response(503,{id:pending.id,state:'unconfirmed',error:'Camera connection interrupted'}))}room.pending?.clear()}
export async function rpcExpire(room){const rows=await room.ctx.storage.list({prefix:'c:',limit:128}),expired=[...rows].filter(([,v])=>v.until<Date.now()).map(([key])=>key);if(expired.length)await room.ctx.storage.delete(expired)}
