#1 comma delimited list of files
#2 comma delimited list of names
#3 x and y field numbers in gnuplot style
#4 output file

pwd;

i=1
for f in `echo $1 | awk -F ',' '{for (i=1;i<=NF;i++){print $i;}}'`
do
	#grabe the name in argument 2
	name=`echo $2 | awk -F ',' '{print $'$i';}'`;

	if [ $i -gt 1 ]
	then
		printf ","
	fi

	printf "'"$f"'"
	printf " using "$3;
	printf " with linespoints";
	printf " title '"$name;	
	printf "'"

	i=$((i+1))
done > /tmp/plot;

gnuplot << HERE
	set term pdf monochrome dashed font ",14" linewidth 3
	set output '$4'
	set logscale y
	set ytics ("1M" 1000000, "2M" 2000000, "10M" 10000000, "25M" 25000000, "50M" 50000000, "100M" 100000000, "200M" 200000000, "300M" 300000000)
	set xtics (1,2,4,8,16,24,32)
	set xlabel 'Threads'
	set ylabel 'Total Ops Completed'
	plot [:] [100000:300000000] `cat /tmp/plot`
HERE

#gnuplot << HERE
#        set term pdf monochrome dashed font ",14" linewidth 3
#        set output '$4'
#        set xlabel 'Threads'
#        set ylabel 'Runtime (ms)'
#	set xtics (1,2,4,8,16,24,32)
#        plot [:] [:] `cat /tmp/plot`
#HERE
