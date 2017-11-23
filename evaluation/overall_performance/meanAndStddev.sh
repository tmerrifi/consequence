#1 file
#2 column

mean=`cat $1 | awk -v col="$2" '{s+=$col;c++;}END{printf("%.4f",s/c);}'`;
len=`wc -l $1`;
stddev=`cat $1 | awk -v col="$2" -v len="$len" -v mean="$mean" \
'{s+=($col-mean)*($col-mean);}END{printf("%.4f",sqrt(s/len));}'`;

echo $mean $stddev;