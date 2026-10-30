(()=>{'use strict';
const SITE='https://vision.robetix.com';
// No credentials or camera identifiers are sent by this public reachability check.
async function probeCloud(fetcher=fetch){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),2500);try{const r=await fetcher(SITE+'/api/connection',{mode:'cors',credentials:'omit',cache:'no-store',redirect:'error',referrerPolicy:'no-referrer',signal:controller.signal});if(!r.ok)return false;const d=await r.json();return d.service==='Robetix Vision'&&d.protocol===1&&d.environment==='production'&&d.customer_origin===SITE&&d.ready===true}catch{return false}finally{clearTimeout(timer)}}
class CloudSwitch{
 constructor({read,probe=probeCloud,wait=ms=>new Promise(r=>setTimeout(r,ms)),navigate,storage,validate=async()=>true,notify=()=>{}}){Object.assign(this,{read,probe,wait,navigate,storage,validate,notify});this.busy=false;this.cancelled=false;this.next=0;}
 stay(){this.cancelled=true;try{this.storage?.setItem('rv-stay-local','1')}catch{}this.notify(false);}
 eligible(){const s=this.read(),c=s.cloud,a=s.device?.app;let stay=false;try{stay=this.storage?.getItem('rv-stay-local')==='1'}catch{}
  return !stay&&!this.cancelled&&s.hotspot&&s.visible&&s.home&&s.online&&!s.interacted&&!s.busy&&s.device?.setup_complete===true&&!s.device?.benchmark&&!a?.privacy&&!a?.recording&&a?.phase==='idle'&&c?.paired===true&&c.remote===true&&c.settings_saved===true&&c.service_matches_build===true&&c.remote_service_ready===true&&c.state==='connected'&&c.url===SITE&&/^[a-f0-9-]{36}$/i.test(c.device_id||'')&&Number.isFinite(c.last_success_uptime)&&c.last_success_uptime>0&&s.device.uptime>=c.last_success_uptime&&s.device.uptime-c.last_success_uptime<90;
 }
 async consider(now=Date.now()){if(this.busy||now<this.next||!this.eligible())return false;this.busy=true;this.next=now+30000;try{if(!await this.probe()||!this.eligible())return false;this.notify(true);await this.wait(5000);if(!this.eligible()||!await this.probe()||!await this.validate()||!this.eligible())return false;const id=this.read().cloud.device_id;this.stay();this.navigate(SITE+'/#camera='+encodeURIComponent(id));return true}finally{this.busy=false;this.notify(false)}}
}
globalThis.CloudSwitch=CloudSwitch;globalThis.probeRobetixCloud=probeCloud;
})();
