CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = $(wildcard *.c)  
EXES = $(patsubst %.c, %, $(SRCS))

buildall: $(EXES)

build: $(TARGET).c 
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS)

run: $(TARGET).c build
	sudo ./build/$(TARGET)

debug: $(TARGET).c
	$(CC) $(TARGET).c -o build/$(TARGET) $(CFLAGS) $(LDFLAGS) -g
	sudo gdb ./build/$(TARGET)

$(EXES):%:%.c $(DEPS)
	$(CC) -o build/$@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf build

.PHONY: run clean build
