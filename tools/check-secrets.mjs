// Never print matching values. Exact local-secret comparisons complement credential-shape checks.
import fs from 'node:fs';import {execFileSync} from 'node:child_process';
const git=(...args)=>execFileSync('git',args,{encoding:'utf8',maxBuffer:100*1024*1024});
const placeholder=s=>!s||/^(?:SET_|YOUR_|GENERATE_|CHOOSE_|CREATE_|REPLACE_|vXX|https:\/\/YOUR|<|\$\{|\$\{\{)/.test(s);
let local=[];if(fs.existsSync('remote/.env.local'))for(const line of fs.readFileSync('remote/.env.local','utf8').split(/\r?\n/)){const at=line.indexOf('=');if(at<0)continue;const name=line.slice(0,at),value=line.slice(at+1).trim();if(/KEY|TOKEN|SECRET/.test(name)&&value.length>=16&&!placeholder(value))local.push(value)}
const patterns=[/\beyJ[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\.[A-Za-z0-9_-]{20,}\b/,/\b(?:sb_secret_|sb_publishable_|ghp_|github_pat_|EAAG)[A-Za-z0-9_]{20,}\b/,/-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----/];
const issues=[];function check(text,label){if(local.some(value=>text.includes(value))||patterns.some(p=>p.test(text)))issues.push(label);for(const line of text.split(/\r?\n/))if(/^(?:SUPABASE_SERVICE_ROLE_KEY|RECIPIENT_ENCRYPTION_KEY|META_ACCESS_TOKEN|META_APP_SECRET|CRON_SECRET)=/.test(line)&&!placeholder(line.slice(line.indexOf('=')+1)))issues.push(label+' (environment assignment)')}
const tracked=git('ls-files','-z').split('\0').filter(Boolean);for(const file of tracked){if(/(?:^|\/)\.env(?:$|\.)/.test(file)&&!file.endsWith('.env.example'))issues.push(file+' (tracked environment file)');if(fs.existsSync(file)&&fs.statSync(file).size<2000000)check(fs.readFileSync(file,'utf8'),file)}
for(const file of git('diff','--cached','--name-only','--diff-filter=ACMR','-z').split('\0').filter(Boolean))check(git('show',':'+file),'staged:'+file);
if(process.argv.includes('--history'))check(git('log','--all','-p','--format=commit %H','--no-ext-diff'),'Git history');
if(fs.existsSync('remote/.env.local')){try{git('check-ignore','--quiet','remote/.env.local')}catch{issues.push('remote/.env.local is not ignored')}}
if(issues.length){console.error('Credential check FAILED. Values withheld. Locations:',[...new Set(issues)].join(', '));process.exitCode=1}else console.log('PASS: tracked files, staged files'+(process.argv.includes('--history')?', Git history':'')+' and ignored local environment checked; no credential matches found.');
