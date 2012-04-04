TESTMODS= json_test jpr_test unescape

all: $(TESTMODS)
	./json_test ../share/*
	./jpr_test
	./unescape
	./json_test ../share/jsc/pass*.json
	JSONSL_FAIL_TESTS=1 ./json_test ../share/jsc/fail*.json
	@echo "All Tests OK"
	@echo "These files were skipped. See tests/jsc/nyi_fail for more info"
	@ls ../share/jsc/nyi_fail/

%: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTMODS)
