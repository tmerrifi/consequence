progs="synchrobench-hashtable-16K-1-0"

variants="CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
pthreads|pthreads|gcc-pthreads"

#variants="CONVLOGGING=true mode=ICWeak tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#CONVLOGGING=false mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#pthreads|pthreads|gcc-pthreads"

#variants="CONVLOGGING=true mode=ICSpecNoDet tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#CONVLOGGING=false mode=ICWeak tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
#pthreads|pthreads|gcc-pthreads"

#variants="CONVLOGGING=true mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICLogging|gcc-consequence;\
#CONVLOGGING=false mode=ICWeak tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
#CONVLOGGING=false mode=IC tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
#CONVLOGGING=false mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpec|gcc-consequence;\
#CONVLOGGING=false mode=ICWeakNoDet tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqICSpecLogging|gcc-consequence;\
#pthreads|pthreads|gcc-pthreads"

threads="1 2 4 8 16"
trials="1"
ops_pattern="awk '{if (\$1==\"#txs\"){print \$3}}'"
