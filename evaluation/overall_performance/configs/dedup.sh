progs="parsec2-dedup"
variants="CONVLOGGING=false mode=ICSpec tokenclockadd=0 minperiod=1000 maxperiod=200000|Consequence|gcc-consequence;"
#CONVLOGGING=false mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|DEPOS|gcc-consequence;\
#pthreads|pthreads|gcc-pthreads"
#threads="1 2 4 8 16 24 32"
threads="8"
trials="1"
ops_pattern="cat $1 | grep OPS"
