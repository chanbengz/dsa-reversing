CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = $(wildcard *.c)  
EXES = $(patsubst %.c, %, $(SRCS))

all: $(EXES)
	sudo setcap cap_sys_rawio+ep ./build/*

build: $(TARGET).c builddir
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS)
	sudo setcap cap_sys_rawio+ep ./build/$(TARGET)

run: $(TARGET).c build
	sudo ./build/$(TARGET)

debug: $(TARGET).c
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS) -g
	sudo gdb ./build/$(TARGET)

$(EXES):%:%.c $(DEPS) builddir
	$(CC) -o build/$@ $< $(CFLAGS) $(LDFLAGS)

builddir:
	mkdir -p build

clean:
	rm -rf build

.PHONY: run clean build all builddir debug
