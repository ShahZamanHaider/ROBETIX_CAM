import {DurableObject} from 'cloudflare:workers';
import {rpcFetch,rpcMessage,rpcDisconnected,rpcExpire} from './rpc.js';
const LIMIT=131072,HEADER=24;
export class CameraRoom extends DurableObject {
 constructor(ctx,env){super(ctx,env);this.ctx=ctx;ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('ping','pong'));}
 sockets(role){return this.ctx.getWebSockets().filter(s=>s.readyState===1&&s.deserializeAttachment()?.role===role)}
 send(ws,value){try{ws.send(typeof value==='string'||value instanceof ArrayBuffer||ArrayBuffer.isView(value)?value:JSON.stringify(value));return true}catch{try{ws.close(1011,'Connection closed')}catch{}return false}}
 demand(){const viewers=this.sockets('viewer'),quality=Math.max(0,...viewers.map(s=>s.deserializeAttachment().quality));for(const camera of this.sockets('camera')){const a=camera.deserializeAttachment();a.quality=quality;camera.serializeAttachment(a);this.send(camera,{type:'demand',run:!!viewers.length,quality});}}
 async fetch(request){const c=JSON.parse(request.headers.get('X-Robetix-Claims'));if(c.role==='rpc'||c.role==='file')return rpcFetch(this,request,c);await this.expire();const stopped=await this.ctx.storage.get('stopped')||0;if(c.iat<=stopped&&c.role!=='stop')return new Response('Authorization expired',{status:401});const used=await this.ctx.storage.transaction(async tx=>{if(await tx.get('n:'+c.jti))return true;await tx.put('n:'+c.jti,c.exp);return false});if(used)return new Response('Ticket already used',{status:401});
  if(c.role==='stop'){await this.ctx.storage.put('stopped',Math.floor(Date.now()/1000));for(const s of this.ctx.getWebSockets())s.close(1008,'Camera authorization revoked');await this.nextAlarm();return Response.json({stopped:true},{headers:{'Cache-Control':'no-store'}})}
  if(c.role==='viewer'&&this.sockets('viewer').length>=2)return new Response('This camera already has two viewers',{status:429});if(c.role==='camera')for(const old of this.sockets('camera'))old.close(1012,'Camera reconnected');
  const pair=new WebSocketPair(),[client,server]=Object.values(pair);this.ctx.acceptWebSocket(server);server.serializeAttachment({...c,awaiting:false,seq:0,last:Date.now()});this.send(server,{type:'ready',server_ms:Date.now(),camera:!!this.sockets('camera').length,expires:c.exp});if(c.role==='viewer'&&!this.sockets('camera').length)this.send(server,{type:'status',state:'waiting_camera'});this.demand();await this.nextAlarm();return new Response(null,{status:101,webSocket:client,headers:{'Sec-WebSocket-Protocol':'robetix.live.v1','Cache-Control':'no-store'}});
 }
 async webSocketMessage(ws,message){const a=ws.deserializeAttachment();if(!a||a.exp*1000<=Date.now()){ws.close(1008,'Authorization expired');return}if(await rpcMessage(this,ws,message))return;if(typeof message==='string'){
  if(message.length>512){ws.close(1009,'Message too large');return}let v;try{v=JSON.parse(message)}catch{ws.close(1008,'Invalid message');return}
  if(a.role==='viewer'&&v.type==='ack'&&v.seq===a.seq){a.awaiting=false;a.last=Date.now();ws.serializeAttachment(a);return}
  if(a.role==='viewer'&&v.type==='quality'&&Number.isInteger(v.value)&&v.value>=0&&v.value<=2){a.quality=v.value;ws.serializeAttachment(a);this.demand();return}
  if(a.role==='camera'&&v.type==='status'&&['memory_limited','recording_priority','camera_paused','frame_too_large','clock_needed'].includes(v.state)){for(const viewer of this.sockets('viewer'))this.send(viewer,v);return}ws.close(1008,'Message not allowed');return;
 }
 if(a.role!=='camera'||message.byteLength<HEADER+4||message.byteLength>HEADER+LIMIT){ws.close(1008,'Invalid camera frame');return}
 const b=new Uint8Array(message),dv=new DataView(message),now=Date.now(),captured=Number(dv.getBigUint64(16));if(String.fromCharCode(...b.slice(0,4))!=='RVJ1'||dv.getUint32(12)!==b.length-HEADER||dv.getUint16(8)<1||dv.getUint16(8)>1600||dv.getUint16(10)<1||dv.getUint16(10)>1200||b[24]!==255||b[25]!==216||b.at(-2)!==255||b.at(-1)!==217||captured<now-5000||captured>now+2000){this.send(ws,{type:'drop',reason:'stale_or_invalid'});return}
 this.send(ws,{type:'frame_ack',seq:dv.getUint32(4),viewers:this.sockets('viewer').length});
 // Enforce the selected ceiling independently of an authorized but faulty sender.
 const fps=a.quality===0?1:2;if(now-a.last<1000/fps-20)return;a.last=now;ws.serializeAttachment(a);const seq=dv.getUint32(4);
 for(const viewer of this.sockets('viewer')){const state=viewer.deserializeAttachment();if(state.exp*1000<=now||state.awaiting)continue;state.awaiting=true;state.seq=seq;state.last=now;viewer.serializeAttachment(state);this.send(viewer,message)}await this.nextAlarm();
 }
 async webSocketClose(ws,code,reason,clean){rpcDisconnected(this,ws);try{ws.close(code===1005?1000:code,'Closed')}catch{}this.demand();for(const viewer of this.sockets('viewer'))if(!this.sockets('camera').length)this.send(viewer,{type:'status',state:'waiting_camera'});await this.nextAlarm();}
 async webSocketError(ws){rpcDisconnected(this,ws);try{ws.close(1011,'Connection interrupted')}catch{}this.demand();}
 async expire(){await rpcExpire(this);const now=Date.now();for(const ws of this.ctx.getWebSockets()){const a=ws.deserializeAttachment();if(!a||a.exp*1000<=now||a.role==='viewer'&&a.awaiting&&now-a.last>8000)try{ws.close(1008,'Session ended')}catch{}}const nonces=await this.ctx.storage.list({prefix:'n:',limit:128});const expired=[...nonces].filter(([,until])=>until*1000<now).map(([key])=>key);if(expired.length)await this.ctx.storage.delete(expired);}
 async nextAlarm(){const sockets=this.ctx.getWebSockets().filter(s=>s.readyState===1),deadline=sockets.length?Math.min(...sockets.map(s=>{const a=s.deserializeAttachment();return Math.min(a.exp*1000,a.role==='viewer'&&a.awaiting?a.last+8100:Infinity)})):Date.now()+95000;const current=await this.ctx.storage.getAlarm();if(!current||current>deadline)await this.ctx.storage.setAlarm(Math.max(Date.now()+50,deadline))}
 async alarm(){await this.expire();this.demand();if(this.ctx.getWebSockets().some(s=>s.readyState===1)||(await this.ctx.storage.list({prefix:'n:',limit:1})).size)await this.nextAlarm();}
}
