# install full stack dependencies to build the WarpBuf and Fauck chugins

CWD=`pwd`
THIRDPARTY=${CWD}/build/thirdparty
PREFIX=${THIRDPARTY}/install


function setup() {
	mkdir -p ${PREFIX}/include && \
	mkdir -p ${PREFIX}/lib
}

function install_libmpg123() {
	SRC=${THIRDPARTY}/libmpg123
	BUILD=${THIRDPARTY}/libmpg123/build
	if [ ! -f ${THIRDPARTY}/install/lib/libmpg123.a ]; then
		rm -rf ${THIRDPARTY}/libmpg123 && \
		mkdir -p build/thirdparty && \
		git clone --depth=1 https://github.com/gypified/libmpg123.git ${THIRDPARTY}/libmpg123 && \
		cd ${THIRDPARTY}/libmpg123 && \
		CFLAGS="-Os -s" ./configure \
			--with-cpu=generic  \
			--disable-id3v2 \
			--disable-lfs-alias \
			--disable-feature-report \
			--with-seektable=0 \
			--disable-16bit \
			--disable-32bit \
			--disable-8bit \
			--disable-messages \
			--disable-feeder \
			--disable-ntom \
			--disable-downsample \
			--disable-icy \
			--enable-static \
			--prefix=${PREFIX} && \
		make && \
		make install 
	fi
}

setup && install_libmpg123
