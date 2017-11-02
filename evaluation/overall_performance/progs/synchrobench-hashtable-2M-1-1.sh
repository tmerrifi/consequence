#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<20)) -r $((1<<21)) -l 1 -t $1 -u 1
cd -;
