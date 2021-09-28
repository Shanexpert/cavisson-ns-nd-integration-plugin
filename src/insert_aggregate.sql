DROP FUNCTION IF EXISTS percentile_add(integer, integer) CASCADE;

DROP FUNCTION IF EXISTS median_percentile_final(integer) CASCADE;

DROP FUNCTION IF EXISTS eighty_percentile_final(integer) CASCADE;

DROP FUNCTION IF EXISTS ninety_percentile_final(integer) CASCADE;

DROP FUNCTION IF EXISTS ninety_five_percentile_final(integer) CASCADE;

DROP FUNCTION IF EXISTS ninety_nine_percentile_final(integer) CASCADE;

DROP FUNCTION IF EXISTS eighty_five_percentile_final(integer) CASCADE;

CREATE FUNCTION percentile_add(integer, integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'percentile_add';

CREATE FUNCTION median_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'median_percentile_final';

CREATE FUNCTION eighty_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'eighty_percentile_final';

CREATE FUNCTION eighty_five_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'eighty_five_percentile_final';

CREATE FUNCTION ninety_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'ninety_percentile_final';

CREATE FUNCTION ninety_five_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'ninety_five_percentile_final';

CREATE FUNCTION ninety_nine_percentile_final(integer) RETURNS integer LANGUAGE C AS '/var/lib/pgsql/percentile.so', 'ninety_nine_percentile_final';

CREATE AGGREGATE median_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=median_percentile_final, INITCOND=0);

CREATE AGGREGATE eighty_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=eighty_percentile_final, INITCOND=0);

CREATE AGGREGATE eighty_five_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=eighty_five_percentile_final, INITCOND=0);

CREATE AGGREGATE ninety_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=ninety_percentile_final, INITCOND=0);

CREATE AGGREGATE ninety_five_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=ninety_five_percentile_final, INITCOND=0);

CREATE AGGREGATE ninety_nine_percentile (BASETYPE=integer, SFUNC=percentile_add, STYPE=integer, FINALFUNC=ninety_nine_percentile_final, INITCOND=0);

CREATE SEQUENCE HIBERNATE_SEQUENCE START WITH 1 INCREMENT BY 1;
