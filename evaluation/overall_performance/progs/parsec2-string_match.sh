#!/bin/bash
threads=$1;
DATASET_HOME=../../datasets;

cd /local_home/tmerrifi/dthreads/eval/tests/string_match;

if [ $2 = "gcc-pthreads" ]
then
	(time ./string_match-pthread $DATASET_HOME/string_match/key_file_500MB.txt $threads)
else
	(time ./string_match-dthread_cv $DATASET_HOME/string_match/key_file_500MB.txt $threads)
fi

rm -f meta_* grace* TASK_CLOCK*;
cd -;
