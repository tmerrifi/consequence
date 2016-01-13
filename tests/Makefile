
ifeq ($(CONSEQ_ARCH),32)
	LIB_ARCH = -m32
endif

SHELL = /bin/bash
LIBS = -lm -ldl
CFLAGS = $(LIB_ARCH) --std=gnu99 -Wall -g -O0 -Wl,-Tdata,0x80a8000 -DDTHREADS_TASKCLOCK_DEBUG -DNDEBUG
CONSEQ_LIBS = $(LIBS) -rdynamic -lconsequence -lrt

PTHREAD_LIBS = $(LIBS) -lpthread

all: progs

progs:
	gcc $(CFLAGS) bug-lockuse.c   -o bug-lockuse $(PTHREAD_LIBS)
	gcc $(CFLAGS) bug-lockuse.c   -o bug-lockuse-det $(CONSEQ_LIBS)
	gcc $(CFLAGS) -DTHREADS=1 bug-lockuse.c   -o bug-lockuse-singlethread-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) barrier_simple.c -o barrier-simple $(PTHREAD_LIBS)
	gcc --std=gnu99 $(CFLAGS) barrier_simple.c -o barrier-simple-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) condvar_simple.c -o condvar-simple $(PTHREAD_LIBS)	
	gcc --std=gnu99 $(CFLAGS) condvar_simple.c -o condvar-simple-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) condvar-broadcast.c -o condvar-broadcast $(PTHREAD_LIBS)			
	gcc --std=gnu99 $(CFLAGS) condvar-broadcast.c -o condvar-broadcast-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) fgl_simple.c -o fgl-simple $(PTHREAD_LIBS)			
	gcc --std=gnu99 $(CFLAGS) fgl_simple.c -o fgl-simple-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) racey.c -o racey $(PTHREAD_LIBS)
	gcc --std=gnu99 $(CFLAGS) racey.c -o racey-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) unbalanced.c -o unbalanced-det $(CONSEQ_LIBS)
	gcc --std=gnu99 $(CFLAGS) unbalanced.c -o unbalanced $(PTHREAD_LIBS)
clean:
	rm -f meta_* grace* TASK_CLOCK* barrier-simple barrier-simple-det condvar-simple condvar-simple-det fgl-simple-det fgl-simple racey race-det bug-lockuse bug-lockuse-det;
