import {fail} from './service.mjs';
export const reads=new Set(['status','files','events','file_stat']);
export function validFile(name,kind){return typeof name==='string'&&(['photos','videos'].includes(kind)&&new RegExp('^rv_[0-9]{10}_[0-9]{10}\\.'+(kind==='videos'?'avi':'jpg')+'$').test(name)||kind==='legacy'&&/^[a-f0-9]{16}\.jpg$/i.test(name))}
const options={quality_mode:[0,2],recording_mode:[0,2],recording_quality:[0,2],recording_fps:[1,10],preview_fps:[1,10],clip_seconds:[5,300],reserve_mb:[16,1024],sensitivity:[0,2],motion_seconds:[5,60],min_motion_seconds:[1,5],cooldown_seconds:[1,600],motion:'bool',motion_photo:'bool',motion_record:'bool',smaller_movements:'bool',cleanup:'bool'};
const camera={size:[5,13],quality:[10,40],brightness:[-2,2],contrast:[-2,2],saturation:[-2,2],mirror:[0,1],flip:[0,1],white_balance:[0,4],exposure:[-1,1200],gain:[-1,30]};
function fields(value,spec){for(const [key,v]of Object.entries(value)){const rule=spec[key];if(!rule)fail(400,'Unsupported camera setting');if(rule==='bool'?typeof v!=='boolean':!Number.isInteger(v)||v<rule[0]||v>rule[1])fail(400,'Invalid camera setting')}return value}
export function command(action,params={}){
 if(!params||typeof params!=='object'||Array.isArray(params))fail(400,'Invalid command');
 if(['status','events','record','stop','photo'].includes(action)){if(Object.keys(params).length)fail(400,'Invalid command');return params}
 if(action==='restart'){if(params.confirm!==true||Object.keys(params).some(k=>k!=='confirm'))fail(400,'Confirm camera restart');return {}}
 if(action==='privacy'&&typeof params.enabled!=='boolean')fail(400,'Invalid camera setting');
 if(action==='light'&&!Number.isInteger(params.brightness))fail(400,'Invalid camera setting');
 if(action==='light_preferences'&&(!Number.isInteger(params.preferred)||!Number.isInteger(params.auto_off)))fail(400,'Invalid camera setting');
 if(action==='privacy')return fields(params,{enabled:'bool'});
 if(action==='light')return fields(params,{brightness:[0,100]});
 if(action==='light_preferences')return fields(params,{preferred:[0,100],auto_off:[10,300]});
 if(action==='options')return fields(params,options);
 if(action==='camera')return fields(params,camera);
 if(action==='files'){if(!['photos','videos','legacy'].includes(params.kind)||Object.keys(params).some(k=>!['kind','before'].includes(k))||params.before!==undefined&&(!Number.isInteger(params.before)||params.before<1||params.before>4294967295))fail(400,'Choose photos or videos');return params}
 if(['file_stat','download','delete'].includes(action)){if(!validFile(params.name,params.kind)||Object.keys(params).some(k=>!['name','kind','confirm'].includes(k))||action==='delete'&&params.confirm!==true)fail(400,'Invalid file request');return {name:params.name,kind:params.kind,...(action==='delete'?{confirm:true}:{})}}
 fail(400,'Unsupported remote command');
}
