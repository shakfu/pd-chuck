PLATFORM = $(shell uname -s)
ARCH = $(shell uname -m)
DIST = build/dist/pd-chuck
THIRDPARTY = $(PWD)/build/thirdparty
PREFIX = $(THIRDPARTY)/install
# FAUST_VERSION = 2.72.14
FAUST_VERSION = 2.69.3


.PHONY: all build \
		full light nomp3 build_fs \
		macos macos-base-native macos-base-universal macos-adv-brew  \
		linux linux-base-alsa linux-base-pulse linux-base-jack linux-base-all \
		linux-adv-alsa linux-adv-pulse linux-adv-jack linux-adv-all \
		faust rubberband libsndfile_formats install_sf2 \
		all_deps light_deps nomp3_deps \
		test test-audio test-faust test-warpbuf probe-chugins \
		clean reset sign

all: build

build:
	@mkdir -p build && \
		cd build && \
		cmake .. && \
		cmake --build . --config Release

build_fs: install_sf2
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_HOMEBREW=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			&& \
		cmake --build . --config Release

faust:
	@mkdir -p $(PREFIX)/include && \
	mkdir -p $(PREFIX)/lib && \
	./scripts/deps/install_faust.sh $(FAUST_VERSION) && \
	./scripts/deps/install_libfaust.sh $(FAUST_VERSION)

rubberband:
	@./scripts/deps/install_rubberband.sh && \
	./scripts/deps/install_libsamplerate.sh

libsndfile_formats:
	@./scripts/deps/install_libflac.sh && \
	./scripts/deps/install_libogg.sh && \
	./scripts/deps/install_libvorbis.sh && \
	./scripts/deps/install_libopus.sh

all_deps: faust rubberband libsndfile_formats
	@./scripts/deps/install_libmpg123.sh && \
	./scripts/deps/install_libmp3lame.sh && \
	./scripts/deps/install_libsndfile.sh

light_deps: faust rubberband
	@./scripts/deps/install_libmpg123_gyp.sh && \
	./scripts/deps/install_libsndfile_light.sh

nomp3_deps: faust rubberband libsndfile_formats
	@./scripts/deps/install_libmpg123_gyp.sh && \
	./scripts/deps/install_libsndfile_nomp3.sh

install_sf2:
	@./scripts/download_sf2.sh

macos: macos-adv-brew

macos-base-native: build

macos-base-universal:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DBUILD_UNIVERSAL=ON && \
		cmake --build . --config Release

macos-adv-brew: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_HOMEBREW=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

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

linux: linux-adv-alsa

linux-base-alsa:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_ALSA=ON && \
		cmake --build . --config Release

linux-base-pulse:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_PULSE=ON && \
		cmake --build . --config Release

linux-base-jack:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_JACK=ON && \
		cmake --build . --config Release

linux-base-all:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_ALL=ON && \
		cmake --build . --config Release

linux-adv-alsa: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DDYNAMIC_LINKING=ON \
			-DLINUX_ALSA=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-pulse: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DDYNAMIC_LINKING=ON \
			-DLINUX_PULSE=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-jack: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DDYNAMIC_LINKING=ON \
			-DLINUX_JACK=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-all: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DDYNAMIC_LINKING=ON \
			-DLINUX_ALL=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FAUCK=O && \
		cmake --build . --config Release

package:
	@rm -rf $(DIST) && \
		mkdir -p $(DIST) && \
		cp -af chuck_tilde $(DIST)/chuck_tilde && \
		cp docs/cheatsheet.pdf $(DIST)/cheatsheet.pdf && \
		cp -f LICENSE $(DIST)/LICENSE && \
		cp -f CHANGELOG.md $(DIST)/CHANGELOG.md && \
		cp -f README.md $(DIST)/README.md && \
		rm -f $(DIST)/chuck_tilde/examples/chuck && \
		rm -f $(DIST)/chuck_tilde/CMakeLists.txt && \
		rm -f $(DIST)/chuck_tilde/*.cpp && \
		rm -f $(DIST)/chuck_tilde/*.h && \
		find $(DIST) -name ".DS_Store" -delete && \
		echo "DONE"


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

test: test-audio

test-audio:
	@pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_audio.pd

test-faust:
	@pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_faust.pd

test-warpbuf:
	@pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_warpbuf.pd

probe-chugins:
	@./build/chuck --chugin-probe --chugin-path:chuck_tilde/examples/chugins
