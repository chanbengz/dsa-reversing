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
	# $(CC) cc_workq.c -o build/cc_workq $(CFLAGS) $(LDFLAGS)

spy:
	mkdir -p build
	$(CC) atc_spy.c -o build/atc_spy $(CFLAGS) $(LDFLAGS)
	$(CC) wq_spy.c -o build/wq_spy $(CFLAGS) $(LDFLAGS)

run: build
	./build/$(TARGET)

debug: $(TARGET).c build
	gdb ./build/$(TARGET)

build: $(TARGET).c
	mkdir -p build
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS) -g
	# sudo setcap cap_sys_rawio+ep ./build/$(TARGET)
	# disable this for now because it's not needed for the attack

clean:
	rm -rf build

.PHONY: run clean build covert