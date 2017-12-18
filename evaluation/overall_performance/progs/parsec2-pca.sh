#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/pca;

if [ $2 = "gcc-pthreads" ]
then
	(time ./pca-pthread -r 4000 -c 4000 -s 100 -t $threads)
else
	(time ./pca-dthread_cv -r 4000 -c 4000 -s 100 -t $threads)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
