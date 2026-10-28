-- Apply AFTER schema.sql. Existing installations run this migration only.
begin;
alter table public.rv_devices add column if not exists recipients_initialized boolean not null default false;
alter table public.rv_devices add column if not exists whatsapp_paused boolean not null default false;
alter table public.rv_events add column if not exists is_test boolean not null default false;
create table if not exists public.rv_recipients(
 id uuid primary key default gen_random_uuid(), device uuid not null references public.rv_devices(id) on delete cascade,
 name text not null check(length(name) between 1 and 48), phone_cipher text not null, phone_hash text not null,
 enabled boolean not null default true, consent boolean not null default false, consent_at timestamptz,
 motion boolean not null default true, photo boolean not null default true,
 interval int not null default 60 check(interval between 30 and 3600), version int not null default 1,
 last_notify timestamptz, created_at timestamptz not null default now(), updated_at timestamptz not null default now(),
 unique(device,phone_hash),check(consent=(consent_at is not null))
);
create table if not exists public.rv_deliveries(
 id uuid primary key default gen_random_uuid(), event uuid not null references public.rv_events(id) on delete cascade,
 recipient uuid references public.rv_recipients(id) on delete set null, recipient_version int not null,
 state text not null default 'queued', provider_id text unique, error text,
 created_at timestamptz not null default now(), updated_at timestamptz not null default now(),unique(event,recipient)
);
create index if not exists rv_recipients_device on public.rv_recipients(device);
create index if not exists rv_deliveries_event on public.rv_deliveries(event);
create index if not exists rv_deliveries_recipient_time on public.rv_deliveries(recipient,created_at desc);
alter table public.rv_recipients enable row level security;
alter table public.rv_deliveries enable row level security;
revoke all on public.rv_recipients,public.rv_deliveries from public,anon,authenticated;
grant all on public.rv_recipients,public.rv_deliveries to service_role;

-- Import at most one opted-in legacy number. Encryption is performed by the backend.
-- Row lock prevents concurrent first requests from recreating deleted contacts.
create or replace function public.rv_initialize_recipients(p_device uuid,p_legacy jsonb) returns void language plpgsql security definer set search_path=public as $$
declare d rv_devices;
begin
 select * into d from rv_devices where id=p_device for update;
 if d.id is null or d.recipients_initialized then return;end if;
 if p_legacy is not null then
  insert into rv_recipients(device,name,phone_cipher,phone_hash,consent,consent_at,photo,interval)
  values(d.id,'Existing recipient',p_legacy->>'phone_cipher',p_legacy->>'phone_hash',true,now(),coalesce((d.config->>'photo')::boolean,true),greatest(30,least(3600,coalesce((d.config->>'interval')::int,60))));
 end if;
 update rv_devices set recipients_initialized=true,config=config-'phone'-'opt_in'-'opt_in_at' where id=p_device;
end $$;

-- All contact mutations use the camera lock, bounding contacts at five even under races.
create or replace function public.rv_save_recipient(p_device uuid,p_id uuid,p_value jsonb) returns text language plpgsql security definer set search_path=public as $$
declare old rv_recipients;duplicate_id uuid;
begin
 perform 1 from rv_devices where id=p_device for update;if not found then return 'not_found';end if;
 if p_id is not null then select * into old from rv_recipients where id=p_id and device=p_device for update;if old.id is null then return 'not_found';end if;end if;
 select id into duplicate_id from rv_recipients where device=p_device and phone_hash=p_value->>'phone_hash' and (p_id is null or id<>p_id);
 if duplicate_id is not null then return 'duplicate';end if;
 if p_id is null and (select count(*) from rv_recipients where device=p_device)>=5 then return 'limit';end if;
 insert into rv_recipients(id,device,name,phone_cipher,phone_hash,enabled,consent,consent_at,motion,photo,interval,version)
 values(coalesce(p_id,gen_random_uuid()),p_device,p_value->>'name',p_value->>'phone_cipher',p_value->>'phone_hash',(p_value->>'enabled')::boolean,(p_value->>'consent')::boolean,
 case when (p_value->>'consent')::boolean then case when old.consent and old.phone_hash=p_value->>'phone_hash' then old.consent_at else now() end else null end,
 (p_value->>'motion')::boolean,(p_value->>'photo')::boolean,(p_value->>'interval')::int,coalesce(old.version,0)+1)
 on conflict(id) do update set name=excluded.name,phone_cipher=excluded.phone_cipher,phone_hash=excluded.phone_hash,enabled=excluded.enabled,consent=excluded.consent,consent_at=excluded.consent_at,motion=excluded.motion,photo=excluded.photo,interval=excluded.interval,version=excluded.version,updated_at=now();
 return 'saved';
