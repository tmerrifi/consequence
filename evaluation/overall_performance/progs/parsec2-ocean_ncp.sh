#!/bin/bash
cd /local_home/tmerrifi/dthreads/eval/tests/ocean_ncp/src;

if [[ $1 -eq 24 ]]
then
    threads=16;
else
    threads=$1;
fi

if [ $2 = "gcc-pthreads" ]
then
	(time ./run.sh $threads $size pthread)
elif [ $2 = "gcc-dthreads" ]
then
	(time ./run.sh $threads $size dthread)
else
	(time ./run.sh $threads $size dthread_cv)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
