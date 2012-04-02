TESTMODS= json_test jpr_test unescape

all: $(TESTMODS)
	./json_test ../share/*
	./jpr_test
	./unescape

%: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTMODS)
