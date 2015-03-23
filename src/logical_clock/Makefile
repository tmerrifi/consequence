
ifeq ($(CONSEQ_TYPE),RR)
	FLAGS = -DNO_INSTRUCTION_COUNTING
endif

SHELL = /bin/bash
CFLAGS = -g -std=gnu99 -fPIC -L/usr/lib $(LIB_ARCH) -DDEBUG_CLOCK_CACHE_OFF -DDETERM_CLOCK_SAMPLE_PERIOD=5000 $(FLAGS)
SOURCES = ./lib/perf_counter.o ./lib/determ_clock.o
LIBS = -lrt -lksnap

.PHONY : module

all : dynlib module

dynlib: $(SOURCES) 
	gcc -shared $(CFLAGS) -o libDetermTaskClock.so $(SOURCES) $(LIBS); \
	sudo cp libDetermTaskClock.so /usr/lib; \
	echo "10000000" | sudo tee /proc/sys/kernel/perf_event_max_sample_rate;

%.o : %.c
	sudo cp lib/*.h /usr/include; \
	gcc $(CFLAGS) -o $@ -c $<;

module : 
	pushd module; \
	make clean; \
	make; \
	./install.sh;

clean : 
	rm ./lib/*.o ./lib/*.so; \
	cd module; \
	make clean; \
	cd -;
