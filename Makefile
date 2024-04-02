

.PHONY: build macos linux-alsa linux-pulse linux-jack linux-all clean 

all: build

linux-alsa:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALSA=ON && cmake --build . --config Release

linux-pulse:
	@mkdir -p build && cd build && cmake .. -DLINUX_PULSE=ON && cmake --build . --config Release

linux-jack:
	@mkdir -p build && cd build && cmake .. -DLINUX_JACK=ON && cmake --build . --config Release

linux-all:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALL=ON && cmake --build . --config Release

macos: build

build:
	@mkdir -p build && cd build && cmake .. && cmake --build . --config Release

clean:
	@rm -rf build
	@rm chuck~/chuck~.pd_*
