#!/bin/bash
DATASET_HOME=../../datasets;

if [[ $1 -lt 8 ]]
then
    threads=1;
elif [[ $1 -lt 16 ]]
then
    threads=2;
elif [[ $1 -lt 32 ]]
then
    threads=5;
elif [[ $1 -lt 48 ]]
then
    threads=7;
elif [[ $1 -lt 64 ]]
then
    threads=9;
else
    threads=11;
fi

cd /local_home/tmerrifi/dthreads/eval/tests/ferret;

if [ $2 = "gcc-pthreads" ]
then
	(time ./ferret-pthread $DATASET_HOME/ferret/corel lsh $DATASET_HOME/ferret/queries 10 20 $threads output.txt)
elif [ $3 = "gcc-dthreads" ]
then
	(time ./ferret-dthread $DATASET_HOME/ferret/corel lsh $DATASET_HOME/ferret/queries 10 20 $threads output.txt)
else
	(time ./ferret-dthread_cv $DATASET_HOME/ferret/corel lsh $DATASET_HOME/ferret/queries 10 20 $threads output.txt)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
