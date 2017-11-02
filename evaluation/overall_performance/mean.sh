#1 file
#2 column

cat $1 | awk -v col="$2" '{s+=$col;c++}END{print s/c;}';
