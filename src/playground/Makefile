CC=gcc
CFLAGS=-I.
LDFLAGS = -laccel-config
DEPS = dsa.h
SRCS = 

run: $(TARGET).c
	$(CC) $(TARGET).c -o $(TARGET) $(CFLAGS) $(LDFLAGS)
	sudo setcap cap_sys_rawio+ep ./$(TARGET)
	./$(TARGET)

.PHONY: run
