

.PHONY: build macos linux-alsa linux-pulse linux-jack linux-all clean 

all: build

linux-alsa:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALSA=ON && make

linux-pulse:
	@mkdir -p build && cd build && cmake .. -DLINUX_PULSE=ON && make

linux-jack:
	@mkdir -p build && cd build && cmake .. -DLINUX_JACK=ON && make

linux-all:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALL=ON && make

macos: build

build:
	@mkdir -p build && cd build && cmake .. && make

clean:
	@rm -rf build
	@rm chuck~/chuck~.pd_*
