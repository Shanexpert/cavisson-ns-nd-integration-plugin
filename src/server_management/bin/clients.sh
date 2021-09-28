#psql  -X  -A -U postgres -d demo -t -c "select team, channel , owner, count (machine_type) as count from allocation where team in (select team from clients) group by team, channel, owner order by team;"

if [[ ! -z $1 ]]; then
	psql -X  -A -U postgres -d demo -t << EOF
	SELECT clients.team, clients.channel, clients.owner, count(allocation.machine_type) as TOT
	FROM allocation right outer join clients using(team,channel)
	WHERE team='$1' and channel!='default'
	GROUP BY clients.team, clients.channel, clients.owner
	ORDER BY team;
EOF
else
	psql -X  -A -U postgres -d demo -t << EOF
	SELECT clients.team, clients.channel, clients.owner, count(allocation.machine_type) as TOT
	FROM allocation right outer join clients using(team,channel)
	GROUP BY clients.team, clients.channel, clients.owner
	ORDER BY team;
EOF
fi
