progs="parsec2-dedup parsec2-watern"
variants="mode=ICSpec tokenclockadd=5000 minperiod=1000 maxperiod=200000 |DEPOS|gcc-consequence"
threads="4 8 16"
trials="1"
ops_pattern="cat $1 | grep OPS"