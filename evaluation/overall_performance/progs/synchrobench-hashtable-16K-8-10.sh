#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<14)) -r $((1<<15)) -l 8 -t $1 -u 10 -d $((1000*60))
cd -;
