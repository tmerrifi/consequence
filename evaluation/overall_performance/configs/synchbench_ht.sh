progs="synchrobench-hashtable-1M-8-1" #synchrobench-hashtable-1M-8-10 synchrobench-hashtable-1M-8-20"

variants="CONVCOUNTERS=true CONVLOGGING=true mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
CONVCOUNTERS=true mode=ICNoCoarse tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqIC|gcc-consequence;\
CONVCOUNTERS=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
CONVCOUNTERS=true mode=ICWeak tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICWeak|gcc-consequence;\
CONVCOUNTERS=true mode=ICWeakNoDet tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICWeak|gcc-consequence;\
pthreads|pthreads|gcc-pthreads"

threads="8"
trials="1"
ops_pattern="awk '{if (\$1==\"#txs\"){print \$3}}'"
