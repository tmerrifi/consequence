INC_DIR = include
SRC_DIR = source

SRCS = $(SRC_DIR)/checkpoint.cpp $(SRC_DIR)/xrun.cpp $(SRC_DIR)/xthread.cpp $(SRC_DIR)/xmemory.cpp $(SRC_DIR)/prof.cpp $(SRC_DIR)/real.cpp $(SRC_DIR)/libdthread.cpp

DEPS = $(SRCS) $(INC_DIR)/syncstats.h $(INC_DIR)/speculation.h $(INC_DIR)/sync_types.h $(INC_DIR)/xpersist.h $(INC_DIR)/checkpoint.h $(INC_DIR)/xdefines.h $(INC_DIR)/xglobals.h $(INC_DIR)/xpersist.h $(INC_DIR)/xplock.h $(INC_DIR)/xrun.h $(INC_DIR)/warpheap.h $(INC_DIR)/xadaptheap.h $(INC_DIR)/xoneheap.h $(INC_DIR)/determ.h

INCLUDE_DIRS = -I$(INC_DIR) -I$(INC_DIR)/heaplayers -I$(INC_DIR)/heaplayers/util


ifeq ($(CONSEQ_TYPE),IC)
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK #-DEVENT_VIEWER
	BINARY_NAME = libconsequence_IC.so
endif

ifeq ($(CONSEQ_TYPE),ICPlain)
	CONSEQ_FLAGS = -DFAST_FORWARD -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK #-DEVENT_VIEWER
	BINARY_NAME = libconsequence_IC.so
endif

ifeq ($(CONSEQ_TYPE),RR)
	CONSEQ_FLAGS = -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK -DTOKEN_ORDER_ROUND_ROBIN -DEVENT_VIEWER
	BINARY_NAME = libconsequence_RR.so
endif

ifeq ($(CONSEQ_TYPE),CheckSchedule)
	CONSEQ_FLAGS = -DFAST_FORWARD -DSINGLE_THREAD_OPT -DUSE_TX_COARSENING -DUSE_USERSPACE_READING -DUSE_DEFERRED_WORK -DPRINT_SCHEDULE -DEVENT_VIEWER
	BINARY_NAME = libconsequence_ICEvent.so
endif

ifeq ($(arch),32)
	LIB_ARCH = -m32
endif

CXX = g++

CFLAGS32 = -g -march=core2 -mtune=core2 -msse3 $(LIB_ARCH) -DSSE_SUPPORT -O3 -shared -fPIC -D'CUSTOM_PREFIX(x)=grace\#\#x' $(CONSEQ_FLAGS) 

LIBS = -ldl -lpthread -lDetermTaskClock -lksnap

all: $(BINARY_NAME)

$(BINARY_NAME): $(SRCS) $(DEPS) Makefile
	$(CXX) $(CFLAGS32) $(INCLUDE_DIRS) $(SRCS) -o $(BINARY_NAME) $(LIBS); \
	cp $(BINARY_NAME) libconsequence.so;\
	sudo cp libconsequence.so /usr/local/lib;
	sudo ldconfig;
clean:
	rm -f *.so 

