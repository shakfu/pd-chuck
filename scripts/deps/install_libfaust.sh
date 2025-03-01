#!/usr/bin/env bash

# install full stack dependencies to build the WarpBuf and Fauck chugins

CWD=`pwd`
BUILD=${CWD}/build
THIRDPARTY=${BUILD}/thirdparty
PREFIX=${THIRDPARTY}/install
DOWNLOADS=${BUILD}/downloads
FAUST_VERSION=$1

function setup {
	mkdir -p ${DOWNLOADS} && \
	mkdir -p ${PREFIX}/include && \
	mkdir -p ${PREFIX}/lib
}


function install_libfaust {
	VERSION=${FAUST_VERSION}
	if [ "$(uname)" = "Darwin" ]; then
		echo "You are running macOS"
	    if [ ! -f ${THIRDPARTY}/libfaust/lib/libfaustwithllvm.a ]; then
	    	rm -rf ${THIRDPARTY}/libfaust
			if [ "$(uname -m)" = "arm64" ]; then
				if [ ! -f ${DOWNLOADS}/Faust-$VERSION-arm64.dmg ]; then
					curl -L https://github.com/grame-cncm/faust/releases/download/$VERSION/Faust-$VERSION-arm64.dmg -o ${DOWNLOADS}/Faust-$VERSION-arm64.dmg
				fi
				hdiutil attach ${DOWNLOADS}/Faust-$VERSION-arm64.dmg
				mkdir -p ${THIRDPARTY}/libfaust
				cp -Rf /Volumes/Faust-$VERSION/Faust-$VERSION/* ${THIRDPARTY}/libfaust/
				hdiutil detach /Volumes/Faust-$VERSION/
				# rm -f Faust-$VERSION-arm64.dmg
			else
				if [ ! -f ${DOWNLOADS}/Faust-$VERSION-x64.dmg ]; then
					curl -L https://github.com/grame-cncm/faust/releases/download/$VERSION/Faust-$VERSION-x64.dmg -o ${DOWNLOADS}/Faust-$VERSION-x64.dmg
				fi
				hdiutil attach ${DOWNLOADS}/Faust-$VERSION-x64.dmg
				mkdir -p ${THIRDPARTY}/libfaust
				cp -Rf /Volumes/Faust-$VERSION/Faust-$VERSION/* ${THIRDPARTY}/libfaust/
				hdiutil detach /Volumes/Faust-$VERSION/
				# rm -f Faust-$VERSION-x64.dmg
			fi
		fi
	elif [ "$(expr substr $(uname -s) 1 5)" = "Linux" ]; then
	    echo "You are running Linux"
	    if [ ! -f ${THIRDPARTY}/libfaust/lib/libfaustwithllvm.a ]; then
	    	rm -rf ${THIRDPARTY}/libfaust
		    if [ ! -f ${DOWNLOADS}/libfaust-ubuntu-x86_64.zip ]; then
		        curl -L https://github.com/grame-cncm/faust/releases/download/$VERSION/libfaust-ubuntu-x86_64.zip -o ${DOWNLOADS}/libfaust-ubuntu-x86_64.zip
		    fi
			mkdir -p ${THIRDPARTY}/libfaust
	        unzip ${DOWNLOADS}/libfaust-ubuntu-x86_64.zip -d ${THIRDPARTY}/libfaust/		    
		fi
	elif [ "$(expr substr $(uname -s) 1 10)" = "MINGW32_NT" ] || [ "$(expr substr $(uname -s) 1 10)" = "MINGW64_NT" ]; then
	    echo "You are running Windows. You should run \"call install_libfaust.bat\"" >&2
	    exit 1
	else
	    echo "Unknown operating system" >&2
	    exit 1
	fi
}


setup && install_libfaust