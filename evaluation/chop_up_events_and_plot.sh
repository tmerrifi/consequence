#args
#1 location of output file from program run
#2 how many images to make
#3 which segment to start at? (some might be hundreds long but we only care about 50-75 (so 25 50 would be the arguments for 2 & 3)
#4 us per pixel
#5 user friendly name (workload name?)

width=50000

cat $1 | grep EVENT: | awk -v usperpixel="$4" '$5<10{seg=int($3/('$width'*usperpixel)); print seg"_"$0;}' > out;

for i in `seq $3 $(($3+$2))`;
do

cat out | egrep "^"$i"_EVENT" | \
awk -v usperpixel="$4" -v seg="$i" '{print $1" "$2" "int(($3-('$width'*usperpixel*int(seg)))/usperpixel)" "int(($4-('$width'*usperpixel*int(seg)))/usperpixel)" "$5" "$6" "$7" "$8" "$9" "$10" "$11" "$12" "$13" "$14" "$15}' | \
sed s/"[0-9]*_"/""/g> /tmp/out$i;

./gen_visualization.sh /tmp/out$i $5_$i;

done;
