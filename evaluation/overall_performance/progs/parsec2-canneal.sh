#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

echo $threads !!!

cd /local_home/tmerrifi/dthreads/eval/tests/canneal;

if [ $2 = "gcc-pthreads" ]
then
	(time ./canneal-pthread $threads 15000 2000 $DATASET_HOME/canneal/400000.nets 128)
else
	(time ./canneal-dthread_cv $(THREADS) 15000 2000 $(DATASET_HOME)/canneal/400000.nets 128)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
