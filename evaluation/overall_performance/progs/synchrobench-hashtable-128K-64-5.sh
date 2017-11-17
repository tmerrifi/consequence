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
./MUTEX-hashtable -i $((1<<17)) -r $((1<<18)) -l 64 -t $1 -u 5 -d $((1000*60)) -x 0
cd -;
