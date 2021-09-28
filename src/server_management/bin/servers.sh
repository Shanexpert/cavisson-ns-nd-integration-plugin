blades()
{
	psql -X  -A -U postgres -d demo -t -c "select row_number() over (order by server_name nulls last), server_name, server_ip, blade_name, build_version, team, channel, allocation, machine_type, refresh_at from allocation order by server_name"
}

servers()
{
	psql -X  -A -U postgres -d demo -t -c "select row_number() over(order by server_name), server_name, server_ip, vendor, location, zone, cpu, ram, total_disk_size, avail_disk_root, avail_disk_home, kernal, refresh_at from servers order by server_name"
}
while getopts abc arg
do
        case $arg in
                a) servers ;;
                b) blades ;;
                *) echo "Invalid option";;
        esac
done

