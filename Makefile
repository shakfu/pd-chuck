VERSION := $(shell sed -n 's/^project(pd_chuck VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)
PLATFORM = $(shell uname -s)
ARCH = $(shell uname -m)
DIST = build/dist/pd-chuck
THIRDPARTY = $(PWD)/build/thirdparty
PREFIX = $(THIRDPARTY)/install
# FAUST_VERSION = 2.72.14
FAUST_VERSION = 2.69.3

# Test timeout in seconds (default: 10)
TEST_TIMEOUT ?= 10
# Use gtimeout on macOS (from coreutils), timeout on Linux
ifeq ($(PLATFORM),Darwin)
    TIMEOUT_CMD = gtimeout
else
    TIMEOUT_CMD = timeout
endif


.PHONY: all build \
		full light nomp3 build_fs build_pd \
		macos macos-base-native macos-base-universal macos-adv-brew  \
		linux linux-base-alsa linux-base-pulse linux-base-jack linux-base-all \
		linux-adv-alsa linux-adv-pulse linux-adv-jack linux-adv-all \
		faust rubberband libsndfile_formats \
		all_deps light_deps nomp3_deps \
		test test-audio test-bind test-reply test-faust test-warpbuf test-pdpatch probe-chugins \
		clean reset sign linux-base-fs

all: build

build:
	@mkdir -p build && \
		cd build && \
		cmake .. && \
		cmake --build . --config Release

build_fs:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_HOMEBREW=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			&& \
		cmake --build . --config Release

build_pd:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DENABLE_PDPATCH=ON \
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

libsndfile: libsndfile_formats
	@./scripts/deps/install_libsndfile.sh

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
			-DENABLE_FAUCK=ON \
			-DENABLE_FLUIDSYNTH=ON \
			&& \
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
			-DLINUX_ALSA=ON \
			-DENABLE_FLUIDSYNTH=ON \
			&& \
		cmake --build . --config Release

linux-base-pulse:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_PULSE=ON \
			-DENABLE_FLUIDSYNTH=ON \
			&& \
		cmake --build . --config Release

linux-base-jack:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_JACK=ON \
			-DENABLE_FLUIDSYNTH=ON \
			&& \
		cmake --build . --config Release

linux-base-all:
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_ALL=ON \
			-DENABLE_FLUIDSYNTH=ON \
			&& \
		cmake --build . --config Release

linux-adv-alsa: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_ALSA=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-pulse: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_PULSE=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-jack: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_JACK=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_FAUCK=ON && \
		cmake --build . --config Release

linux-adv-all: faust
	@mkdir -p build && \
		cd build && \
		cmake .. \
			-DLINUX_ALL=ON \
			-DENABLE_EXTRA_FORMATS=ON \
			-DENABLE_MP3=ON \
			-DENABLE_WARPBUF=ON \
			-DENABLE_FLUIDSYNTH=ON \
			-DENABLE_FAUCK=ON && \
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

test: test-audio test-bind

test-audio:
	@$(TIMEOUT_CMD) $(TEST_TIMEOUT) pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_audio.pd || true

test-reply:
	@$(TIMEOUT_CMD) 20 pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_reply.pd 2>&1 | grep -q "REPLY: val counter" && echo "OK: reply outlet works" || (echo "FAIL: reply outlet"; exit 1)

# Host.msg, HostIn, HostBuffer and HostBufPlay; -batch runs DSP as fast as possible
test-bind:
	@out=$$($(TIMEOUT_CMD) 30 pd -batch -nogui -noprefs -open chuck_tilde/tests/test_bind.pd 2>&1); \
	if echo "$$out" | grep -q "BIND-DONE: 0" && ! echo "$$out" | grep -q "BIND-FAIL" \
		&& [ "$$(echo "$$out" | grep -c "HostBufPlay: channel 2 plays silence")" = 1 ] \
		&& ! echo "$$out" | grep -q "HostBufPlay: channel 1" \
		&& echo "$$out" | grep -qF "BIND-MBANG: bang" && echo "$$out" | grep -qF "BIND-MSYM: hello" \
		&& echo "$$out" | grep -qF "BIND-MLIST: 1 2.5 -3" && echo "$$out" | grep -qF "BIND-MEMPTY: bang"; then \
		echo "OK: bind tests passed ($$(echo "$$out" | grep -c BIND-PASS) checks, Host.msg overloads)"; \
	else echo "$$out"; echo "FAIL: bind tests"; exit 1; fi
	@# receivers per 'msg w': before/after removeall, rerun, after reset, rerun
	@got=$$($(TIMEOUT_CMD) 30 pd -batch -nogui -noprefs -open chuck_tilde/tests/test_bind_lifecycle.pd 2>&1 \
		| sed -n 's/.*msg w: delivered to \([0-9]*\) HostIn/\1/p' | tr '\n' ' '); \
	if [ "$$got" = "2 0 2 0 2 " ]; then echo "OK: HostIn lifecycle"; \
	else echo "FAIL: HostIn lifecycle: got '$$got', want '2 0 2 0 2 '"; exit 1; fi
	@# Host.send* without 'reply 1': each type once, then 300 floats in one block (256 fit)
	@out=$$($(TIMEOUT_CMD) 30 pd -batch -nogui -noprefs -open chuck_tilde/tests/test_bind_send.pd 2>&1); \
	n=$$(echo "$$out" | grep -c "BIND-N:"); ok=1; \
	for want in "BIND-SENDFLOAT: 7" "BIND-BANG: bang" "BIND-SYMBOL: symbol hello" \
		"BIND-LIST: 1 2.5 -3" "BIND-LEMPTY: bang" "BIND-LLEN: 32" \
		"1 lists cut to 32 elements" "44 messages dropped"; do \
		echo "$$out" | grep -qF "$$want" || { echo "missing: $$want"; ok=0; }; \
	done; \
	if [ $$ok = 1 ] && [ "$$n" = 256 ]; then echo "OK: Host.send{Bang,Float,Symbol,List}"; \
	else echo "$$out" | grep "BIND\|warning" | tail; echo "FAIL: Host.send* ($$n of 256)"; exit 1; fi
	@# HostClock.ck driven by timed 'msg clock ...' messages
	@out=$$($(TIMEOUT_CMD) 30 pd -r 44100 -batch -nogui -noprefs -open chuck_tilde/tests/test_bind_clock.pd 2>&1); \
	if echo "$$out" | grep -q "BIND-CLOCK-DONE: 0" && ! echo "$$out" | grep -q "BIND-CLOCK-FAIL" \
		&& echo "$$out" | grep -q "HostClock: tempo must be > 0"; then \
		echo "OK: HostClock.ck ($$(echo "$$out" | grep -c BIND-CLOCK-PASS) checks)"; \
	else echo "$$out" | grep "BIND-CLOCK\|HostClock\|error"; echo "FAIL: HostClock.ck"; exit 1; fi

test-faust:
	@$(TIMEOUT_CMD) $(TEST_TIMEOUT) pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_faust.pd || true

test-warpbuf:
	@$(TIMEOUT_CMD) $(TEST_TIMEOUT) pd -nogui -send "pd dsp 1" -open chuck_tilde/tests/test_warpbuf.pd || true

test-pdpatch:
	@$(TIMEOUT_CMD) $(TEST_TIMEOUT) ./build/chuck --silent --chugin:chuck_tilde/examples/chugins/PdPatch.chug thirdparty/chugins/PdPatch/examples/test-pdpatch.ck 2>&1 | tee /dev/stderr | grep -q "OK:.*tests passed"

probe-chugins:
	@./build/chuck --chugin-probe --chugin-path:chuck_tilde/examples/chugins
