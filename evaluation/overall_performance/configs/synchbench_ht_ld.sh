#progs="synchrobench-hashtable-1M-1-5 synchrobench-hashtable-1M-8-5 synchrobench-hashtable-1M-16-5 synchrobench-hashtable-1M-32-5 synchrobench-hashtable-1M-64-5"
progs="synchrobench-hashtable-1M-1-5 synchrobench-hashtable-1M-8-5 synchrobench-hashtable-1M-16-5"
#variants="CONVLOGGING=false mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqIC|gcc-consequence"
variants="CONVLOGGING=false mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqIC|gcc-consequence;\
CONVLOGGING=true mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
CONVLOGGING=false mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpecLogging|gcc-consequence;\
pthreads|pthreads|gcc-pthreads"
#variants="pthreads|pthreads|gcc-pthreads"
threads="2 4 8 16 32"
trials="1"
ops_pattern="awk '{if (\$1==\"#txs\"){print \$3}}'"
