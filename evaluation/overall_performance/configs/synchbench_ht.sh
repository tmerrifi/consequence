progs="synchrobench-hashtable-1K-64-5"
#variants="mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence;\
#mode=IC tokenclockadd=0 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence;\
#pthreads|pthreads|gcc-pthreads"
#variants="mode=IC tokenclockadd=0 minperiod=1000 maxperiod=200000|conseqIC|gcc-consequence"
#variants="pthreads|pthreads|gcc-pthreads;\
variants="CONVLOGGING=false mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence"
#mode=IC tokenclockadd=0 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence"
#pthreads|pthreads|gcc-pthreads"
threads="2"
trials="1"
ops_pattern="awk '{if (\$1==\"#txs\"){print \$3}}'"
