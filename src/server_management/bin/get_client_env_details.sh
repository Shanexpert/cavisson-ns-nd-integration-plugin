psql demo -t -A -U postgres -c "select row_number() over (order by env nulls last), * from client_env"
