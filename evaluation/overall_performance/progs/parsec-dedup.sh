#!/bin/bash
threads=`echo $1 | awk '{if ($1 <= 4){print 1;}else{print $1/4;}}'`;
parsecmgmt -c $2 -i simlarge -n $threads -a run -p dedup;
