#!/usr/bin/env bash


echo "changing kAudioObjectPropertyElementMaster -> kAudioObjectPropertyElementMain"
rpl --match-case --whole-words --fixed-strings --recursive \
	kAudioObjectPropertyElementMaster \
	kAudioObjectPropertyElementMain \
	thirdparty/chuck/host/RtAudio
