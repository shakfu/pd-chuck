PLATFORM = $(shell uname -s)
ARCH = $(shell uname -m)

THIRDPARTY = $(PWD)/build/thirdparty
PREFIX = $(THIRDPARTY)/install
FAUST_VERSION = 2.72.14
#FAUST_VERSION = 2.69.3




.PHONY: full light nomp3 \
		macos-native macos-universal macos-brew \
		linux-alsa linux-pulse linux-jack linux-all \
		faust rubberband libsndfile_formats \
		all_deps light_deps deps_nomp3 \
		clean reset sign
		

all: build


build:
	@mkdir -p build && cd build && cmake .. && cmake --build . --config Release



faust:
	mkdir -p $(PREFIX)/include && \
	mkdir -p $(PREFIX)/lib && \
	./scripts/deps/install_faust.sh $(FAUST_VERSION) && \
	./scripts/deps/install_libfaust.sh $(FAUST_VERSION)

rubberband:
	./scripts/deps/install_rubberband.sh && \
	./scripts/deps/install_libsamplerate.sh


libsndfile_formats:
	./scripts/deps/install_libflac.sh && \
	./scripts/deps/install_libogg.sh && \
	./scripts/deps/install_libvorbis.sh && \
	./scripts/deps/install_libopus.sh

all_deps: faust rubberband libsndfile_formats
	./scripts/deps/install_libmpg123.sh && \
	./scripts/deps/install_libmp3lame.sh && \
	./scripts/deps/install_libsndfile.sh

light_deps: faust rubberband
	./scripts/deps/install_libmpg123_gyp.sh && \
	./scripts/deps/install_libsndfile_light.sh

nomp3_deps: faust rubberband libsndfile_formats
	./scripts/deps/install_libmpg123_gyp.sh && \
	./scripts/deps/install_libsndfile_nomp3.sh


full: all_deps
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

light: light_deps
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_EXTRA_FORMATS=OFF \
			-DENABLE_MP3=OFF \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

nomp3: nomp3_deps
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=OFF \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release


linux-alsa:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALSA=ON && cmake --build . --config Release

linux-pulse:
	@mkdir -p build && cd build && cmake .. -DLINUX_PULSE=ON && cmake --build . --config Release

linux-jack:
	@mkdir -p build && cd build && cmake .. -DLINUX_JACK=ON && cmake --build . --config Release

linux-all:
	@mkdir -p build && cd build && cmake .. -DLINUX_ALL=ON && cmake --build . --config Release

macos-native: build

macos-universal:
	@mkdir -p build && cd build && cmake .. -DBUILD_UNIVERSAL=ON && cmake --build . --config Release

macos-brew: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_HOMEBREW=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release


sign:
	@codesign --force --sign - chuck_tilde/chuck\~.pd_darwin
	@codesign --force --sign - chuck_tilde/examples/chugins/*.chug

clean:
	@rm -rf build
	@rm -f chuck_tilde/chuck_tilde.pd_*

reset:
	@rm -rf chuck_tilde/chuck\~.pd_darwin chuck_tilde/examples/chugins/*.chug
	@rm -rf build/CMakeCache.txt build/Makefile build/CMakeFiles build/chuck_tilde build/chuck 
	@rm -rf build/cmake_install.cmake build/thirdparty build/*.a

