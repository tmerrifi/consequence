#1: output file
#2: type

cat $1 | grep -v "EVENT: 0" | awk -v type="$2" '$7==type{t[$4]+=$6-$5;}END{for (i in t){print i" "t[i]}}'
