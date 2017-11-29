progs="synchrobench-hashtable-1M-16-5 synchrobench-hashtable-1M-64-5 synchrobench-hashtable-1M-128-5"
progs=$progs" synchrobench-hashtable-16K-64-5 synchrobench-hashtable-128K-64-5 synchrobench-hashtable-2M-64-5"
progs=$progs" synchrobench-hashtable-1M-64-1 synchrobench-hashtable-1M-64-10 synchrobench-hashtable-1M-64-20"

variants="CONVCOUNTERS=true mode=ICNoCoarse tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqIC|gcc-consequence;\
CONVCOUNTERS=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
CONVCOUNTERS=true mode=ICWeak tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICWeak|gcc-consequence;\
CONVCOUNTERS=true mode=ICWeakNoDet tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICWeakNoDet|gcc-consequence;\
CONVCOUNTERS=true mode=ICSpecNoDet tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpecNoDet|gcc-consequence;\
pthreads|pthreads|gcc-pthreads"

threads="1 2 4 8 16 24 32"
trials="3"
ops_pattern="cat \$rawOutputFile | awk '{if (\$1==\"#txs\"){print \$3}}'"
parseConversionStats="true"
timeoutSecs="90"
