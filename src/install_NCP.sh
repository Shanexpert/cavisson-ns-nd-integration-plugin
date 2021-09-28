#!/bin/sh
# Purpose: To install servers database for netcloud server management.
# Author: Gyanendra Veeru

ptype=$(nsi_show_config)
if [ "xx$ptype" == "xxNCP" ]
then
  echo "Going to create tables for Netcloud Server management."
  if psql -lqt | cut -d \| -f 1 | grep "demo" > /dev/null; then
    echo 'NCP database is already present in db';
  else
   createdb -U postgres demo
   if [[ $? -eq 0 ]]; then
     echo "Database for NC Server management created."
     psql demo -U postgres <<+
     SET statement_timeout = 0;
     SET lock_timeout = 0;
     SET client_encoding = 'UTF8';
     SET standard_conforming_strings = on;
     SELECT pg_catalog.set_config('search_path', '', false);
     SET check_function_bodies = false;
     SET client_min_messages = warning;
     SET row_security = off;

     --
     -- Name: plpgsql; Type: EXTENSION; Schema: -; Owner:
     --

     CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


     --
     -- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner:
     --

     COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


     --
     -- Name: test(); Type: FUNCTION; Schema: public; Owner: postgres
     --

     CREATE FUNCTION public.test() RETURNS SETOF record
         LANGUAGE plpgsql
         AS \$$
     DECLARE
     i RECORD;
     BEGIN
      FOR i IN select team, channel, owner from clients order by team
      LOOP
       return query
       SELECT team, channel, count(*) from allocation where team=i.team and channel=i.channel;
      END LOOP;
     END;
     \$$;


     ALTER FUNCTION public.test() OWNER TO postgres;

     SET default_tablespace = '';

     SET default_with_oids = false;

     --
     -- Name: allocation; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.allocation (
         server_name text,
         server_ip text,
         blade_name text NOT NULL,
         ubuntu_version text NOT NULL,
         machine_type text NOT NULL,
         status boolean NOT NULL,
         team text NOT NULL,
         channel text NOT NULL,
         owner text NOT NULL,
         allocation text NOT NULL,
         build_version text,
         build_upgradation_date timestamp without time zone,
         server_type character(2),
         controller_ip text,
         controller_blade text,
         refresh_at timestamp without time zone,
         bandwidth integer,
         shared character varying[],
         bkp_blade character varying,
         bkp_ctrl character varying
     );


     ALTER TABLE public.allocation OWNER TO postgres;

     --
     -- Name: billing; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.billing (
         server_name text NOT NULL,
         server_ip text,
         vendor text,
         status boolean,
         uptime timestamp without time zone,
         uptime_epoch integer,
         downtime timestamp without time zone,
         duration interval
     );


     ALTER TABLE public.billing OWNER TO postgres;

     --
     -- Name: billing_cc; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.billing_cc (
         server_name text,
         server_ip text,
         vendor text,
         status boolean,
         uptime timestamp without time zone,
         downtime_epoch integer,
         downtime timestamp without time zone,
         duration interval,
         next_mail integer
     );


     ALTER TABLE public.billing_cc OWNER TO postgres;

     --
     -- Name: client_env; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.client_env (
         env character varying,
         appname character varying,
         envtype character varying,
         product character varying,
         hostname character varying,
         ip character varying,
         controller character varying,
         nodemasterver character varying,
         appliancever character varying,
         cmonagents character varying,
         applications text,
         javaagentver character varying,
         nodejsagentver character varying,
         cavwmonver character varying,
         patchinfo character varying,
         latestupdate text,
         bkpctrlr character varying
     );


     ALTER TABLE public.client_env OWNER TO postgres;

     --
     -- Name: clients; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.clients (
         team text,
         channel text,
         owner text
     );


     ALTER TABLE public.clients OWNER TO postgres;

     --
     -- Name: location; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.location (
         location text,
         country character varying,
         state character varying,
         lat double precision,
         lon double precision,
         ccode character varying(2),
         zone character varying
     );


     ALTER TABLE public.location OWNER TO postgres;

     --
     -- Name: resources; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.resources (
         subject character varying NOT NULL,
         description text,
         link character varying NOT NULL,
         author character varying,
         added_on timestamp without time zone,
         category character varying
     );


     ALTER TABLE public.resources OWNER TO postgres;

     --
     -- Name: servers; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.servers (
         server_name text NOT NULL,
         server_ip text NOT NULL,
         vendor text NOT NULL,
         location text NOT NULL,
         zone text NOT NULL,
         cpu integer NOT NULL,
         ram integer NOT NULL,
         total_disk_size character varying,
         server_type character(2),
         kernal character varying,
         avail_disk_root integer,
         avail_disk_home numeric,
         refresh_at timestamp without time zone,
         spec_rating numeric,
         model text,
         cpu_freq numeric,
         c_param character varying,
         country character varying,
         state character varying
     );


     ALTER TABLE public.servers OWNER TO postgres;

     --
     -- Name: vendor; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE public.vendor (
         vendor text
     );


     ALTER TABLE public.vendor OWNER TO postgres;

     --
     -- Name: servers_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
     --

     ALTER TABLE ONLY public.servers
         ADD CONSTRAINT servers_pkey PRIMARY KEY (server_ip);


     --
     -- Name: allocation_server_ip_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
     --

     ALTER TABLE ONLY public.allocation
         ADD CONSTRAINT allocation_server_ip_fkey FOREIGN KEY (server_ip) REFERENCES public.servers(server_ip) ON DELETE CASCADE;


     --
     -- Name: SCHEMA public; Type: ACL; Schema: -; Owner: postgres
     --

     REVOKE ALL ON SCHEMA public FROM PUBLIC;
     REVOKE ALL ON SCHEMA public FROM postgres;
     GRANT ALL ON SCHEMA public TO postgres;
     GRANT ALL ON SCHEMA public TO PUBLIC;

+
   else
     echo "Could not create NCP database. PostgreSQL Server may not be responding."
     exit 1
   fi
 fi
else
  echo "Product Type if not NCP. Hence skipping NCP database installation."
fi
mkdir -p $NS_WDIR/server_management/logs/ $NS_WDIR/server_management/conf $NS_WDIR/server_management/tmp
touch -a $NS_WDIR/server_management/logs/blade_state.log $NS_WDIR/server_management/logs/assignment.log $NS_WDIR/server_management/logs/addDelete.log $NS_WDIR/server_management/logs/build_upgrade.log
if [ ! -e $NS_WDIR/server_management/conf/NCP.conf ]; then
  echo -n -e "#ND_IP=x.x.x.x\nmaps_key=lYrP3vF3Uk5zgTiGGuEzQGwGIVDGuy24" > $NS_WDIR/server_management/conf/NCP.conf
fi
