#!/bin/sh
# Purpose: To install servers database for netcloud server management.
# Author: Gyanendra Veeru

if [ "xx`grep 'NCSM' /home/cavisson/etc/cav.conf | grep -v '^#' | awk '{print $2}'`" == "xx1" ] && [ "xx`grep 'CONFIG' /home/cavisson/etc/cav.conf | grep -v '^#' | awk '{print $2}'`" == "xxNC" ]
then
  echo "Going to create table for Netcloud Server management."
  if psql -lqt | cut -d \| -f 1 | grep "servers" > /dev/null; then
    echo 'servers database is already present in db';
  else
   createdb -U postgres servers
   if [[ $? -eq 0 ]]; then
     echo "Database for NC Server management created."
     psql servers -U postgres << EOF
     --
     -- PostgreSQL database dump
     --

     -- Dumped from database version 9.5.9
     -- Dumped by pg_dump version 9.5.6

     SET statement_timeout = 0;
     SET lock_timeout = 0;
     SET client_encoding = 'UTF8';
     SET standard_conforming_strings = on;
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


     SET search_path = public, pg_catalog;

     SET default_tablespace = '';

     SET default_with_oids = false;

     --
     -- Name: allocation; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE allocation (
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
         bandwidth integer
     );


     ALTER TABLE allocation OWNER TO postgres;

     --
     -- Name: billing; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE billing (
         server_name text NOT NULL,
         server_ip text,
         vendor text,
         status boolean,
         uptime timestamp without time zone,
         uptime_epoch integer,
         downtime timestamp without time zone,
         duration interval
     );


     ALTER TABLE billing OWNER TO postgres;

     --
     -- Name: billing_cc; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE billing_cc (
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


     ALTER TABLE billing_cc OWNER TO postgres;

     --
     -- Name: clients; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE clients (
         team text,
         channel text,
         owner text
     );


     ALTER TABLE clients OWNER TO postgres;

     --
     -- Name: demo; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE demo (
         date interval
     );


     ALTER TABLE demo OWNER TO postgres;

     --
     -- Name: location; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE location (
         location text
     );


     ALTER TABLE location OWNER TO postgres;

     --
     -- Name: my_allocation; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE my_allocation (
         server_name text,
         server_ip text,
         blade_name text,
         ubuntu_version text,
         machine_type text,
         status boolean,
         channel text,
         team text,
         owner text,
         allocation text,
         build_version text,
         build_upgradation_date timestamp without time zone,
         server_type character(2)
     );


     ALTER TABLE my_allocation OWNER TO postgres;

     --
     -- Name: portal_users; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE portal_users (
         username text,
         password character varying,
         role character varying(10),
         logging_status boolean,
         execution_area text,
         session_count integer
     );


     ALTER TABLE portal_users OWNER TO postgres;

     --
     -- Name: scratch; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE scratch (
         column1 character varying,
         column2 character varying
     );


     ALTER TABLE scratch OWNER TO postgres;

     --
     -- Name: server; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE server (
         name text NOT NULL,
         ip text NOT NULL
     );


     ALTER TABLE server OWNER TO postgres;

     --
     -- Name: servers; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE servers (
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
         refresh_at timestamp without time zone
     );


     ALTER TABLE servers OWNER TO postgres;

     --
     -- Name: users; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE users (
         user_name text,
         team text
     );


     ALTER TABLE users OWNER TO postgres;

     --
     -- Name: vendor; Type: TABLE; Schema: public; Owner: postgres
     --

     CREATE TABLE vendor (
         vendor text
     );


     ALTER TABLE vendor OWNER TO postgres;

     --
     -- Name: server_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
     --

     ALTER TABLE ONLY server
         ADD CONSTRAINT server_pkey PRIMARY KEY (name);


     --
     -- Name: servers_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
     --

     ALTER TABLE ONLY servers
         ADD CONSTRAINT servers_pkey PRIMARY KEY (server_ip);


     --
     -- Name: allocation_server_ip_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
     --

     ALTER TABLE ONLY allocation
         ADD CONSTRAINT allocation_server_ip_fkey FOREIGN KEY (server_ip) REFERENCES servers(server_ip) ON DELETE CASCADE;


     --
     -- Name: public; Type: ACL; Schema: -; Owner: postgres
     --

     REVOKE ALL ON SCHEMA public FROM PUBLIC;
     REVOKE ALL ON SCHEMA public FROM postgres;
     GRANT ALL ON SCHEMA public TO postgres;
     GRANT ALL ON SCHEMA public TO PUBLIC;


     --
     -- PostgreSQL database dump complete
     --
EOF
   else
     echo "Could not create servers db. PostgreSQL Server may not be responding."
     exit 1
   fi
 fi
else
  echo "NCSM Not enabled for this machine. Hence skipping installation."
fi
