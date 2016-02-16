
#You can pass it in two values: mode and arch
#mode - possible values are IC,RR and CheckSchedule

ifndef mode
	mode = IC
endif

ifeq ($(arch),32)
	LIB_ARCH = -m32
endif

ifndef clockmode
	clockmode = RetiredIns
endif

ifndef viewer
	viewer = false
endif

ifdef tokenclockadd
	CONSEQ_FLAGS += -DTOKEN_ACQ_ADD_CLOCK=$(tokenclockadd)
endif

ifdef minperiod
	CLOCK_MODULE_FLAGS += -DMIN_CLOCK_SAMPLE_PERIOD=$(minperiod)
endif

#CLOCK_MODULE_FLAGS += -DMIN_CLOCK_SAMPLE_PERIOD=20000

export CONSEQ_FLAGS

export CLOCK_MODULE_FLAGS

.PHONY : logical_clock thread_lib

all : logical_clock thread_lib

thread_lib :
	cd thread_lib;\
	make LIB_ARCH=$(arch) CONSEQ_TYPE=$(mode) CLOCK_MODE=$(clockmode) VIEWER=$(viewer) TOKEN_CLOCK_ADD=$(tokenclockadd);\
	cd -;

logical_clock :
	cd logical_clock;\
	make LIB_ARCH=$(arch) CONSEQ_TYPE=$(mode) CLOCK_MODE=$(clockmode);\
	cd -;

clean :
	cd thread_lib;\
	make clean; \
	cd -; \
	cd logical_clock;\
	make clean; \
	cd -;
