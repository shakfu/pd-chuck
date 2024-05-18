

.PHONY: build macos linux-alsa linux-pulse linux-jack linux-all full light clean \
		install_all_deps install_min_deps

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

install_all_deps:
	./scripts/install_deps.sh

full: install_all_deps
	@mkdir -p build && cd build && cmake .. -DSNDFILE_MAX=ON -DENABLE_WARPBUF=ON -DENABLE_FAUCK=ON && cmake --build . --config Release

install_min_deps:
	./scripts/install_min_deps.sh

light: install_min_deps
	@mkdir -p build && cd build && cmake .. -DSNDFILE_MAX=OFF -DENABLE_WARPBUF=ON -DENABLE_FAUCK=ON && cmake --build . --config Release

clean:
	@rm -rf build
	@rm -f chuck_tilde/chuck_tilde.pd_*
