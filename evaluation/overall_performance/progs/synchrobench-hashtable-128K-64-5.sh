#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
./MUTEX-hashtable -i $((1<<17)) -r $((1<<18)) -l 64 -t $1 -u 5 -d $((1000*30))
cd -;
