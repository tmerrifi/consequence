
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


.PHONY : logical_clock thread_lib

all : logical_clock thread_lib

thread_lib :
	cd thread_lib;\
	make LIB_ARCH=$(arch) CONSEQ_TYPE=$(mode) CLOCK_MODE=$(clockmode) VIEWER=$(viewer);\
	cd -;

logical_clock :
	cd logical_clock;\
	make LIB_ARCH=$(arch) CONSEQ_TYPE=$(mode) CLOCK_MODE=$(clockmode); \
	cd -;

clean :
	cd thread_lib;\
	make clean; \
	cd -; \
	cd logical_clock;\
	make clean; \
	cd -;
