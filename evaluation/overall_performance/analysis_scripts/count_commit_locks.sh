#1: output file

cat $1 | grep -v "EVENT: 0" | awk '$5==42{t[$2]++;}END{for (i in t){print i" "t[i]}}'
