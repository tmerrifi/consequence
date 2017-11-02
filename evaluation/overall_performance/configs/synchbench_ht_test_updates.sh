#progs="synchrobench-hashtable-1M-8-1 synchrobench-hashtable-1M-8-5 synchrobench-hashtable-1M-8-10 synchrobench-hashtable-1M-8-20"
progs="synchrobench-hashtable-64K-1-50"
variants="CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
CONVLOGGING=false mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpecLogging|gcc-consequence"
#variants="CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence"
#variants="CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence"
threads="2 4 8"
trials="1"
ops_pattern="awk '{if (\$1==\"#txs\"){print \$3}}'"
