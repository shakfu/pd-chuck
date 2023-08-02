
.PHONY: build clean

all: build

build:
	@mkdir -p build && cd build && cmake .. && make

clean:
	@rm -rf build
	@rm chuck~/chuck~.pd_darwin

