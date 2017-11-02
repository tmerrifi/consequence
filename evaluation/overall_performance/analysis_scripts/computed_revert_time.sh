for t in `seq 1 $2`;
do 
	cat $1 | grep "EVENT: "$t | awk '{if (start==0){start=$3 } else if (($4-start)<1700000){print $0}}' > /tmp/blah;

	cat /tmp/blah | grep "EVENT: "$t | awk '$5==39{c=0;}
	$5==43{totalcs++;c++;if(repeating==1){f--; if (f==0){ total+=($4-rtime) }  } }
	$5==40{f=c;repeating=1;rtime=$3;c=0;totalf+=f;}
	$5==44{repeating=0;c=0;f=0;}
	END{print "time: "total" repeatedcs: "totalf" cs: "totalcs-totalf;}'

	cat /tmp/blah | grep "EVENT: "$t | 
	awk '{if (start==0){start=$3}end=$4;}
		END{print "time: "end-start}'
done;
