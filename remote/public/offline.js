let checking=false;
async function reconnect(){
 if(checking)return;checking=true;const button=document.getElementById('retryOnline');button.disabled=true;
 try{for(let attempt=0;attempt<3;attempt++){
  try{const r=await fetch('/api/connection',{cache:'no-store',credentials:'omit',redirect:'error',signal:AbortSignal.timeout(4000)});const d=await r.json();if(r.ok&&d.service==='Robetix Vision'&&d.ready){location.reload();return}}catch{}
  if(attempt<2)await new Promise(r=>setTimeout(r,1000*(attempt+1)));
 }}finally{checking=false;button.disabled=false}
}
document.getElementById('retryOnline').onclick=reconnect;
// OS connectivity signals are only a hint. Verify the portal before navigating.
window.addEventListener('online',reconnect);
