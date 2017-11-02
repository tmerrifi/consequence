#!/bin/bash

#benchmarks="parsec2-dedup parsec2-watern"
benchmarks="parsec2-revindex"

variants="ICSpectokenadd=5000minperiod=1000maxperiod=200000 ICtokenadd=0minperiod=1000maxperiod=200000" 
threads="4 8 16 32"

echo "program,locks,spec locks,signals,spec signals,barrier,tokens,reverts,commits,mean revert cs, mean commit cs"

for v in $variants
do
	for b in $benchmarks
	do 
		for t in $threads
		do
			cat "../out/$1/log/output_"$b"_"$v"_"$t"_"1 | grep specstats | sed s/"specstats:"//g | \
				sed s/-nan/0/g | \
				awk -v p="$b" -v t="$t" -F ',' \
				'{for (i=1;i<=NF;i++){a[i]+=$i;} nf=NF;count++;}\
				END{a[9]=a[9]/count; a[10]=a[10]/10;}\
				END{printf p"&"t"&"; for (i=1;i<=nf;i++){if (a[i] > 1000){printf("%.2fK&",a[i]/1000);}\
							else {printf("%.2f&",a[i]);}}}'
				printf "\n";
		done;
	done;
done;
