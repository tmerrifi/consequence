#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/matrix_multiply;

if [ $2 = "gcc-pthreads" ]
then
	(time ./matrix_multiply-pthread 1200 1200 $threads);
else
	(time ./matrix_multiply-dthread_cv 1200 1200 $threads);
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
