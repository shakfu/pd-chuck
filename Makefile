
.PHONY: build clean linux-alsa linux-pulse linux-jack linux-all

all: build

linux-alsa:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALSA=ON && make

linux-pulse:
	@mkdir -p build && cd build && cmake .. -DLINUX_PULSE=ON && make

linux-jack:
	@mkdir -p build && cd build && cmake .. -DLINUX_JACK=ON && make

linux-all:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALL=ON && make

build:
	@mkdir -p build && cd build && cmake .. && make

clean:
	@rm -rf build
	@rm chuck~/chuck~.pd_*

