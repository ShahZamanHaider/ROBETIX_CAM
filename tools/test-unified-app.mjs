import fs from 'node:fs';import http from 'node:http';import assert from 'node:assert/strict';import {createRequire} from 'node:module';
const require=createRequire(import.meta.url),{chromium}=require('../.pio/ui-test/node_modules/playwright');
let removed=false,release,removes=0;
const ids=Array.from({length:5},(_,i)=>`11111111-1111-4111-8111-${String(i+1).padStart(12,'0')}`);
const server=http.createServer(async(req,res)=>{
 const u=new URL(req.url,'http://localhost'),json=(v,status=200)=>{res.writeHead(status,{'Content-Type':'application/json'});res.end(JSON.stringify(v))};
 if(u.pathname==='/api/auth')return require('./mock-auth.cjs').mockAuth(req,res,{get:()=>true,set:()=>{}});
 if(u.pathname==='/api/connection')return json({service:'Robetix Vision',ready:true});
 if(u.pathname==='/api/account'){
  if(u.searchParams.get('action')==='remove'){removes++;await new Promise(r=>release=r);removed=true;return json({removed:true,live_stopped:true})}
  return json({capabilities:{live:true,setup:true},devices:ids.filter((_,i)=>!(removed&&i===0)).map((id,i)=>({id,name:i?'Camera '+i:'Front Door',enabled:true,remote:true,online:false,control_protocol:2}))});
 }
 if(u.pathname==='/api/live')return json({relay:'wss://relay.test/camera/'+ids[0],ticket:'fixture'});
 if(u.pathname==='/api/remote')return json({error:'Camera offline'},503);
 const name=u.pathname==='/'?'index.html':u.pathname.slice(1);if(!/^[a-z0-9.-]+$/.test(name)||!fs.existsSync('remote/public/'+name)){res.statusCode=404;return res.end()}
 res.setHeader('Content-Type',name.endsWith('.js')?'text/javascript':name.endsWith('.css')?'text/css':name.endsWith('.svg')?'image/svg+xml':name.endsWith('.webmanifest')?'application/manifest+json':'text/html');res.end(fs.readFileSync('remote/public/'+name));
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));const browser=await chromium.launch({headless:true,channel:process.env.ROBETIX_BROWSER_CHANNEL||'msedge'});
try{
 const context=await browser.newContext({viewport:{width:390,height:844}}),page=await context.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.routeWebSocket('wss://relay.test/**',()=>{});await page.goto('http://127.0.0.1:'+server.address().port);
 await page.waitForFunction(()=>document.querySelectorAll('#devices article').length===5);
 await page.locator('#devices article').first().getByRole('button',{name:'Camera Settings',exact:true}).click();
 await page.locator('#remoteRemove').click();await page.locator('#i18nDialogTitle').waitFor();assert(await page.locator('dialog').innerText().then(s=>s.includes('Front Door')));
 await page.locator('#i18nCancel').click();assert.equal(removes,0);
 await page.locator('#remoteRemove').click();await page.locator('#i18nConfirm').click();await page.waitForFunction(()=>document.querySelector('dialog').open===false);
 while(!release)await new Promise(r=>setTimeout(r,10));assert(!await page.locator('#cameraDashboard').isHidden(),'wait for server confirmation');release();
 await page.waitForFunction(()=>document.querySelectorAll('#devices article').length===4);assert(await page.locator('#cameraDashboard').isHidden());assert.equal(removes,1);
 await page.locator('#language').selectOption('ur');await page.waitForFunction(()=>document.documentElement.dir==='rtl');assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));assert.deepEqual(await page.evaluate(()=>[...I18n.missing]),[]);
 fs.mkdirSync('.pio/ui-test/results',{recursive:true});await page.screenshot({path:'.pio/ui-test/results/unified-cameras-urdu.png',fullPage:true});
 await page.evaluate(()=>navigator.serviceWorker.ready);await page.waitForFunction(()=>navigator.serviceWorker.controller!==null);
 const paths=await page.evaluate(async()=>{const key=(await caches.keys()).find(k=>k.startsWith('rv-offline-shell-'));const c=await caches.open(key);return (await c.keys()).map(r=>new URL(r.url).pathname)});
 assert(paths.includes('/offline.html'));assert(!paths.some(p=>p==='/'||p.startsWith('/api/')||p.includes('download')));
 await context.setOffline(true);await page.reload({waitUntil:'domcontentloaded'});await page.locator('#retryOnline').waitFor();await page.waitForFunction(()=>document.documentElement.dir==='rtl');assert.equal(await page.locator('html').getAttribute('dir'),'rtl');assert.equal(await page.locator('#liveImage').count(),0);
 assert.equal(await page.locator('a').getAttribute('href'),'http://192.168.4.1/#stay-local');assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 await page.screenshot({path:'.pio/ui-test/results/unified-offline-urdu.png',fullPage:true});
 await context.setOffline(false);await page.waitForFunction(()=>document.querySelectorAll('#devices article').length===4);assert.deepEqual(errors,[]);
 console.log('PASS: five-camera overview, camera Settings removal, cancel and actual-ACK gating, Urdu RTL, private-data-free offline shell, local bootstrap and online recovery. Device/cloud responses simulated.');
}finally{await browser.close();server.close()}
