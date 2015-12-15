#./run_benchmarks.sh -b reverse_index -l dthread_cv -t 1 -p 8 > /tmp/out;
#./run_benchmarks.sh -b reverse_index -l dthread_cv -t 1 -p 8 > /tmp/out;

#pushd synthetic;
#./$1-test-cv > /tmp/out;
#popd;

lowest_time=`cat $1 | grep EVENT: | sort -n -k 3,3 | head -1 | awk '{print $3}'`

#2 871906 871924 3 1313640310 10000 0 0 0 2981326986 0

cat $1 | grep EVENT: | sed s/"EVENT: "/""/g | awk -v l="$lowest_time" '{print $1" "($2-l)" "($3-l)" "$4" "$5" "$6" "$7" "$8" "$9" "$10" "$11" "$12" "$13" "$14" "$15}' > thread_events_viewer/ri_output;


pushd thread_events_viewer;
cat ri_output | python thread_events_viewer.py $2;
popd;
