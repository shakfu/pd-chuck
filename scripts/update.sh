#!/usr/bin/env bash

# update.sh
#
# Updates the vendored copies of chuck, the chugins and the chuck examples
# from upstream, preserving this project's local additions.
#
# usage:
#   ./scripts/update.sh            update everything
#   ./scripts/update.sh chuck      update thirdparty/chuck only
#   ./scripts/update.sh examples   update chuck_tilde/examples only
#   ./scripts/update.sh chugins    update thirdparty/chugins only
#
# 'chuck' and 'examples' share a single clone of the chuck repo, so running
# them together costs no more than running either alone.

set -uo pipefail

CHUCK_REPO=https://github.com/ccrma/chuck.git
CHUGINS_REPO=https://github.com/ccrma/chugins.git

THIRDPARTY=thirdparty
BUILD=build
CHUCK_SRC=${BUILD}/chuck-src
CHUGINS_SRC=${BUILD}/chugins-src
CHUCK_SRC_EXAMPLES=${CHUCK_SRC}/examples
EXAMPLES=chuck_tilde/examples
EXAMPLES_BAK=${BUILD}/examples-local

# Directories under chuck_tilde/examples that belong to this project rather
# than to upstream chuck. The examples tree is replaced wholesale from
# upstream, so these are set aside first and restored afterwards.
CUSTOM_DIRS=(
	chugins
	convrev
	fauck
	faust
	fluidsynth
	hanoi
	line
	otf
	pd
	test
	util
	warpbuf
)

# Individual files added by this project that live inside otherwise-upstream
# directories, as paths relative to the examples directory.
CUSTOM_FILES=(
	README.md
	data/amen.wav
	data/nylon2.mp3
	stk/honkeytonk-algo3.ck
	midi/data/africa.mid
	midi/data/yiruma.mid
)

# Loose upstream files this project does not ship. Removed from the fresh
# clone before it is copied into place.
UPSTREAM_CRUFT=(
	README
	book
	hanoi++.ck
	hanoi.ck
	hanoi2.ck
	hanoi3.ck
	help.ck
	otf_01.ck
	otf_02.ck
	otf_03.ck
	otf_04.ck
	otf_05.ck
	otf_06.ck
	otf_07.ck
	status.ck
)


function clone_chuck() {
	if [ -d ${CHUCK_SRC} ]; then
		echo "  reusing existing clone at ${CHUCK_SRC}"
		return 0
	fi
	mkdir -p ${BUILD} && \
	git clone --depth=1 ${CHUCK_REPO} ${CHUCK_SRC}
}


function update_chuck() {
	echo "==> updating ${THIRDPARTY}/chuck"
	clone_chuck && \
	mkdir -p ${THIRDPARTY}/chuck-new && \
	cp -Rf ${CHUCK_SRC}/src/core ${THIRDPARTY}/chuck-new/ && \
	cp -Rf ${CHUCK_SRC}/src/host ${THIRDPARTY}/chuck-new/ && \
	cp ${THIRDPARTY}/chuck/core/CMakeLists.txt ${THIRDPARTY}/chuck-new/core/ && \
	cp ${THIRDPARTY}/chuck/host/CMakeLists.txt ${THIRDPARTY}/chuck-new/host/ && \
	mv ${THIRDPARTY}/chuck ${THIRDPARTY}/chuck-old && \
	mv ${THIRDPARTY}/chuck-new ${THIRDPARTY}/chuck && \
	rm -rf ${THIRDPARTY}/chuck-old
}


function save_local_examples() {
	rm -rf ${EXAMPLES_BAK}
	mkdir -p ${EXAMPLES_BAK} || return 1

	local d
	for d in "${CUSTOM_DIRS[@]}"; do
		if [ -d "${EXAMPLES}/${d}" ]; then
			cp -Rf "${EXAMPLES}/${d}" "${EXAMPLES_BAK}/" || return 1
		else
			echo "  note: local example dir '${d}' not found, skipping"
		fi
	done

	local f
	for f in "${CUSTOM_FILES[@]}"; do
		if [ -f "${EXAMPLES}/${f}" ]; then
			mkdir -p "${EXAMPLES_BAK}/$(dirname "${f}")" || return 1
			cp -f "${EXAMPLES}/${f}" "${EXAMPLES_BAK}/${f}" || return 1
		else
			echo "  note: local example file '${f}' not found, skipping"
		fi
	done
}


function restore_local_examples() {
	local d
	for d in "${CUSTOM_DIRS[@]}"; do
		if [ -d "${EXAMPLES_BAK}/${d}" ]; then
			rm -rf "${EXAMPLES}/${d}"
			cp -Rf "${EXAMPLES_BAK}/${d}" "${EXAMPLES}/" || return 1
		fi
	done

	local f
	for f in "${CUSTOM_FILES[@]}"; do
		if [ -f "${EXAMPLES_BAK}/${f}" ]; then
			mkdir -p "${EXAMPLES}/$(dirname "${f}")" || return 1
			cp -f "${EXAMPLES_BAK}/${f}" "${EXAMPLES}/${f}" || return 1
		fi
	done
}


