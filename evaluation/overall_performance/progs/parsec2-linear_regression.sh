#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/linear_regression;

if [ $2 = "gcc-pthreads" ]
then
	(time ./linear_regression-pthread $DATASET_HOME/linear_regression/key_file_500MB.txt $threads)
elif [ $2 = "gcc-dthreads" ]
then
	(time ./linear_regression-dthread $DATASET_HOME/linear_regression/key_file_500MB.txt $threads)
else
	(time ./linear_regression-dthread_cv $DATASET_HOME/linear_regression/key_file_500MB.txt $threads)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
