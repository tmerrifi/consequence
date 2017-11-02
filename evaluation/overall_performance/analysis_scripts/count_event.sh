#1: output file
#2: type

cat $1 | grep -v "EVENT: 0" | awk -v type="$2" '$5==type{t[$2]++;}END{for (i in t){print i" "t[i]}}'
