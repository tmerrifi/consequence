#!/bin/bash
cd /local_home/tmerrifi/dthreads/eval/tests/lu_ncb/src;

if [ $2 = "gcc-pthreads" ]
then
	(time ./run.sh $1 $3 pthread)
elif [ $2 = "gcc-dthreads" ]
then
	(time ./run.sh $1 $3 dthread)
else
	(time ./run.sh $1 $3 dthread_cv)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