function update_examples() {
	echo "==> updating ${EXAMPLES}"
	clone_chuck || return 1

	if [ ! -d ${CHUCK_SRC_EXAMPLES} ]; then
		echo "  error: ${CHUCK_SRC_EXAMPLES} not found; did the clone succeed?"
		return 1
	fi

	echo "  saving local examples"
	save_local_examples || return 1

	echo "  removing upstream files this project does not ship"
	local c
	for c in "${UPSTREAM_CRUFT[@]}"; do
		rm -rf "${CHUCK_SRC_EXAMPLES:?}/${c}"
	done

	echo "  replacing examples with the upstream tree"
	rm -rf ${EXAMPLES} && \
	cp -Rf ${CHUCK_SRC_EXAMPLES} ${EXAMPLES} || return 1

	echo "  restoring local examples"
	restore_local_examples || return 1

	# the chuck executable symlink lives inside examples/ and is wiped by the
	# replacement above, so put it back
	ln -sf ../../build/chuck ${EXAMPLES}/chuck

	rm -rf ${EXAMPLES_BAK}
	echo "  done"
}


function move_to_new() {
	mv ${CHUGINS_SRC}/"$1" ${THIRDPARTY}/chugins-new/"$1"
}


function update_new_chugin() {
	move_to_new "$1" && \
	cp ${THIRDPARTY}/chugins/"$1"/CMakeLists.txt ${THIRDPARTY}/chugins-new/"$1" && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/makefile* && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/*.dsw && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/*.dsp && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/*.xcodeproj && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/*.vcxproj && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/*.sln && \
	rm -rf ${THIRDPARTY}/chugins-new/"$1"/.gitignore
}


function update_chugins() {
	echo "==> updating ${THIRDPARTY}/chugins"
	mkdir -p ${BUILD} && \
	rm -rf ${CHUGINS_SRC} && \
	git clone --depth=1 ${CHUGINS_REPO} ${CHUGINS_SRC} && \
	mkdir -p ${THIRDPARTY}/chugins-new && \
	cp ${THIRDPARTY}/chugins/CMakeLists.txt ${THIRDPARTY}/chugins-new/ && \
	# non-chugins
	move_to_new chuck && \
	move_to_new chuginate && \
	move_to_new LICENSE && \
	move_to_new notes && \
	move_to_new README.md && \
	# chugins maintained by this project rather than upstream
	mv ${THIRDPARTY}/chugins/AbletonLink ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/AudioUnit ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/CLAP ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/Fauck ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/PdPatch ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/VST3 ${THIRDPARTY}/chugins-new/ && \
	mv ${THIRDPARTY}/chugins/WarpBuf ${THIRDPARTY}/chugins-new/ && \
	# upstream chugins
	update_new_chugin ABSaturator && \
	update_new_chugin AmbPan && \
	update_new_chugin Binaural && \
	update_new_chugin Bitcrusher && \
	update_new_chugin ConvRev && \
	update_new_chugin Elliptic && \
	update_new_chugin ExpDelay && \
	update_new_chugin ExpEnv && \
	update_new_chugin FIR && \
	update_new_chugin FluidSynth && \
	update_new_chugin FoldbackSaturator && \
	update_new_chugin GVerb && \
	update_new_chugin KasFilter && \
	update_new_chugin Ladspa && \
	update_new_chugin Line && \
	update_new_chugin MagicSine && \
	update_new_chugin Mesh2D && \
	update_new_chugin MIAP && \
	update_new_chugin Multicomb && \
	update_new_chugin NHHall && \
	update_new_chugin Overdrive && \
	update_new_chugin PanN && \
	update_new_chugin Patch && \
	update_new_chugin Perlin && \
	update_new_chugin PitchTrack && \
	update_new_chugin PowerADSR && \
	update_new_chugin Random && \
	update_new_chugin Range && \
	update_new_chugin RegEx && \
	update_new_chugin Sigmund && \
	# duplicate symbols between sigmund.c and sigmund-dsp.c
	mv ${THIRDPARTY}/chugins-new/Sigmund/sigmund.c ${THIRDPARTY}/chugins-new/Sigmund/sigmund.c.orig && \
	update_new_chugin Spectacle && \
	update_new_chugin WPDiodeLadder && \
	update_new_chugin WPKorg35 && \
	update_new_chugin Wavetable && \
	update_new_chugin WinFuncEnv && \
	update_new_chugin XML && \
	rm -rf ${THIRDPARTY}/chugins-new/RegEx/RegEx.vcxproj.filters && \
	rm -rf ${THIRDPARTY}/chugins-new/chuginate && \
	mv ${THIRDPARTY}/chugins ${THIRDPARTY}/chugins-old && \
	mv ${THIRDPARTY}/chugins-new ${THIRDPARTY}/chugins && \
	rm -rf ${THIRDPARTY}/chugins-old
}


function cleanup() {
	rm -rf ${CHUCK_SRC}
	rm -rf ${CHUGINS_SRC}
	rm -rf ${THIRDPARTY}/chuck-old
	rm -rf ${THIRDPARTY}/chugins-old
	rm -rf ${EXAMPLES_BAK}
}


function update_all() {
	update_chuck && \
	update_examples && \
	update_chugins
}


case "${1:-all}" in
	chuck)    update_chuck ; rc=$? ;;
	examples) update_examples ; rc=$? ;;
	chugins)  update_chugins ; rc=$? ;;
	all)      update_all ; rc=$? ;;
	*)        echo "usage: $0 [all|chuck|examples|chugins]"; exit 1 ;;
esac

cleanup
exit ${rc}
