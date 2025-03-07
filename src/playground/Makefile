CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = $(wildcard *.c)  
EXES = $(patsubst %.c, %, $(SRCS))

build: $(EXES)

run: $(TARGET).c
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS)
	sudo setcap cap_sys_rawio+ep ./build/$(TARGET)
	./build/$(TARGET)

$(EXES):%:%.c $(DEPS)
	$(CC) -o build/$@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: run clean build
