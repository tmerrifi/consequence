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
./MUTEX-hashtable -i $((1<<15)) -r $((1<<16)) -l 1 -t $1 -u 50 -d $((1000*60)) -x 0
cd -;
