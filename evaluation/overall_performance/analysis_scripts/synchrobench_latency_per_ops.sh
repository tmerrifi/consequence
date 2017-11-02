#$1: log file
#$2: token to grep for

cat $1 | grep $2 | \
awk 	'$2<10000{t[int($2/1000)]++;total+=$2;ops++;}\
     	$2>10000{t[10000]++;total+=$2;ops++;}\
	END{for (i in t){printf("%d %d %.2f %\n",i*1000,t[i],t[i]/ops*100);}; print "TOTAL: "total/ops " cycles/op"; }' | sort -n
