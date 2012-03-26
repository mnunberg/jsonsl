LIBJSONSL_DIR=$(shell pwd)
LDFLAGS=-L$(LIBJSONSL_DIR) -Wl,-rpath=$(LIBJSONSL_DIR) -ljsonsl
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

share: json_samples.tgz
	tar xzf $^

check: json_test share
	JSONSL_QUIET_TESTS=1 ./json_test share/*

libjsonsl.so: jsonsl.c
	$(CC) $(CFLAGS) -g -ggdb3 -shared -fPIC -o $@ $^

clean:
	rm -f *.o json_test *.so
	rm -r share
