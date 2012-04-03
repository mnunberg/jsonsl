LIBJSONSL_DIR=$(shell pwd)
LDFLAGS=-L$(LIBJSONSL_DIR) -Wl,-rpath $(LIBJSONSL_DIR) -ljsonsl $(PROFILE)
CFLAGS=\
	   -Wall -std=gnu89 -pedantic \
	   -O3 -ggdb3 \
	   -I$(LIBJSONSL_DIR) -DJSONSL_STATE_GENERIC

export CFLAGS
export LDFLAGS

all: libjsonsl.so

.PHONY: examples
examples:
	$(MAKE) -C $@

share: json_samples.tgz
	tar xzf $^

json_examples_tarball:
	rm -f json_samples.tgz
	tar -czf json_samples.tgz share

check: libjsonsl.so share
	JSONSL_QUIET_TESTS=1 $(MAKE) -C tests

perf/bench: perf/bench.c jsonsl.c
	$(CC) $(CFLAGS) -o $@ $^

bench: perf/bench
	@echo "Running benchmark"
	time -p ./perf/bench share/auction 100

libjsonsl.so: jsonsl.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^

.PHONY: doc


doc: Doxyfile
	doxygen -s $^


.PHONY: clean
clean:
	rm -f *.o *.so
	rm -f -r share
	rm -f -r *.dSYM
	rm -f perf/bench
	$(MAKE) -C examples clean
	$(MAKE) -C tests clean

distclean: clean
	rm -rf share doc *.out
