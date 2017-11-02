#!/bin/bash
cd  /local_home/tmerrifi/dthreads/eval/tests/reverse_index;

threads=`echo $1 | awk '{if ($1<2){print 1;}else{print int($1/2);}}'`;
if [ $2 = "gcc-pthreads" ]
then
	(time ./reverse_index-pthread ../../datasets/reverse_index $threads)
else
	(time ./reverse_index-dthread_cv ../../datasets/reverse_index $threads)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
