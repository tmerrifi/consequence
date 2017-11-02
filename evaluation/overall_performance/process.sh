cd out/$1;

for v in `cat variants | sed s/#//g | awk '{printf $1" "}'`
do
	echo $v;
	for p in `cat progs | awk '{printf $1" "}'`
	do
		printf $p" ";
		for t in `cat threads`
		do
			trials=`cat trials`;
			total=0;
			for tr in `seq 1 $trials`
			do
				result=`cat $p"_"$v"_"$t"_"$tr`;
				total=$((total+result))
			done;
			printf $((total/trials))" "
		done;
	done;
	printf "\n";
done;
cd -;
