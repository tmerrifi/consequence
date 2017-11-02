progs="parsec-blackscholes parsec-dedup parsec-ferret parsec-swaptions parsec-vips"
variants="mode=ICPlain tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqPlain|gcc-consequence;\
mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence;\
pthreads|pthreads|gcc-pthreads"
threads="1 2 4 8 16 24 32"
trials="3"
ops_pattern="cat $1 | grep OPS"
