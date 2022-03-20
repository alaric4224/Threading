override CFLAGS := -Wall -Werror -std=gnu99 -O0 -g $(CFLAGS) -I.

# Build the threads.o file
threads.o: threads.c ec440threads.h

test_busy_threads.o: test_busy_threads.c ec440threads.h

lock_test_1.o: lock_test_1.c ec440threads.h

barrier_test_1.o: barrier_test_1.c ec440threads.h

test_busy_threads: test_busy_threads.o threads.o
	$(CC) $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

lock_test_1: lock_test_1.o threads.o
	$(CC) $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@

barrier_test_1: barrier_test_1.o threads.o
	$(CC) $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS) -o $@


test_files= lock_test_1 barrier_test_1

# Run the test programs
check: $(test_files) 
	bash run_tests.sh $(test_files) 

clean:
	rm -f *.o $(test_files) *~
