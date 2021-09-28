cvs update -l 2>/dev/null 1>&2
egrep "^Group\|" *.gdf | cut -f3 -d '|' | sort -n
