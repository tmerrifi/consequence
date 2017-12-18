#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/kmeans;

if [ $2 = "gcc-pthreads" ]
then
	(time ./kmeans-pthread -d 2 -c 100 -p 50000 -s 100 -t $threads)
else
	(time ./kmeans-dthread_cv -d 2 -c 100 -p 50000 -s 100 -t $threads)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
