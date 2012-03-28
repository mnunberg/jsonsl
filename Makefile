LIBJSONSL_DIR=$(shell pwd)
LDFLAGS=-L$(LIBJSONSL_DIR) -Wl,-rpath $(LIBJSONSL_DIR) -ljsonsl $(PROFILE)
CFLAGS=\
	   -Wall -std=gnu89 -pedantic \
	   -O3 -ggdb3 \
	   -I$(LIBJSONSL_DIR) -DJSONSL_STATE_GENERIC

export CFLAGS
export LDFLAGS

all: json_test libjsonsl.so

.PHONY: examples
examples:
	$(MAKE) -C $@


json_test: json_test.c libjsonsl.so
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

jpr_test: jpr_test.c libjsonsl.so
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

share: json_samples.tgz
	tar xzf $^

json_examples_tarball:
	rm -f json_samples.tgz
	tar -czf json_samples.tgz share

check: json_test share jpr_test
	JSONSL_QUIET_TESTS=1 ./json_test share/*
	JSONSL_QUIET_TESTS=1 ./jpr_test

libjsonsl.so: jsonsl.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^

.PHONY: doc


doc: Doxyfile
	doxygen -s $^


.PHONY: clean
clean:
	rm -f *.o json_test *.so jpr_test
	rm -f -r share
	rm -f -r *.dSYM
	$(MAKE) -C examples clean

distclean: clean
	rm -rf share doc *.out
