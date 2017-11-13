#!/bin/bash

if [[ $1 -lt 8 ]]
then
    threads=1;
elif [[ $1 -lt 16 ]]
then
    threads=2;
elif [[ $1 -lt 32 ]]
then
    threads=4;
elif [[ $1 -lt 48 ]]
then
    threads=10;
elif [[ $1 -lt 64 ]]
then
    threads=18;
else
    threads=25;
fi

cd /local_home/tmerrifi/dthreads/eval/tests/dedup;

if [ $2 = "gcc-pthreads" ]
then
	(time ./dedup-pthread -c -p -f -t $threads -i ../../datasets/dedup/media_large.dat -o output.dat.ddp)
else
	(time ./dedup-dthread_cv -c -p -f -t $threads -i ../../datasets/dedup/media_large.dat -o output.dat.ddp)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
