#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<10)) -r $((1<<11)) -l 64 -t $1 -u 5 -x 0
cd -;
