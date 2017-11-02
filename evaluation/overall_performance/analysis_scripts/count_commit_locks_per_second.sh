#1: output file

cat $1 | grep -v "EVENT: 0" | awk '$5==42{if (start==0){start=$3;} t[$2]++; end=$4;}END{secs=(end-start)/1000000;for (i in t){total+=t[i]/secs; print i" "t[i]/secs;} print total;}'
