
COREDIR = ../core
HOSTDIR = ../host
SDK=$(shell xcrun --show-sdk-path)

INCLUDE = -I./include/core -I./include/core/lo -I./include/host

# LIBS=$(wildcard lib/*)
LIBCHUCK = ../libchuck.a
MACOS_VER=10.9

ISYSROOT = -isysroot $(SDK)
FRAMEWORKS += CoreAudio CoreMIDI CoreFoundation IOKit Carbon AppKit Foundation
LINK_EXTRAS =

FLAGS = -DCPU_IS_LITTLE_ENDIAN -D__MACOSX_CORE__

cflags += $(FLAGS) $(ISYSROOT) -stdlib=libc++ -mmacosx-version-min=$(MACOS_VER) $(INCLUDE)
ldflags += $(addprefix -framework ,$(FRAMEWORKS)) \
    	   $(LINK_EXTRAS) -lm $(LIBCHUCK)

# library name
lib.name = chuck

# input source file (class name == source file basename)
class.sources = chuck~.cpp

# all extra files to be included in binary distribution of the library
datafiles =

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
include Makefile.pdlibbuilder


demo:
	clang++ -o demo \
		$(cflags) \
		../host/chuck-embed.cpp \
		$(ldflags) \
 		-weak_framework MultitouchSupport \
 		-F/System/Library/PrivateFrameworks
