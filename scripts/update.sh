# update.sh
#
# This script git clones latest version of chuck and chugins

CHUCK_TILDE=chuck~/chuck~.cpp
CHUCK_REPO=https://github.com/ccrma/chuck.git
CHUGINS_REPO=https://github.com/ccrma/chugins.git
RPL="rpl --match-case \
		 --whole-words \
		 --verbose \
		 --fixed-strings"


function replace() {
	src=$1
	dst=$2
	target_file=$3
	rpl --match-case \
		--whole-words \
		--verbose \
		--fixed-strings $src $dst $target_file
}

function update_chuck() {
	git clone ${CHUCK_REPO} chuck-src && \
	mkdir -p thirdparty/chuck-new && \
	mv chuck-src/src/core thirdparty/chuck-new/ && \
	mv chuck-src/src/host thirdparty/chuck-new/ && \
	rm -rf chuck-src && \
	cp thirdparty/chuck/core/CMakeLists.txt thirdparty/chuck-new/core/ && \
	cp thirdparty/chuck/host/CMakeLists.txt thirdparty/chuck-new/host/ && \
	mv thirdparty/chuck thirdparty/chuck-old && \
	mv thirdparty/chuck-new thirdparty/chuck
}



function move_to_new() {
	mv chugins-src/$1 thirdparty/chugins-new/$1
}

function update_new_chugin() {
	move_to_new $1 && \
	cp thirdparty/chugins/$1/CMakeLists.txt thirdparty/chugins-new/$1 && \
	rm -rf thirdparty/chugins-new/$1/makefile* && \
	rm -rf thirdparty/chugins-new/$1/*.dsw && \
	rm -rf thirdparty/chugins-new/$1/*.dsp && \
	rm -rf thirdparty/chugins-new/$1/*.xcodeproj && \
	rm -rf thirdparty/chugins-new/$1/*.vcxproj
}



function update_chugins() {
	git clone ${CHUGINS_REPO} chugins-src && \
	mkdir -p thirdparty/chugins-new && \
	cp thirdparty/chugins/CMakeLists.txt thirdparty/chugins-new/ && \
	# non-chugins
	move_to_new chuck && \
	move_to_new chuginate && \
	move_to_new LICENSE && \
	move_to_new notes && \
	move_to_new README.md && \
	# chugins
	update_new_chugin ABSaturator && \
	update_new_chugin AmbPan && \
	update_new_chugin Binaural && \
	update_new_chugin Bitcrusher && \
	update_new_chugin Elliptic && \
	update_new_chugin ExpDelay && \
	update_new_chugin ExpEnv && \
	update_new_chugin FIR && \
	update_new_chugin FoldbackSaturator && \
	update_new_chugin GVerb && \
	update_new_chugin KasFilter && \
	update_new_chugin MagicSine && \
	update_new_chugin Mesh2D && \
	update_new_chugin Multicomb && \
	update_new_chugin NHHall && \
	update_new_chugin Overdrive && \
	update_new_chugin PanN && \
	# Patch not working
	# update_new_chugin Patch && \
	# cp -rf thirdparty/chugins/Patch thirdparty/chugins-new/ && \
	update_new_chugin Perlin && \
	update_new_chugin PitchTrack && \
	update_new_chugin PowerADSR && \
	update_new_chugin Random && \
	update_new_chugin Range && \
	update_new_chugin RegEx && \
	update_new_chugin Sigmund && \
	# duplicaate symbols between sigmund.c and sigmund-dsp.c
	mv thirdparty/chugins-new/Sigmund/sigmund.c thirdparty/chugins-new/Sigmund/sigmund.c.orig && \
	update_new_chugin Spectacle && \
	update_new_chugin WPDiodeLadder && \
	update_new_chugin WPKorg35 && \
	update_new_chugin Wavetable && \
	update_new_chugin WinFuncEnv && \
	mv thirdparty/chugins thirdparty/chugins-old && \
	mv thirdparty/chugins-new thirdparty/chugins && \
	rm -rf chugins-src
}


function fix_chuck_tilde() {
	# replace "msg->reply" "msg->reply_cb" ${CHUCK_TILDE}
	patch chuck~/chuck~.cpp < scripts/patch/chuck_tilde.patch
}

function fix_chuck_core_cmakelists() {
	patch thirdparty/chuck/core/CMakeLists.txt < scripts/patch/chuck_core_cmakelists.patch
}


function fix_chugins_cmakelists() {
	patch thirdparty/chugins/CMakeLists.txt < scripts/patch/chugins_cmakelists.patch
}


function update() {
	update_chuck
	update_chugins
	fix_chuck_tilde
	fix_chuck_core_cmakelists
	fix_chugins_cmakelists
}

update
