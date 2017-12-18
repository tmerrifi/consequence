#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/word_count;

if [ $2 = "gcc-pthreads" ]
then
	(time ./word_count-pthread $DATASET_HOME/word_count/word_100MB.txt 10 $threads);
else
	(time ./word_count-dthread_cv $DATASET_HOME/word_count/word_100MB.txt 10 $threads);
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
