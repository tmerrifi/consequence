progs="tests-unbalanced-100-w0"
variants="mode=ICSpec viewer=true tokenclockadd=5000 minperiod=1000 maxperiod=200000|conseqSpec|gcc-consequence"
threads="2 4"
trials="1"
ops_pattern="cat $1 | grep OPS"
