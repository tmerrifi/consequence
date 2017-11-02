#This script runs the experiment but assumes the EVENT_VIEWER option for consequence is turned on
#The result is some event diagrams and instruction count plots

#arg1: path to config file
#arg2: comment on this run

#get the sequence number
seq=`cat seqnum`;
seq=$((seq+1))
echo $seq > seqnum;

echo "sequence number is: " $seq " for: "$2

mkdir out/$seq;
mkdir out/$seq/log;

#more info for this run
date > out/$seq/date;
echo $2 > out/$seq/comment;

export CONSEQ_PATH=../../;
source $1

echo $threads > out/$seq/threads;

echo $trials > out/$seq/trials;

rm -rf $CONSEQ_PATH/evaluation/clock_skew/output/$seq
mkdir $CONSEQ_PATH/evaluation/clock_skew/output/$seq

for p in $progs
do
	#conseq_config=TOKEN_ACQ_ADD_CLOCK:0
	echo $p >> out/$seq/progs;
	for v in `echo $variants | sed s/" "/_/g | awk -F ';' '{for (i=1;i<=NF;i++){print $i;}}'`
	do
		vname=`echo $v|sed s/mode=//g|sed s/clock//g|sed s/_//g|sed s/viewer=//g`
		#compile this variant
		cd $CONSEQ_PATH;
		echo compiling with `echo $v | sed s/"_"/" "/g`
		./compile_conseq.sh `echo $v | sed s/"_"/" "/g` &>> /tmp/conseq_compile_log_$vname;
		cd - &> /dev/null;
		#make a nice variant name
		echo "#"$vname >> out/$seq/variants;
		for t in $threads
		do
			for i in `seq 1 $trials`;
			do
				echo "running: "$p $vname $t
				filename=$p"_"$vname"_"$t"_"$i;
				outfile=output_$filename;
				(time ./progs/"$p".sh $t) &> out/$seq/log/$outfile;
				echo "processing output.."
				sleep $((3*t));
	                        #get minutes in milliseconds
        	                mins=`cat out/$seq/log/$outfile | grep real | awk -F 'm' '{print $1}' | awk '{print $2*1000*60}'`
                	        #get seconds and milliseconds
                        	rest=`cat out/$seq/log/$outfile | grep real | sed -r s/"[0-9]*m|s"//g | awk '{print $2}' | awk -F '.' '{print $1*1000+$2}'`
	                        printf $((mins+rest)) >> out/$seq/$filename;
				fulllogpath=$CONSEQ_PATH/evaluation/overall_performance/out/$seq/log/$outfile;
				#Ok, lets analyze this run
				viewer=`echo $v | grep "viewer=" | wc -l`;
				if [ $viewer = 1 ]
				then
					cd $CONSEQ_PATH/evaluation/thread_events_viewer;
					./chop_up_events_and_plot.sh $fulllogpath 10 200 5 $filename"_"$seq;
					cd -;
				fi
				cd $CONSEQ_PATH/evaluation/clock_skew;
				#./compute_ticks_per_us.sh $fulllogpath $filename"_"$seq;
				#./compute_variance_per_tx.sh $fulllogpath $filename"_"$seq;
				./compute_wakeup_latency_logical.sh $fulllogpath $filename $seq;
				cd -;
			done;
		done;
	done;
done;
