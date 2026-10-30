// Connectivity assistance never redirects, replays a command, or caches account data.
(()=>{
 const notice=document.getElementById('connectionNotice'),label=document.getElementById('connectionLabel');
 let pending=false,failures=0,installPrompt;
 async function check(){
  if(pending||document.hidden)return;pending=true;
  try{
   const r=await fetch('/api/connection',{cache:'no-store',credentials:'omit',redirect:'error',signal:AbortSignal.timeout(4500)});
   const d=await r.json();if(!r.ok||d.service!=='Robetix Vision'||!d.ready)throw Error();
   failures=0;notice.hidden=true;
  }catch{failures++;if(!navigator.onLine||failures>=2){notice.hidden=false;label.textContent=navigator.onLine?'Reconnecting':'Offline';globalThis.I18n?.apply(globalThis.I18n.language)}}finally{pending=false}
 }
 window.addEventListener('offline',()=>{notice.hidden=false;label.textContent='Offline';globalThis.I18n?.apply(globalThis.I18n.language)});
 window.addEventListener('online',check);document.addEventListener('visibilitychange',check);
 document.getElementById('retryConnection').onclick=check;
 // Reuse the secure download worker: competing workers at / would break transfers.
 if('serviceWorker'in navigator)navigator.serviceWorker.register('/download-worker.js',{scope:'/'}).catch(()=>{});
 window.addEventListener('beforeinstallprompt',e=>{e.preventDefault();installPrompt=e;document.getElementById('installApp').hidden=false});
 document.getElementById('installApp').onclick=async()=>{if(!installPrompt)return;await installPrompt.prompt();installPrompt=null;document.getElementById('installApp').hidden=true};
 setInterval(check,20000);check();
})();
