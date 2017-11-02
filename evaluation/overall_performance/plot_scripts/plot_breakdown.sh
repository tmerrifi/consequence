#$1 a directory of output files
#$2 comment

#fields are in the form of... "output_<programName>_<variant>_<threadcount>_<trial>

cat breakdown_template.txt > /tmp/plottemplate;

for f in `ls $1 | grep output | egrep -v "runtime|*.pdf" | sort -t "_" -k 4 -n | sort -t "_" -k 3 -s`;
do
	filename=`echo $f | awk -F '_' '{print $2}'`;
	threads=`echo $f | awk -F '_' '{print $4}'`;
	variant=`echo $f | awk -F '_' '{print $3}' | sed s/true//g`;

	if [[ $threads -eq 1 ]]
	then
		echo "multimulti="$variant;
	fi;

	waitingtoken=`./../analysis_scripts/compute_time.sh $1/$f 1 | awk '{t+=$4;c++;}END{printf "%.2f",(t/c)*100}'`;
	waitinglowest=`./../analysis_scripts/compute_time.sh $1/$f 3 | awk '{t+=$4;c++;}END{printf "%.2f",(t/c)*100}'`;
	chunk=`./../analysis_scripts/compute_time.sh $1/$f 0 | awk '{t+=$4;c++;}END{printf "%.2f",(t/c)*100}'`;
	conseq=`./../analysis_scripts/compute_time.sh $1/$f 7 | awk '{t+=$4;c++;}END{printf "%.2f",(t/c)*100}'`;
	conv=`./../analysis_scripts/compute_time.sh $1/$f 2 | awk '{t+=$4;c++;}END{printf "%.2f",(t/c)*100}'`;

	echo $threads $waitingtoken $waitinglowest $chunk $conseq $conv;

done >> /tmp/plottemplate;

./bargraph.pl -pdf /tmp/plottemplate > ../figures/breakdown_$2.pdf;

#cp $1/breakdown.pdf ../figures;
