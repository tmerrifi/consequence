#args
#1 location of output file from program run
#2 how many images to make
#3 which segment to start at? (some might be hundreds long but we only care about 50-75 (so 25 50 would be the arguments for 2 & 3)
#4 pixels per us
#5 user friendly name (workload name?)

width=10000


cat $1 | grep EVENT: | awk '{seg=int($3/'$width'); print seg" "$0;}' > out;

sleep 10;

echo "# of segments: "`cat out | awk '{if ($1 > m){m=$1;}}END{print m}'`

for i in `seq $3 $(($3+$2))`;
do

    echo "working on figure "$i;
    
    cat out | \
        awk -v usperpixel="$4" -v width="$width" -v seg="$i" '$1==seg{ \
printf $2" "$3" "($4-($1*width))*usperpixel" "($5-($1*width))*usperpixel" "; \
       for (i=6;i<=16;i++){ printf $i" ";} printf "\n";}' | sed s/"[0-9]*_"/""/g> /tmp/out$i;

    lowest_time=`cat /tmp/out$i | grep EVENT: | sort -n -k 3,3 | head -1 | awk '{print $3}'`

    cat /tmp/out$i | grep EVENT: | sed s/"EVENT: "/""/g |
        awk -v l="$lowest_time" '{printf $1" "($2-l)" "($3-l)" "; for (i=4;i<=15;i++){ printf $i" ";} printf "\n";}' > ri_output;
  

    cat ri_output | python thread_events_viewer.py $5"_"$i $((width*$4));
    mv ri_output "trace_"$5"_"$i;
    
done;

rm out /tmp/out*;
