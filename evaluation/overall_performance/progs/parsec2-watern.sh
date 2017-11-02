#!/bin/bash
cd /local_home/tmerrifi/dthreads/eval/tests/water_nsquared/src;

if [ $2 = "gcc-pthreads" ]
then
	(time ./run.sh $1 simlarge pthread)
	#./dedup-pthread -c -p -f -t $threads -i ../../datasets/dedup/media_medium.dat -o output.dat.ddp
else
	(time ./run.sh $1 simlarge dthread_cv)
	#./dedup-dthread_cv -c -p -f -t $threads -i ../../datasets/dedup/media_medium.dat -o output.dat.ddp
fi
rm -f meta_* grace* TASK_CLOCK*;
cd -;
