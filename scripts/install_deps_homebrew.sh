#!/usr/bin/env sh

brew_install() {
    echo "\nInstalling $1"
    if brew list $1 &>/dev/null; then
        echo "${1} is already installed"
    else
        brew install $1 && echo "$1 is installed"
    fi
}



# required for base
brew_install cmake 
brew_install bison
brew_install flex

# required for base + Faust + WarpBuf
# (additional generic build tools you may not have) // check if these are actuall needed
brew_install autoconf
brew_install autogen
brew_install automake

# these are the actual dependencies
brew_install flac 
brew_install libogg
brew_install libtool
brew_install libvorbis
brew_install opus mpg123
brew_install lame
brew_install rubberband
brew_install libsamplerate