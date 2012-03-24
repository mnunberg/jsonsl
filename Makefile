
all: json_test libjsonsl.so

CFLAGS=-Wall -ggdb3 -O0

json_test: json_test.c libjsonsl.so
	$(CC) $(CFLAGS) $< -o $@ -I. -L. -Wl,-rpath=$(shell pwd) -ljsonsl

libjsonsl.so: jsonsl.c
	$(CC) -g -ggdb3 -shared -fPIC -o $@ $^

clean:
	rm -f *.o json_test *.so

