#1: output file
#2: type


cat $1 | grep -v "EVENT: 0" | grep "EVENT:" | \
awk -v type="$2" '$5==type{if (($4-$3)==0 && $5==0){t[$2]+=5;}else{t[$2]+=$4-$3;}}
NR==0{start[$2]=$3;}
{if (start[$2]==0){start[$2]=$3;}last[$2]=$4;}
END{for (i in t){print i" "t[i]" "last[i]-start[i]" "t[i]/(last[i]-start[i]) }}' | sort -n -k 1,1
