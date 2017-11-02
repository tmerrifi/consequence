#!/bin/bash
threads=`echo $1 | awk '{if ($1 <= 4){print 1;}else{print $1/4;}}'`;

cd /local_home/tmerrifi/dthreads/eval/tests/dedup;

if [ $2 = "gcc-pthreads" ]
then
	(time ./dedup-pthread -c -p -f -t $threads -i ../../datasets/dedup/media_large.dat -o output.dat.ddp)
else
	(time ./dedup-dthread_cv -c -p -f -t $threads -i ../../datasets/dedup/media_large.dat -o output.dat.ddp)
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
