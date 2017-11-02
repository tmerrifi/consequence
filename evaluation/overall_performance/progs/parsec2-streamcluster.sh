#!/bin/bash
cd ~/dthreads/eval/tests/streamcluster;
if [ $2 = "gcc-pthreads" ]
then
	(time ./streamcluster-pthread 10 20 128 16384 16384 1000 none output.txt $1)
else
	(time ./streamcluster-dthread_cv 10 20 128 16384 16384 1000 none output.txt $1)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
