#1 thread_count

cd $SYNCHROBENCH_PATH/c-cpp/bin/;
ls;
#./MUTEX-hashtable -i $((1<<14)) -r $((1<<15)) -l 1 -t $1 -u 0 -d $((1000*5)) -x 0
./MUTEX-hashtable -i $((1<<17)) -r $((1<<19)) -l 1 -t $1 -u 0 -d $((1000*10)) -x 0
cd -;