end $$;

create or replace function public.rv_queue(p_device uuid,p_event uuid,p_test boolean) returns text language plpgsql security definer set search_path=public as $$
declare d rv_devices;s text;current_state text;
begin
 select notification into current_state from rv_events where id=p_event and device=p_device for update;
 if current_state is null then raise exception 'Invalid event';end if;if current_state<>'uploading' then return current_state;end if;
 select * into d from rv_devices where id=p_device for update;
 if not d.enabled or d.whatsapp_paused or not coalesce((d.config->>'whatsapp')::boolean,false) then s='disabled';
 elsif not p_test and d.last_notify>now()-make_interval(secs=>greatest(30,least(3600,coalesce((d.config->>'interval')::int,60)))) then s='rate_limited';
 else s='queued';update rv_devices set last_notify=now() where id=p_device;end if;
 update rv_events set notification=s where id=p_event and device=p_device;return s;
end $$;

create or replace function public.rv_prepare_deliveries(p_event uuid,p_recipient uuid default null) returns setof rv_deliveries language plpgsql security definer set search_path=public as $$
declare e rv_events;d rv_devices;r rv_recipients;s text;
begin
 select * into e from rv_events where id=p_event for update;if e.id is null then return;end if;
 select * into d from rv_devices where id=e.device for update;
 for r in select * from rv_recipients where device=d.id and (p_recipient is null or id=p_recipient) order by id limit 5 for update loop
  if exists(select 1 from rv_deliveries where event=e.id and recipient=r.id) then continue;end if;
  s='queued';
  if not d.enabled or d.whatsapp_paused or not coalesce((d.config->>'whatsapp')::boolean,false) or not r.enabled or not r.consent or (not e.is_test and not r.motion) then s='disabled';
  elsif not e.is_test and r.last_notify>now()-make_interval(secs=>r.interval) then s='rate_limited';end if;
  insert into rv_deliveries(event,recipient,recipient_version,state) values(e.id,r.id,r.version,s);
  if s='queued' and not e.is_test then update rv_recipients set last_notify=now() where id=r.id;end if;
 end loop;
 return query select * from rv_deliveries where event=e.id;
end $$;

-- Re-check current consent, enablement, version and device pause immediately before sending.
create or replace function public.rv_delivery_lease(p_id uuid) returns boolean language plpgsql security definer set search_path=public as $$
declare current_state text;
begin
 update rv_deliveries x set state=case when exists(select 1 from rv_recipients r join rv_devices d on d.id=r.device join rv_events e on e.device=d.id where e.id=x.event and r.id=x.recipient and r.version=x.recipient_version and r.enabled and r.consent and (e.is_test or r.motion) and d.enabled and not d.whatsapp_paused and coalesce((d.config->>'whatsapp')::boolean,false)) then 'sending' else 'disabled' end,updated_at=now() where x.id=p_id and x.state='queued';
 -- FOUND guards a second lease attempt, even inside the same transaction.
 if not found then return false;end if;
 select state into current_state from rv_deliveries where id=p_id;return current_state='sending';
end $$;

revoke all on function public.rv_initialize_recipients(uuid,jsonb),public.rv_save_recipient(uuid,uuid,jsonb),public.rv_prepare_deliveries(uuid,uuid),public.rv_delivery_lease(uuid) from public,anon,authenticated;
grant execute on function public.rv_initialize_recipients(uuid,jsonb),public.rv_save_recipient(uuid,uuid,jsonb),public.rv_prepare_deliveries(uuid,uuid),public.rv_delivery_lease(uuid) to service_role;
commit;
