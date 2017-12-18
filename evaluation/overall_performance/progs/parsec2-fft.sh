#!/bin/bash
cd /local_home/tmerrifi/dthreads/eval/tests/fft/src;

if [[ $1 -eq 24 ]]
then
    threads=16;
else
    threads=$1;
fi

if [ $2 = "gcc-pthreads" ]
then
	(time ./run.sh $threads simmedium pthread)
else
	(time ./run.sh $threads simmedium dthread_cv)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
