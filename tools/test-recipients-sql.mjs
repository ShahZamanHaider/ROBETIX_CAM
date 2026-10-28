// Real PostgreSQL WASM execution of schema/RPCs. No production database is contacted.
// Install the isolated test tool: npm install --prefix .pio/sql-test @electric-sql/pglite
import {PGlite} from '../.pio/sql-test/node_modules/@electric-sql/pglite/dist/index.js';
import fs from 'node:fs';import assert from 'node:assert/strict';
import {protectPhone} from '../remote/lib/recipients.mjs';
process.env.RECIPIENT_ENCRYPTION_KEY='34'.repeat(32);
const sql=new PGlite(),owner='aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa',device='11111111-1111-4111-8111-111111111111',other='99999999-9999-4999-8999-999999999999';
const call=async(name,args)=>{const placeholders=args.map((_,i)=>'$'+(i+1)).join(',');return (await sql.query('select * from '+name+'('+placeholders+')',args)).rows};
const scalar=async(name,args)=>Object.values((await call(name,args))[0])[0];
try{
 await sql.exec("create role anon;create role authenticated;create role service_role bypassrls;create schema auth;create table auth.users(id uuid primary key);create schema storage;create table storage.buckets(id text primary key,name text,public boolean,file_size_limit bigint,allowed_mime_types text[]);");
 // pgcrypto extension installation is omitted; gen_random_uuid is built into this PostgreSQL.
 await sql.exec(fs.readFileSync('remote/schema.sql','utf8').replace('create extension if not exists pgcrypto;',''));
 const migration=fs.readFileSync('remote/migrations/002_recipients.sql','utf8');await sql.exec(migration);await sql.exec(migration);
 await sql.query('insert into auth.users values($1)',[owner]);await sql.query("insert into rv_devices(id,owner,name,token_hash,config) values($1,$2,'Front Door','token',$3),($4,$2,'Other','other',$3)",[device,owner,{whatsapp:true,photo:true,interval:60,opt_in:true,phone:'923001234567'},other]);
 const encrypted=protectPhone('923001234567');await call('rv_initialize_recipients',[device,encrypted]);let rows=(await sql.query('select * from rv_recipients')).rows;assert.equal(rows.length,1);assert(rows[0].consent_at);assert.equal((await sql.query('select config from rv_devices where id=$1',[device])).rows[0].config.phone,undefined);
 const first=rows[0].id,base={name:'Family',...encrypted,enabled:true,consent:true,motion:true,photo:true,interval:60};assert.equal(await scalar('rv_save_recipient',[device,null,base]),'duplicate');assert.equal(await scalar('rv_save_recipient',[other,first,base]),'not_found');
 for(let n=1;n<=4;n++)assert.equal(await scalar('rv_save_recipient',[device,null,{...base,...protectPhone('92300123456'+(n+1)),consent:n!==3,enabled:n!==4}]),'saved');assert.equal(await scalar('rv_save_recipient',[device,null,{...base,...protectPhone('923009999999')}]),'limit');
 await scalar('rv_save_recipient',[device,first,{...base,consent:false}]);assert.equal((await sql.query('select consent_at from rv_recipients where id=$1',[first])).rows[0].consent_at,null);await scalar('rv_save_recipient',[device,first,base]);
 const event=(await sql.query("insert into rv_events(device,event_key,notification) values($1,'event_1','uploading') returning id",[device])).rows[0].id;
 assert.equal(await scalar('rv_queue',[device,event,false]),'queued');assert.equal(await scalar('rv_lease',[event]),true);assert.equal(await scalar('rv_lease',[event]),false);rows=await call('rv_prepare_deliveries',[event,null]);assert.equal(rows.length,5);assert.equal(rows.filter(x=>x.state==='queued').length,3);
 const queued=rows.filter(x=>x.state==='queued');assert(await scalar('rv_delivery_lease',[queued[0].id]));assert.equal(await scalar('rv_delivery_lease',[queued[0].id]),false,'Only the first lease may submit');
 await sql.query('update rv_devices set whatsapp_paused=true where id=$1',[device]);assert.equal(await scalar('rv_delivery_lease',[queued[1].id]),false);assert.equal((await sql.query('select state from rv_deliveries where id=$1',[queued[1].id])).rows[0].state,'disabled');await sql.query('update rv_devices set whatsapp_paused=false where id=$1',[device]);
 const remaining=queued[2];await sql.query('update rv_recipients set version=version+1 where id=$1',[remaining.recipient]);assert.equal(await scalar('rv_delivery_lease',[remaining.id]),false,'Edits invalidate pending delivery snapshots');
 const second=(await sql.query("insert into rv_events(device,event_key,notification) values($1,'event_2','uploading') returning id",[device])).rows[0].id;assert.equal(await scalar('rv_queue',[device,second,false]),'rate_limited');
 const third=(await sql.query("insert into rv_events(device,event_key,notification,is_test) values($1,'test_1','queued',true) returning id",[device])).rows[0].id;rows=await call('rv_prepare_deliveries',[third,first]);assert.equal(rows.length,1);assert.equal(rows[0].state,'queued','Explicit tests bypass motion cooldown but keep consent gates');
 await sql.query('delete from rv_recipients where device=$1',[device]);await call('rv_initialize_recipients',[device,encrypted]);assert.equal((await sql.query('select count(*)::int n from rv_recipients where device=$1',[device])).rows[0].n,0);assert.equal((await sql.query('select count(*)::int n from rv_deliveries where recipient is not null')).rows[0].n,0);
 for(const role of ['anon','authenticated']){await sql.exec('set role '+role);await assert.rejects(sql.query('select * from rv_recipients'),/permission denied/);await assert.rejects(call('rv_initialize_recipients',[device,null]),/permission denied/);await assert.rejects(sql.query('select * from rv_deliveries'),/permission denied/);await sql.exec('reset role')}
 await sql.exec('set role service_role');await sql.query('select * from rv_recipients');await sql.exec('reset role');
 console.log('PASS: actual PostgreSQL schema/migration twice, encrypted legacy import once, duplicate/limit/ownership, consent withdrawal, single leases, version/disable/pause gates, cooldown, targeted tests, deletion/no resurrection and denied anon/authenticated table/RPC access.');
}finally{await sql.close()}
