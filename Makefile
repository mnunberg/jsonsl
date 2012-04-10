LIBJSONSL_DIR+=$(shell pwd)
LDFLAGS+=-L$(LIBJSONSL_DIR) -Wl,-rpath $(LIBJSONSL_DIR)
CFLAGS+=\
	   -Wall -std=gnu89 -pedantic \
	   -O3 -ggdb3 \
	   -I$(LIBJSONSL_DIR) -DJSONSL_STATE_GENERIC \

export CFLAGS
export LDFLAGS

LIB_BASENAME=jsonsl
LIB_PREFIX?=lib
LIB_SUFFIX?=.so
LIB_FQNAME = $(LIB_PREFIX)$(LIB_BASENAME)$(LIB_SUFFIX)

ifdef STATIC_LIB
	LDFLAGS+=$(shell pwd)/$(LIB_FQNAME)
	LIBFLAGS=-c
else
	LDFLAGS+=-l$(LIB_BASENAME)
	LIBFLAGS=-shared -fPIC
endif

all: $(LIB_FQNAME)

.PHONY: examples
examples:
	$(MAKE) -C $@

share: json_samples.tgz
	tar xzf $^

json_examples_tarball:
	rm -f json_samples.tgz
	tar -czf json_samples.tgz share

check: $(LIB_FQNAME) share jsonsl.c
	JSONSL_QUIET_TESTS=1 $(MAKE) -C tests

bench:
	$(MAKE) -C perf run-benchmarks

$(LIB_FQNAME): jsonsl.c
	$(CC) $(CFLAGS) $(LIBFLAGS) -o $@ $^

.PHONY: doc


doc: Doxyfile
	doxygen -s $^


.PHONY: clean
clean:
	rm -f *.o *.so *.a
	rm -f $(LIB_FQNAME)
	rm -f -r share
	rm -f -r *.dSYM
	$(MAKE) -C examples clean
	$(MAKE) -C tests clean
	$(MAKE) -C perf clean

distclean: clean
	rm -rf share doc *.out

dist:
	-rm -f jsonsl.tar.gz
	xargs < MANIFEST tar czf jsonsl.tar.gz
