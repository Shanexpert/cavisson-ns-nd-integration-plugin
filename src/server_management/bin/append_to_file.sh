var=$(echo $1|cut -d "+" -f 1,2,3,4,5)
var1=$(echo $1|cut -d "+" -f 6)
echo $var >> /tmp/$var1
