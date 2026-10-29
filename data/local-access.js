(()=>{'use strict';
class LocalAccessSession{
 constructor(request,{sleep=ms=>new Promise(r=>setTimeout(r,ms)),now=()=>Date.now(),state=()=>{}}={}){this.request=request;this.sleep=sleep;this.now=now;this.state=state;this.pending=null;this.session=null;}
 connect({attempts=3,budget=12000}={}){if(this.pending)return this.pending;this.pending=this.run(attempts,budget).finally(()=>{this.pending=null});return this.pending;}
 async run(attempts,budget){const until=this.now()+budget;let error;const call=(path,data)=>{const remaining=until-this.now();if(remaining<=0)throw Object.assign(Error('Cannot reach the camera. Check your Wi-Fi connection and try again.'),{retryable:true});return this.request(path,data,Math.min(3000,remaining))};
  for(let attempt=0;attempt<attempts;attempt++){try{this.state({attempt,phase:'connecting'});let s=await call('session');this.session=s;if(s.authenticated)return s;
   if(s.camera_wifi===false){if(s.interface==='hotspot')throw Object.assign(Error('The camera has not confirmed this Wi-Fi connection yet. Reconnect to camera Wi-Fi and try again.'),{retryable:true});throw Object.assign(Error('Connect to the camera Wi-Fi to authorize this browser.'),{status:403})}
   await call('hotspot-session',{});s=await call('session');this.session=s;if(!s.authenticated)throw Object.assign(Error(s.local_protocol&&s.local_protocol<2?'Camera software needs updating. Please contact your installer.':'Your browser did not remember camera access. Allow cookies for this camera and try again.'),{status:401});return s;
  }catch(e){error=e;if(!e.retryable||attempt+1>=attempts||this.now()+300>=until)break;await this.sleep(attempt===0?300:700)}}
  throw error||Error('Please try again');
 }
}
globalThis.LocalAccessSession=LocalAccessSession;
})();
