INC_DIR = include
SRC_DIR = source

SRCS = $(SRC_DIR)/checkpoint.cpp $(SRC_DIR)/xrun.cpp $(SRC_DIR)/xthread.cpp $(SRC_DIR)/xmemory.cpp $(SRC_DIR)/prof.cpp $(SRC_DIR)/real.cpp $(SRC_DIR)/libdthread.cpp

DEPS = $(SRCS) $(INC_DIR)/syncstats.h $(INC_DIR)/speculation.h $(INC_DIR)/sync_types.h $(INC_DIR)/xpersist.h $(INC_DIR)/checkpoint.h $(INC_DIR)/xdefines.h $(INC_DIR)/xglobals.h $(INC_DIR)/xpersist.h $(INC_DIR)/xplock.h $(INC_DIR)/xrun.h $(INC_DIR)/xheap.h $(INC_DIR)/warpheap.h $(INC_DIR)/xadaptheap.h $(INC_DIR)/xoneheap.h $(INC_DIR)/determ.h $(INC_DIR)/heaplayers/segheap.h $(INC_DIR)/heaplayers/util/sllist.h $(INC_DIR)/heaplayers/adapt.h $(INC_DIR)/heaplayers/zoneheap.h $(INC_DIR)/xmemory.h

INCLUDE_DIRS = -I$(INC_DIR) -I$(INC_DIR)/heaplayers -I$(INC_DIR)/heaplayers/util

ifeq ($(CLOCK_MODE),Cycles)
	CLOCK_FLAGS = -DUSE_CYCLES_TICKS
endif

ifeq ($(VIEWER),true)
	VIEWER_MODE = -DEVENT_VIEWER
endif

ifeq ($(CONSEQ_TYPE),ICSpec)  
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_DEFERRED_WORK -DSINGLE_THREAD_OPT -DUSE_SPECULATION -DSPEC_USE_TICKS 
	BINARY_NAME = libconsequence_IC.so
endif

ifeq ($(CONSEQ_TYPE),ICSpecNoDet)  
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_DEFERRED_WORK -DSINGLE_THREAD_OPT -DUSE_SPECULATION -DSPEC_USE_TICKS -DNO_DETERM_SYNC
	BINARY_NAME = libconsequence_IC.so
endif


ifeq ($(CONSEQ_TYPE),ICSpecCycles) 
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_SPECULATION -DSPEC_USE_TICKS -DTRACK_LIBRARY_CYCLES -DUSE_CYCLES_TICKS -DNO_DETERM_SYNC
	BINARY_NAME = libconsequence_IC.so
endif


ifeq ($(CONSEQ_TYPE),IC)
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_DEFERRED_WORK -DSINGLE_THREAD_OPT -DUSE_TX_COARSENING
	BINARY_NAME = libconsequence_IC.so
endif


ifeq ($(CONSEQ_TYPE),RRSpec) 
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_SPECULATION -DSPEC_USE_TICKS -DTOKEN_ORDER_ROUND_ROBIN
	BINARY_NAME = libconsequence_IC.so
endif


ifeq ($(CONSEQ_TYPE),ICPlain)
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_DEFERRED_WORK
	BINARY_NAME = libconsequence_IC.so
endif

ifeq ($(CONSEQ_TYPE),ICWeak)
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_DEFERRED_WORK -DWEAK_DETERMINISM
	BINARY_NAME = libconsequence_IC.so
endif

ifeq ($(CONSEQ_TYPE),ICWeakNoDet)
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_DEFERRED_WORK -DWEAK_DETERMINISM -DNO_DETERM_SYNC
	BINARY_NAME = libconsequence_IC.so
endif


ifeq ($(CONSEQ_TYPE),RR)
	CONSEQ_FLAGS = -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK -DTOKEN_ORDER_ROUND_ROBIN
	BINARY_NAME = libconsequence_RR.so
endif

ifeq ($(CONSEQ_TYPE),CheckSchedule)
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_TX_COARSENING -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK -DPRINT_SCHEDULE
	BINARY_NAME = libconsequence_ICEvent.so
endif

ifeq ($(arch),32)
	LIB_ARCH = -m32
endif

CXX = g++

#-fno-inline-small-functions

CFLAGS32 = -g -march=core2 -mtune=core2 -msse3 $(LIB_ARCH) -DSSE_SUPPORT -O3 -shared -fPIC -D'CUSTOM_PREFIX(x)=grace\#\#x' $(CONSEQ_FLAGS) $(CLOCK_FLAGS) $(VIEWER_MODE)

LIBS = -ldl -lpthread -lDetermTaskClock -lksnap

all: $(BINARY_NAME)

$(BINARY_NAME): $(SRCS) $(DEPS) Makefile
	$(CXX) $(CFLAGS32) $(INCLUDE_DIRS) $(SRCS) -o $(BINARY_NAME) $(LIBS); \
	cp $(BINARY_NAME) libconsequence.so;\
	sudo cp libconsequence.so /usr/local/lib;
	sudo ldconfig;
clean:
	rm -f *.so 

