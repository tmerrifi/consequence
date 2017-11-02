#1: output file

cat $1 | grep -v "EVENT: 0" | awk '$7==42{t[$4]++;}END{for (i in t){print i" "t[i]}}'
