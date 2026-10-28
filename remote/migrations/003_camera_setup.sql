-- Optional consumer setup/rename activation. Apply before CAMERA_SETUP_ENABLED=true.
begin;
-- Fail rather than block live camera writes indefinitely behind another transaction.
set local lock_timeout='5s';
set local statement_timeout='60s';
do $$ begin
 if to_regclass('public.rv_recipients') is null or to_regclass('public.rv_deliveries') is null then
  raise exception 'Apply migration 002_recipients before camera setup';
 end if;
 if exists(select 1 from pg_attribute where attrelid='public.rv_devices'::regclass and attname='display_name' and not attisdropped and atttypid<>'text'::regtype) then
  raise exception 'Unexpected display_name type; review schema drift before migration';
 end if;
end $$;
alter table public.rv_devices add column if not exists display_name text check(length(display_name) between 1 and 48);
create table if not exists public.rv_hardware_claims(hardware text primary key check(hardware ~ '^[a-f0-9]{32}$'),device uuid references public.rv_devices(id));
alter table public.rv_hardware_claims enable row level security;
revoke all on public.rv_hardware_claims from public,anon,authenticated;
grant all on public.rv_hardware_claims to service_role;
create or replace function public.rv_claim_pair_v2(p_hash text,p_token text,p_name text,p_hardware text) returns uuid language plpgsql security definer set search_path=pg_catalog,public,pg_temp as $$
declare pair public.rv_pair_codes;bound public.rv_hardware_claims;old public.rv_devices;new_id uuid;
begin
 select * into pair from public.rv_pair_codes where code_hash=p_hash and expires>now() for update;
 if pair.owner is null then raise exception 'Pairing expired or invalid';end if;
 insert into public.rv_hardware_claims(hardware) values(p_hardware) on conflict do nothing;
 select * into bound from public.rv_hardware_claims where hardware=p_hardware for update;
 if bound.device is not null then select * into old from public.rv_devices where id=bound.device for update;if old.enabled then raise exception 'Camera already registered; owner must disconnect it first';end if;end if;
 -- Serialize enabled-camera limit across different hardware claims for this owner.
 perform pg_advisory_xact_lock(hashtextextended(pair.owner::text,0));
 if (select count(*) from public.rv_devices where owner=pair.owner and enabled)>=20 then raise exception 'Camera limit reached';end if;
 insert into public.rv_devices(owner,name,token_hash) values(pair.owner,left(p_name,48),p_token) returning id into new_id;
 update public.rv_hardware_claims set device=new_id where hardware=p_hardware;
 delete from public.rv_pair_codes where code_hash=p_hash;
 return new_id;
end $$;
revoke all on function public.rv_claim_pair_v2(text,text,text,text) from public,anon,authenticated;
grant execute on function public.rv_claim_pair_v2(text,text,text,text) to service_role;
-- Bind upgraded, already-paired firmware without changing its existing owner or history.
create or replace function public.rv_bind_hardware(p_device uuid,p_hardware text) returns boolean language plpgsql security definer set search_path=pg_catalog,public,pg_temp as $$
declare bound uuid;
begin
 -- Match pairing's hardware-row then device-row lock order. Recheck enabled under lock.
 if not exists(select 1 from public.rv_devices where id=p_device and enabled) then return false;end if;
 insert into public.rv_hardware_claims(hardware) values(p_hardware) on conflict do nothing;
 select device into bound from public.rv_hardware_claims where hardware=p_hardware for update;
 perform 1 from public.rv_devices where id=p_device and enabled for update;
 if not found then return false;end if;
 if bound is null then update public.rv_hardware_claims set device=p_device where hardware=p_hardware;return true;end if;
 return bound=p_device;
end $$;
revoke all on function public.rv_bind_hardware(uuid,text) from public,anon,authenticated;
grant execute on function public.rv_bind_hardware(uuid,text) to service_role;
commit;
