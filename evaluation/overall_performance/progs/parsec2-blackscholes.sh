#!/bin/bash
threads=$1;

echo $threads !!!

cd /local_home/tmerrifi/dthreads/eval/tests/blackscholes;
if [ $2 = "gcc-pthreads" ]
then
	(time ./blackscholes-pthread $threads ../../datasets/blackscholes/in_64K.txt prices.txt)
else
	(time ./blackscholes-dthread_cv $threads ../../datasets/blackscholes/in_64K.txt prices.txt)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
