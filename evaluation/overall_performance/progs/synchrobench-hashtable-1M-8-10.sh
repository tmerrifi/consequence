#1 thread_count

#-i, --initial-size <int>
#-r, --range <int>
#-l , --load-factor <int> (keys over buckets)
#-t, --thread-num <int>
#-u, --update-rate <int>
#-d, --duration <int>
#-x, --unit-tx (default=1)


cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<20)) -r $((1<<21)) -l 8 -t $1 -u 10 -d $((1000*60)) -x 0;
rm *.mem* TASK_CLOCK*;
cd -;
