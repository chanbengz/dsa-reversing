PROFILE ?= debug

.PHONY: clean build run

build:
	cargo build --example $(TARGET)
	sudo setcap cap_sys_admin,cap_sys_rawio+ep target/$(PROFILE)/examples/$(TARGET)

run:
	./target/$(PROFILE)/examples/$(TARGET)

clean:
	cargo clean
