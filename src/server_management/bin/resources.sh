add()
{
  if [ -z "$DESCRIPTION" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -d missing"
    exit 1
  fi
  if [ -z "$LINK" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -l missing"
    exit 1
  fi
  if [ -z "$SUB" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -s missing"
    exit 1
  fi
  if [ -z "$AUTHOR" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -a missing"
    exit 1
  fi
  if [ -z "$CATEGORY" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -c missing"
    exit 1
  fi
  psql -U postgres -d demo -A -t -c "insert into resources(subject, description, link, author, added_on, category) values('$SUB','$DESCRIPTION','$LINK','$AUTHOR','$(date)','$CATEGORY')" >/dev/null 2>&1
  if [[ $? -eq 0 ]]; then
    echo "0"
    echo "Resource added successfully."
  else
    echo "1"
    echo "Error in adding resource."
  fi
}

delete()
{
  if [ -z "$SUB" ]; then
    echo "1"
    echo "ERROR: Mandatory argument -s missing"
    exit 1
  fi
  psql -U postgres -d demo -A -t -c "delete from resources where subject='$SUB'" >/dev/null 2>&1
  if [[ $? -eq 0 ]]; then
    echo "0"
    echo "Resource Deleted successfully."
  else
    echo "1"
    echo "Error in Deleting resource."
  fi
}
display()
{
  psql -U postgres -d demo -A -t -c "select row_number() over(order by subject), subject, author, category, link, added_on, description from resources"
}

display_help_and_exit()
{
  echo "USAGE: $0 -o <Operation> -s <subject> -d <description> -l <link> -a <author> -c <category>
  value of operation could be:
  add , delete or display"
  exit 1
}

while getopts o:d:l:s:a:c: arg
do
  case $arg in
    o) OPERATION=$OPTARG;;
    d) DESCRIPTION=$OPTARG;;
    l) LINK=$OPTARG;;
    s) SUB=$OPTARG;;
    a) AUTHOR=$OPTARG;;
    c) CATEGORY=$OPTARG;;
    *) echo "ERROR: Invalid Option."
      display_help_and_exit ;;
  esac
done

if [[ "x$OPERATION" == "xadd" ]]; then
  add
elif [[ "x$OPERATION" == "xdelete" ]]; then
  delete
elif [[ "x$OPERATION" == "xdisplay" ]]; then
  display
else
  echo "ERROR: Invalid Option"
  display_help_and_exit
fi
