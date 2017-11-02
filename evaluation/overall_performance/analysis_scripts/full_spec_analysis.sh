#$1 - The trace file
#$2 - The number of threads in the trace
#$3 - The length of time in the trace to analyze (in milliseconds)

#rm this file if its there
rm -f /tmp/full_spec_per_thread;

for t in `seq 1 $2`;
do 
	#trim the trace to just use the right time range
	cat $1 | grep "EVENT: "$t | awk '{if (start==0){start=$3 } else if (($4-start)<'$(($3*1000))'){print $0}}' > /tmp/blah;

	#43 is a speculative unlock...totalcs tracks how many locks we've released (repeated and new)
	#c tracks the number of cs's done during a transaction
	#40 is a failed spec...f tracks the number of cs's we need to repeat
	
	cat /tmp/blah | grep "EVENT: "$t | awk '
	BEGIN{total=0;totalf=0;totalcs=0;}
	$5==39{c=0;}
	$5==43{totalcs++;c++;if(repeating==1){f--; if (f==0){ total+=($4-rtime) }  } }
	$5==24{totalcs++}
	$5==40{f=c;repeating=1;rtime=$3;c=0;totalf+=f;}
	$5==44{repeating=0;c=0;f=0;}
	END{print "thread '$t' repeatedtime: "total" repeatedcs: "totalf" netcs: "totalcs-totalf" allcs: "totalcs;}' >> /tmp/full_spec_per_thread;
done;

cat /tmp/full_spec_per_thread | \
awk 	'BEGIN{print "#THREAD REPEATED_TIME REPEATED_CS NET_CS ALL_CS";}
	/thread/{rtime+=$4; rcs+=$6; netcs+=$8; allcs+=$10;c++;print $2" "$4" "$6" "$8" "$10}
	END{print "TOTALS "rtime" "rcs" "netcs" "allcs;}'
