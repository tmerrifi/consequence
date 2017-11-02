cat $1 | grep OPS | awk '{t+=$2;}END{print $t;}'
