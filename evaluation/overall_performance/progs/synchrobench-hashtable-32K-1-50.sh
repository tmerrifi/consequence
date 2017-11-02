#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<15)) -r $((1<<16)) -l 1 -t $1 -u 50 -d $((1000*60)) -x 0
cd -;
