#progs="tests-unbalanced-1000 tests-unbalanced-5000 tests-unbalanced-10000 tests-unbalanced-20000"
progs="tests-unbalanced-1000"
variants="mode=ICPlain clockmode=Cycles tokenclockadd=30000 minperiod=1000 maxperiod=50000|plainAdd30K;\
mode=ICSpec clockmode=Cycles tokenclockadd=0 minperiod=1000 maxperiod=200000|specAdd0KMax200KMin1K;\
mode=ICSpec clockmode=Cycles tokenclockadd=10000 minperiod=1000 maxperiod=200000|specAdd10KMax200KMin1K;\
mode=ICSpec clockmode=Cycles tokenclockadd=20000 minperiod=1000 maxperiod=200000|specAdd20KMax200KMin1K;\
mode=ICSpec clockmode=Cycles tokenclockadd=30000 minperiod=1000 maxperiod=200000|specAdd30KMax200KMin1K"
threads="1 2 4 8"
trials="3"
ops_pattern="cat $1 | grep OPS"
