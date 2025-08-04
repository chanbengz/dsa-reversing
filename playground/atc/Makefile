CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = $(wildcard *.c)  
EXES = $(patsubst %.c, %, $(SRCS))

buildall: $(EXES)

run: $(TARGET).c build
	sudo ./build/$(TARGET)

debug: $(TARGET).c build
	sudo gdb ./build/$(TARGET)

build: $(TARGET).c
	mkdir -p build
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS) -g
	sudo setcap cap_sys_rawio+ep ./build/$(TARGET)

$(EXES):%:%.c $(DEPS)
	$(CC) -o build/$@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: run clean build
