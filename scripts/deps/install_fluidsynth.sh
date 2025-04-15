# install full stack dependencies to build the WarpBuf and Fauck chugins

CWD=`pwd`
THIRDPARTY=${CWD}/build/thirdparty
PREFIX=${THIRDPARTY}/install
REPO=https://github.com/FluidSynth/fluidsynth.git
VERSION=v2.4.5

function setup() {
	mkdir -p ${PREFIX}/include && \
	mkdir -p ${PREFIX}/lib
}


function install_fluidsynth() {
	SRC=${THIRDPARTY}/fluidsynth
	if [ ! -f ${THIRDPARTY}/install/lib/libfluidsynth.a ]; then
		rm -rf ${THIRDPARTY}/fluidsynth && \
		mkdir -p ${THIRDPARTY} && \
		git clone --depth=1 -b ${VERSION} ${REPO} ${THIRDPARTY}/fluidsynth && \
		cd ${SRC} && \
		mkdir build && cd build && \
		cmake .. \
			-DBUILD_SHARED_LIBS=OFF \
			-Denable-framework=OFF \
			-Denable-alsa=OFF \
			-Denable-aufile=OFF \
			-Denable-dbus=OFF \
			-Denable-ipv6=OFF \
			-Denable-jack=OFF \
			-Denable-ladspa=OFF \
			-Denable-libinstpatch=OFF \
			-Denable-libsndfile=OFF \
			-Denable-midishare=OFF \
			-Denable-opensles=OFF \
			-Denable-oboe=OFF \
			-Denable-network=OFF \
			-Denable-osse=OFF \
			-Denable-dsound=OFF \
			-Denable-wasapi=OFF \
			-Denable-waveout=OFF \
			-Denable-winmidi=OFF \
			-Denable-sdl2=OFF \
			-Denable-sdl3=OFF \
			-Denable-pulseaudio=OFF \
			-Denable-pipewire=OFF \
			-Denable-readline=OFF \
			-Denable-threads=OFF \
			-Denable-openmp=OFF \
			-Denable-unicode=OFF \
			-DCMAKE_INSTALL_PREFIX=${PREFIX} \
			&& \
		cmake --build . --config Release && \
		cmake --install . --prefix ${PREFIX}
	fi
}

setup && install_fluidsynth





