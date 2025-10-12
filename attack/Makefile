CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = $(wildcard *.c)  
EXES = $(patsubst %.c, %, $(SRCS))

covert:
	mkdir -p build
	$(CC) cc_sender.c -o build/cc_sender $(CFLAGS) $(LDFLAGS)
	$(CC) cc_receiver.c -o build/cc_receiver $(CFLAGS) $(LDFLAGS)
	$(CC) cc_sworkq.c -o build/cc_sworkq $(CFLAGS) $(LDFLAGS)
	$(CC) cc_rworkq.c -o build/cc_rworkq $(CFLAGS) $(LDFLAGS)
	sudo setcap cap_sys_rawio+ep ./build/*

spy:
	mkdir -p build
	$(CC) atc_spy.c -o build/atc_spy $(CFLAGS) $(LDFLAGS)
	$(CC) wq_spy.c -o build/wq_spy $(CFLAGS) $(LDFLAGS)
	sudo setcap cap_sys_rawio+ep ./build/*

run: build
	./build/$(TARGET)

debug: $(TARGET).c build
	gdb ./build/$(TARGET)

build: $(TARGET).c
	mkdir -p build
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: run clean build covert