

.PHONEY: build clean

all: build


build:
	@mkdir build && cd build && cmake .. && make

clean:
	@rm -rf build
	@rm chuck~/chuck~.pd_darwin

