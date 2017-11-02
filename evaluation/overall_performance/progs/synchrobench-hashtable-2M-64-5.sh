#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<21)) -r $((1<<22)) -l 64 -t $1 -u 5
cd -;
