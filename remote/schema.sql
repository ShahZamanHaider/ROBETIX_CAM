-- Run once in a dedicated Supabase project. No client may access these tables directly.
create extension if not exists pgcrypto;
create table public.rv_devices(id uuid primary key default gen_random_uuid(),owner uuid not null references auth.users(id),name text not null,token_hash text unique not null,enabled boolean not null default true,last_seen timestamptz,config jsonb not null default '{}',last_notify timestamptz);
create table public.rv_pair_codes(code_hash text primary key,owner uuid not null references auth.users(id),expires timestamptz not null);
create table public.rv_events(id uuid primary key default gen_random_uuid(),device uuid not null references public.rv_devices(id) on delete cascade,event_key text not null,local_event bigint not null default 0,created_at timestamptz not null default now(),epoch bigint not null default 0,offset_minutes int not null default 0,score int not null default 0,photo text,notification text not null default 'disabled',provider_id text,error text,unique(device,event_key));
create table public.rv_rates(key text primary key,start_at timestamptz not null default now(),count int not null default 1);
alter table public.rv_devices enable row level security;
alter table public.rv_pair_codes enable row level security;
alter table public.rv_events enable row level security;
alter table public.rv_rates enable row level security;
revoke all on public.rv_devices,public.rv_pair_codes,public.rv_events,public.rv_rates from anon,authenticated;
create or replace function public.rv_rate(p_key text,p_limit int,p_window int) returns boolean language plpgsql security definer set search_path=public as $$
declare n int;
begin
 insert into rv_rates(key) values(p_key) on conflict(key) do update set count=case when rv_rates.start_at<now()-make_interval(secs=>p_window) then 1 else rv_rates.count+1 end,start_at=case when rv_rates.start_at<now()-make_interval(secs=>p_window) then now() else rv_rates.start_at end returning count into n;
 return n<=p_limit;
end $$;
create or replace function public.rv_claim_pair(p_hash text,p_token text,p_name text) returns uuid language plpgsql security definer set search_path=public as $$
declare pair rv_pair_codes;new_id uuid;
begin
 select * into pair from rv_pair_codes where code_hash=p_hash and expires>now() for update;
 if pair.owner is null then raise exception 'Pairing expired or invalid';end if;
 delete from rv_pair_codes where code_hash=p_hash;
 insert into rv_devices(owner,name,token_hash) values(pair.owner,left(p_name,48),p_token) returning id into new_id;
 return new_id;
end $$;
create or replace function public.rv_queue(p_device uuid,p_event uuid,p_test boolean) returns text language plpgsql security definer set search_path=public as $$
declare d rv_devices;s text;current_state text;
begin
 select notification into current_state from rv_events where id=p_event and device=p_device for update;
 if current_state is null then raise exception 'Invalid event';end if;
 if current_state<>'uploading' then return current_state;end if;
 select * into d from rv_devices where id=p_device for update;
 if not d.enabled or coalesce((d.config->>'whatsapp')::boolean,false)=false or coalesce((d.config->>'opt_in')::boolean,false)=false then s='disabled';
 elsif not p_test and d.last_notify>now()-make_interval(secs=>greatest(30,least(3600,coalesce((d.config->>'interval')::int,60)))) then s='rate_limited';
 else s='queued';update rv_devices set last_notify=now() where id=p_device;end if;
 update rv_events set notification=s where id=p_event and device=p_device;
 return s;
end $$;
-- Atomic sending lease: an ambiguous Meta submission is never automatically retried.
create or replace function public.rv_lease(p_event uuid) returns boolean language plpgsql security definer set search_path=public as $$
begin update rv_events set notification='sending' where id=p_event and notification='queued';return found;end $$;
revoke all on function public.rv_rate(text,int,int),public.rv_claim_pair(text,text,text),public.rv_queue(uuid,uuid,boolean),public.rv_lease(uuid) from public,anon,authenticated;
grant execute on function public.rv_rate(text,int,int),public.rv_claim_pair(text,text,text),public.rv_queue(uuid,uuid,boolean),public.rv_lease(uuid) to service_role;
insert into storage.buckets(id,name,public,file_size_limit,allowed_mime_types) values('rv-media','rv-media',false,131072,array['image/jpeg']) on conflict(id) do update set public=false,file_size_limit=131072,allowed_mime_types=array['image/jpeg'];
-- No anonymous/authenticated storage policies. Only server service-role access is used.

grant all on public.rv_devices,public.rv_pair_codes,public.rv_events,public.rv_rates to service_role;
