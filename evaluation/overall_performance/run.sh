#arg1: path to config file
#arg2: comment on this run

function usage(){
	echo "./run.sh -n <name> -c <config> [ -p ]";
	printf "\n\n";
}

#get the sequence number
seq=`cat seqnum`;
seq=$((seq+1))
echo $seq > seqnum;
mkdir out/$seq;
mkdir out/$seq/log;

while getopts "n:c:p:h" val
do
	case $val in
		h)
			usage;
			exit;
			;;
		n)
			name=$OPTARG;
			;;
		c)
			configFile=$OPTARG;
			;;

		p)
			plot=1;
			;;
		\?)
			usage;
			exit;
	esac
done

if [[ -z "$configFile" || -z "$name" ]]
then
	usage;
	exit;
fi

#more info for this run
date > out/$seq/date;
echo $name > out/$seq/comment;

export SYNCHROBENCH_PATH=../../../synchrobench;

export CONSEQ_PATH=../../;
source $configFile

echo $threads > out/$seq/threads;

echo $trials > out/$seq/trials;

rm -rf $CONSEQ_PATH/evaluation/clock_skew/output/$seq
mkdir $CONSEQ_PATH/evaluation/clock_skew/output/$seq

for p in $progs
do
	echo $p >> out/$seq/progs;
	for v in `echo $variants | sed s/" "/_/g | awk -F ';' '{for (i=1;i<=NF;i++){print $i;}}'`
	do
		vname=`echo $v | awk -F '|' '{print $1}' |sed s/mode=//g|sed s/clock//g|sed s/_//g|sed s/viewer=//g|sed s/"\""//g`
		#we use this in figures
		vtitle=`echo $v | awk -F '|' '{print $2}'`;
		#configuration 
		vconfig=`echo $v | awk -F '|' '{print $3}'`;
		#compile this variant
		v=`echo $v | awk -F '|' '{print $1;}'`;
		if [ $vconfig = "gcc-consequence" ]
		then
			#compile the library
			cd $CONSEQ_PATH;
			echo compiling with `echo $v | sed s/"_"/" "/g`
			./compile_conseq.sh "`echo $v | sed s/"_"/" "/g`" &>> /tmp/conseq_compile_log_$vname;
			cd - &> /dev/null;
			cd $SYNCHROBENCH_PATH/c-cpp;
			make clean;
			make lock use_consequence=true &>> /tmp/synchrobench_compile_log_$vname;
			cd -;
		else
			echo "no compile pthreads..."
			cd $SYNCHROBENCH_PATH/c-cpp;
			make clean;
                        make lock &>> /tmp/synchrobench_compile_log_$vname;
                        cd -;
		fi
		#make a nice variant name
		echo "#"$vname >> out/$seq/variants;
		for t in $threads
		do
			rm /tmp/results;
			for i in `seq 1 $trials`;
			do
				echo "running: "$p $vname $t
				outfile=output_$p"_"$vname"_"$t"_"$i;
				export logfile=out/$seq/log/$outfile;
				sudo truncate -s0 /var/log/syslog;
				(timeout 500s ./progs/"$p".sh $t $vconfig) &> out/$seq/log/$outfile;
				#ops=`eval $ops_pattern | awk '{t+=$1;}END{print t}'`;
				if [ -z $ops ]
				then
					ops=0;
				fi
	                        #get minutes in milliseconds
     		               	mins=`cat out/$seq/log/$outfile | grep real | awk -F 'm' '{print $1}' | awk '{print $2*1000*60}'`
               		        #get seconds and milliseconds
                       		rest=`cat out/$seq/log/$outfile | grep real | sed -r s/"[0-9]*m|s"//g | awk '{print $2}' | awk -F '.' '{print $1*1000+$2}'`
				fulllogpath=$CONSEQ_PATH/evaluation/overall_performance/out/$seq/log/$outfile;
				kernlogpath=$CONSEQ_PATH/evaluation/overall_performance/out/$seq/log/$outfile"_kernlog";
				#lets get the stats
				stats=`cat $fulllogpath | grep specstats:`;
				if [ -z $token ]
                                then
                                        token=0;
                                fi
				sleep 5;
				#sudo killall MUTEX-hashtable;
				sudo cat /var/log/syslog > $kernlogpath;
	                        echo $((mins+rest))" "$ops" "$token >> /tmp/results; #out/$seq/$p"_"$vname"_"$t"_"$i;
				#Ok, lets analyze this run
                                viewer=`echo $v | grep "viewer=" | wc -l`;
                                #if [ $viewer = 1 ]
                                #then
                                #        cd $CONSEQ_PATH/evaluation/thread_events_viewer;
                                #        ./chop_up_events_and_plot.sh $fulllogpath 3 200 5 $p"_"$vname"_"$t"_"$i"_"$seq;
                                #        cd -;
				#	filename=$p"_"$vname"_"$t"_"$i;
	                        #        cd $CONSEQ_PATH/evaluation/clock_skew;
        	                #        ./compute_wakeup_latency_logical.sh $fulllogpath $filename $seq;
                	        #        cd -;
                                #fi
			done;
			echo $t `./meanAndStddev.sh /tmp/results 3` >> out/$seq/$p"_"$vname"_token"
			echo $t `./meanAndStddev.sh /tmp/results 2` >> out/$seq/$p"_"$vname"_ops"
			echo $t `./meanAndStddev.sh /tmp/results 1` >> out/$seq/$p"_"$vname"_time"
			echo $t $stats >> out/$seq/$p"_"$vname"_stats"
		done;
		ops_files=$p"_"$vname"_ops,"$ops_files;
		time_files=$p"_"$vname"_time,"$time_files;
		titles=$vtitle","$titles;
	done;
	if [ -n "$plot" ]
	then
		echo $ops_files $titles;
		cd out/$seq;
		./../../../plot_scripts/plot_lines_points.sh $ops_files $titles 1:2 ops_$p.pdf;
		./../../../plot_scripts/plot_lines_points.sh $time_files $titles 1:2 times_$p.pdf;
		cd -;
	fi
	ops_files="";
	time_files="";
	titles="";
done;

cd $CONSEQ_PATH/tests;
make clean &> /dev/null;
make &> /dev/null;
cd -;
rm -f $SYNCHROBENCH_PATH/c-cpp/bin/grace* $SYNCHROBENCH_PATH/c-cpp/bin/TASK*;


