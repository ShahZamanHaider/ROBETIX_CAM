// Browser checks use simulated device responses, never hardware measurements.
const fs=require('fs'),http=require('http'),assert=require('assert');
const {chromium}=require('../.pio/ui-test/node_modules/playwright');
const html=fs.readFileSync('data/index.html','utf8');
new Function(html.match(/<script>([\s\S]*?)<\/script>/)[1]);
const options={quality_mode:1,recording_mode:0,name:'Front Door',recording_quality:1,recording_fps:5,preview_fps:5,clip_seconds:60,reserve_mb:64,cleanup:false,privacy:false,motion:false,motion_photo:true,motion_record:false,sensitivity:1,min_motion_seconds:2,cooldown_seconds:30,motion_seconds:15};
const status={uptime:120,heap:170000,min_heap:150000,psram:4194304,free_psram:3400000,ip:'192.168.1.10',ap_ip:'192.168.4.1',connected:true,rssi:-50,reconnects:1,hotspot:true,needs_network_setup:false,ap_ssid:'My Camera',ssid:'Home Wi-Fi',benchmark:false,camera:{ready:true,pid:38,error:'',size:8,quality:18,brightness:0,contrast:0,saturation:0,mirror:0,flip:0,capture_ms:55,captured:10,failures:0},storage:{available:true,total:8*1024**3,used:1024**3,free:7*1024**3,error:''},app:{recording:false,phase:'idle',elapsed:0,frames:0,write_ms:0,record_errors:0,preview_drops:0,privacy:false,message:'',message_id:0,last_video:'',clock_set:true,options}};
let authenticated=false,setup=true,calls=[],frameId=0,stall=false,deleted=false;
const png=Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jY1sAAAAASUVORK5CYII=','base64');
function message(s){status.app.message=s;status.app.message_id++}
const server=http.createServer((req,res)=>{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname==='/'){res.setHeader('Content-Type','text/html');return res.end(html)}
 let input='';req.on('data',c=>input+=c);req.on('end',()=>{
  let body=input?JSON.parse(input):null;calls.push({url:url.pathname,body});res.setHeader('Content-Type','application/json');const send=d=>res.end(JSON.stringify(d));
  if(url.pathname==='/api/session')return send({setup,authenticated});
  if(url.pathname==='/api/login'){authenticated=true;setup=false;return send({})}
  if(!authenticated){res.statusCode=401;return send({error:'Sign in required'})}
  if(url.pathname==='/api/preview'){if(stall){res.statusCode=503;return send({error:'Waiting for frame'})}res.setHeader('Content-Type','image/png');res.setHeader('X-Frame-Id',String(++frameId));return res.end(png)}
  if(url.pathname==='/api/status')return send(status);
  if(url.pathname==='/api/time')return send({});
  if(url.pathname==='/api/benchmark')return send({running:false,results:[]});
  if(url.pathname==='/api/camera'){Object.assign(status.camera,body);message('Picture settings saved');return send({})}
  if(url.pathname==='/api/options'){Object.assign(options,body);message('Settings saved');return send({})}
  if(url.pathname==='/api/action'){if(body.action==='record'){status.app.recording=true;status.app.phase='recording';message('Recording started')}if(body.action==='stop'){status.app.phase='saving';setTimeout(()=>{status.app.recording=false;status.app.phase='idle';message('Video saved')},500)}if(body.action==='photo')message('Photo saved');if(body.action==='privacy'){status.app.privacy=body.enabled;message(body.enabled?'Camera paused':'Camera resumed')}return send({})}
  if(url.pathname==='/api/files')return send({files:deleted?[]:[{name:'rv_0000000001_0000000000.avi',kind:'videos',size:100000,duration:3,epoch:0}],next:0});
  if(url.pathname==='/api/delete'){deleted=true;return send({})}
  if(url.pathname==='/api/cleanup')return send({phase:'ready',count:2,bytes:2000,token:'0123456789abcdef0123456789abcdef',log:[]});
  if(url.pathname==='/api/events')return send({events:[]});
  if(url.pathname==='/api/scan')return send({running:false,networks:[{ssid:'<script>unsafe</script>',rssi:-55}]});
  if(url.pathname==='/api/logout'){authenticated=false;return send({})}
  if(url.pathname==='/api/network'){status.hotspot=body.hotspot;status.ap_ssid=body.ap_ssid;status.ssid=body.ssid;status.needs_network_setup=false;return send({})}
  res.statusCode=404;send({error:'Not found'});
 });
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const browser=await chromium.launch({channel:'msedge',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 try{
  await page.goto('http://127.0.0.1:'+server.address().port);await page.locator('#setupHelp').waitFor({state:'visible'});
  await page.locator('#password').fill('test-password-123');await page.locator('#loginForm button').click();
  await page.waitForFunction(()=>document.querySelector('#connectionText').textContent==='Connected');
  await page.locator('#feed').waitFor({state:'visible'});
  assert.equal(await page.locator('#feed').evaluate(e=>getComputedStyle(e).objectFit),'contain');
  await page.locator('#record').click();await page.waitForFunction(()=>document.querySelector('#recordLabel').textContent==='Stop & Save');
  assert(await page.locator('#recBadge').isVisible());await page.locator('#record').click();await page.waitForFunction(()=>document.querySelector('#recordLabel').textContent==='Record');
  await page.locator('#photo').click();assert(calls.some(c=>c.url==='/api/action'&&c.body.action==='photo'));
  fs.mkdirSync('.pio/ui-test/results',{recursive:true});await page.screenshot({path:'.pio/ui-test/results/home-desktop.png',fullPage:true});
  stall=true;await page.waitForFunction(()=>document.querySelector('#feed').hidden,{},{timeout:6000});
  assert((await page.locator('#previewMessage').textContent()).includes('Reconnecting'),'Stale frame must not remain labelled Live');stall=false;
  await page.locator('nav [data-page=saved]').click();await page.waitForFunction(()=>document.querySelector('#files').textContent.includes('Video 1'));
  assert((await page.locator('#files a').getAttribute('href')).includes(':81/file?kind=videos'));
  await page.locator('#files button').click();await page.waitForFunction(()=>document.querySelector('#files').textContent==='No videos saved yet.');
  await page.locator('nav [data-page=settings]').click();await page.waitForFunction(()=>document.querySelector('#name').value==='Front Door');await page.locator('#name').fill('Garden');await page.locator('#pictureForm button').click();
  await page.waitForFunction(()=>document.querySelector('#notice').textContent.includes('Applying picture'));assert(calls.some(c=>c.url==='/api/options'&&c.body.name==='Garden'));assert.equal(await page.locator('#settingsPreviewDock #feed').count(),1);
  await page.locator('#hotspot').selectOption('false');assert(await page.locator('#routerFields').isVisible());await page.locator('#scan').click();
  await page.waitForFunction(()=>document.querySelector('#networks').children.length===1);assert.equal(await page.locator('#networks script').count(),0);
  await page.setViewportSize({width:390,height:844});await page.locator('nav [data-page=home]').click();await page.locator('#feed').waitFor({state:'visible'});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'Mobile overflow');
  await page.screenshot({path:'.pio/ui-test/results/home-mobile.png',fullPage:true});
  assert(calls.some(c=>c.url==='/api/login'&&c.body.remember===true));
  await page.locator('#lightButton').click();await page.waitForTimeout(100);assert(calls.some(c=>c.body?.action==='light'&&c.body.brightness===10));
  await page.locator('#homeMotion').check();await page.waitForTimeout(100);assert(calls.some(c=>c.body?.motion===true));
  await page.locator('nav [data-page=settings]').click();await page.locator('#advancedButton').click();
  await page.locator('#storageAdvanced summary').click();await page.locator('#deleteFrom').fill('2026-10-01');await page.locator('#deleteTo').fill('2026-10-27');await page.locator('#findDates').click();await page.locator('#confirmDates').waitFor({state:'visible'});await page.waitForFunction(()=>!document.querySelector('#confirmDates').disabled);await page.locator('#confirmDates').click();await page.waitForTimeout(100);assert(calls.some(c=>c.url==='/api/cleanup'&&c.body?.action==='delete'&&c.body.confirm===true));
  await page.locator('nav [data-page=home]').click();await page.reload();await page.locator('#page-home').waitFor({state:'visible'});assert(await page.locator('#login').isHidden());
  status.storage.available=false;await page.waitForFunction(()=>document.querySelector('#photo').disabled);assert(await page.locator('#record').isDisabled());
  await page.locator('nav [data-page=settings]').click();await page.locator('#logout').click();await page.locator('#login').waitFor({state:'visible'});assert.deepEqual(errors,[]);
  console.log('PASS: login, fresh preview, stale-frame hiding, recording/stop state, photo action, file download/deletion, camera settings, AP/STA fields, safe SSIDs, mobile layout, missing card, remembered Home entry, flashlight request, motion toggle, confirmed date deletion and logout. Simulated APIs only.');
 }finally{await browser.close();server.close()}
})().catch(e=>{console.error(e);server.close();process.exitCode=1});

