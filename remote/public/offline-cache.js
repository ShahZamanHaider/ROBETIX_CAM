// Only public recovery-shell assets enter this cache. No API, camera image,
// account page, callback URL, query string or file download is ever cached.
const shellCache='rv-offline-shell-7226e7d974eab307';
const shellAssets=['/offline.html','/offline.js','/app.css','/design-tokens.css','/i18n.js','/app-icon.svg'];
self.addEventListener('install',event=>event.waitUntil(caches.open(shellCache).then(cache=>cache.addAll(shellAssets))));
self.addEventListener('activate',event=>event.waitUntil(caches.keys().then(keys=>Promise.all(keys.filter(k=>k.startsWith('rv-offline-shell-')&&k!==shellCache).map(k=>caches.delete(k))))));
self.addEventListener('fetch',event=>{
 const u=new URL(event.request.url);if(u.origin!==self.location.origin||event.request.method!=='GET'||u.search)return;
 if(event.request.mode==='navigate'&&u.pathname==='/'){
  event.respondWith(fetch(event.request).catch(async()=>await caches.match('/offline.html')||new Response('Reconnect to open Robetix Vision',{status:503})));return;
 }
 if(shellAssets.includes(u.pathname))event.respondWith(fetch(event.request).catch(()=>caches.match(u.pathname)));
});
