CC=gcc
CFLAGS=-O3 $(OPS) -I/usr/local/include $(SIMFLAGS) -g #-fsanitize=address
.DEFAULT_GOAL:=all
LDFLAGS+=-lm -lperf_counters

helpers.o: helpers.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)

util.o: util.c
	$(CC) $(CFLAGS) -c $< $(LDFLAGS)

get_slice_mapping: get_slice_mapping.c util.o helpers.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

example_hash_function_usage: example_hash_function_usage.c
	$(CC) $(CFLAGS) $^ -o $@

all: get_slice_mapping example_hash_function_usage

clean:
	rm -rf get_slice_mapping example_hash_function_usage *.a *.o
