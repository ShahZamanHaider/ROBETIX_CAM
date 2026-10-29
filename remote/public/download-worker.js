// A one-use same-origin download path streams card chunks into the browser's
// download manager. Neither camera files nor relay credentials are cached.
const downloads=new Map(),active=new Map();
self.addEventListener('install',()=>self.skipWaiting());
self.addEventListener('activate',event=>event.waitUntil(self.clients.claim()));
self.addEventListener('message',event=>{
 if(!event.source?.url||new URL(event.source.url).origin!==self.location.origin)return;
 if(event.data?.type==='cancel-downloads'){for(const list of [downloads,active])for(const [key,value]of list)if(value.client===event.source.id){value.cancelled=true;list.delete(key)}return}
 if(event.data?.type!=='download'||!event.ports[0])return;
 for(const [key,v]of downloads)if(v.until<Date.now())downloads.delete(key);
 const v=event.data.value;if(!v||!/^rv_[0-9]{10}_[0-9]{10}\.(avi|jpg)$/.test(v.name)||!Number.isInteger(v.size)||v.size<1||downloads.size+active.size>=2){event.ports[0].postMessage({error:'Download is unavailable'});return}
 const id=crypto.randomUUID().replaceAll('-','');downloads.set(id,{...v,client:event.source.id,until:Date.now()+60000,cancelled:false});
 event.ports[0].postMessage({path:'/__camera_download/'+id+'/'+v.name});
});
const id=()=>crypto.randomUUID().replaceAll('-','');
async function renew(value){
 const r=await fetch('/api/auth?action=session',{credentials:'same-origin',cache:'no-store'}),s=await r.json();if(!r.ok||!s.authenticated)throw Error('Sign in again');
 const next=await fetch('/api/remote',{method:'POST',credentials:'same-origin',cache:'no-store',headers:{'Content-Type':'application/json','X-CSRF-Token':s.csrf},body:JSON.stringify({device:value.device,id:id(),action:'download',params:{kind:value.kind,name:value.name}})}),proof=await next.json();if(!next.ok)throw Error(proof.error||'File transfer failed');if(proof.name!==value.name||proof.size!==value.size)throw Error('The saved file changed');Object.assign(value,proof);
}
async function notify(value,message){const client=await self.clients.get(value.client);client?.postMessage({type:'download-status',name:value.name,...message})}
self.addEventListener('fetch',event=>{
 const url=new URL(event.request.url),match=/^\/__camera_download\/([a-f0-9]{32})\/([^/]+)$/.exec(url.pathname);if(url.origin!==self.location.origin||!match)return;
 const value=downloads.get(match[1]);downloads.delete(match[1]);if(!value||value.until<Date.now()||value.name!==match[2]){event.respondWith(new Response('Download link expired',{status:403}));return}
 active.set(match[1],value);let offset=0;const stream=new ReadableStream({async pull(controller){
  try{if(value.cancelled)throw Error('Download cancelled');if(value.expires*1000<Date.now()+20000)await renew(value);let response;
   for(let attempt=0;attempt<4;attempt++){response=await fetch(value.relay+'?offset='+offset,{headers:{Authorization:'Bearer '+value.ticket},credentials:'omit',cache:'no-store',referrerPolicy:'no-referrer',signal:AbortSignal.timeout(13000)});if(![503,504].includes(response.status))break;await new Promise(r=>setTimeout(r,1000))}
   if(!response.ok){let message='File transfer failed';try{message=(await response.json()).error||message}catch{}throw Error(message)}
   const bytes=new Uint8Array(await response.arrayBuffer());if(!bytes.length||bytes.length>8192||Number(response.headers.get('X-File-Size'))!==value.size||Number(response.headers.get('X-File-Offset'))!==offset||offset+bytes.length>value.size)throw Error('Invalid file transfer response');
   if(value.cancelled)throw Error('Download cancelled');controller.enqueue(bytes);offset+=bytes.length;notify(value,{received:offset,total:value.size});if(offset===value.size){active.delete(match[1]);controller.close();notify(value,{complete:true,received:offset,total:value.size})}
  }catch(error){active.delete(match[1]);controller.error(error);notify(value,{error:error.message})}
 },cancel(){value.cancelled=true;active.delete(match[1])}});
 event.respondWith(new Response(stream,{headers:{'Content-Type':'application/octet-stream','Content-Disposition':'attachment; filename="'+value.name+'"','Content-Length':String(value.size),'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}}));
});
