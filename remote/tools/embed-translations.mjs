// Remote-only generation; does not touch ESP32 sources or require a firmware upload.
import fs from 'node:fs';import {fileURLToPath} from 'node:url';import path from 'node:path';
import {createHash} from 'node:crypto';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const dictionary={...JSON.parse(fs.readFileSync(path.join(root,'data/ur.json'),'utf8')),...JSON.parse(fs.readFileSync(path.join(root,'remote/public/auth-ur.json'),'utf8'))};
const source=fs.readFileSync(path.join(root,'data/i18n.js'),'utf8').replace('/* @translations */',JSON.stringify(dictionary));fs.writeFileSync(path.join(root,'remote/public/i18n.js'),source);

const origins=JSON.parse(fs.readFileSync(path.join(root,'remote/service-origins.json'),'utf8'));const configFile=path.join(root,'remote/vercel.json'),config=JSON.parse(fs.readFileSync(configFile,'utf8'));config.redirects=[{source:'/:path*',has:[{type:'host',value:new URL(origins.legacy).host}],destination:origins.customer+'/:path*',permanent:true}];fs.writeFileSync(configFile,JSON.stringify(config,null,2)+'\n');

for(const asset of ['alarm.js','design-tokens.css'])fs.writeFileSync(path.join(root,'remote/public',asset),fs.readFileSync(path.join(root,'data',asset)));const local=fs.readFileSync(path.join(root,'data/index.html'),'utf8');const icons=local.match(/<svg class="sprite"[\s\S]*?<\/svg>/);if(icons)fs.writeFileSync(path.join(root,'remote/public/icons.svg'),icons[0].replace(' class="sprite"','').replace('</svg>','<symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l2 2m10 10l2 2M5 19l2-2M17 7l2-2"/></symbol></svg>'));

// A changed shell asset must update the worker, not leave an installed offline
// page permanently pinned. Canonical line endings make Windows/CI agree.
const workerPath=path.join(root,'remote/public/offline-cache.js');let worker=fs.readFileSync(workerPath,'utf8');
const shell=JSON.parse(worker.match(/const shellAssets=(\[[^;]+\]);/)[1].replaceAll("'",'"'));
const digest=createHash('sha256');for(const name of shell)digest.update(name+'\n'+fs.readFileSync(path.join(root,'remote/public',name.slice(1)),'utf8').replace(/\r\n/g,'\n'));
worker=worker.replace(/const shellCache='[^']+';/,"const shellCache='rv-offline-shell-"+digest.digest('hex').slice(0,16)+"';");fs.writeFileSync(workerPath,worker);
