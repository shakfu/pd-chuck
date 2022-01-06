
COREDIR = ../core
SDK=$(shell xcrun --show-sdk-path)

INCLUDE = -I$(COREDIR) -I$(COREDIR)/lo
LIBS=$(wildcard lib/*)
MACOS_VER=10.9

ISYSROOT = -isysroot $(SDK)
FRAMEWORKS += CoreAudio CoreMIDI CoreFoundation IOKit Carbon AppKit Foundation
LINK_EXTRAS =

FLAGS = -DCPU_IS_LITTLE_ENDIAN -D__MACOSX_CORE__

cflags += $(FLAGS) $(ISYSROOT) -stdlib=libc++ -mmacosx-version-min=$(MACOS_VER) $(INCLUDE)
ldflags += $(addprefix -framework ,$(FRAMEWORKS)) \
    	   $(LINK_EXTRAS) -lm ../libchuck.a

# library name
lib.name = puck

# input source file (class name == source file basename)
class.sources = puck~.cpp

# all extra files to be included in binary distribution of the library
datafiles =

# include Makefile.pdlibbuilder from submodule directory 'pd-lib-builder'
include Makefile.pdlibbuilder


demo:
	clang++ -o demo \
		$(FLAGS) \
		-I./include/core \
		-I./include/host \
		../host/chuck-embed.cpp \
		../libchuck.a \
		-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk \
		-framework CoreAudio \
		-framework CoreMIDI \
		-framework CoreFoundation \
		-framework IOKit \
		-framework Carbon \
		-framework AppKit \
		-framework Foundation \
		-F/System/Library/PrivateFrameworks \
		-weak_framework MultitouchSupport \
		-lc++ \
		-lm \
		-framework GLUT \
		-framework OpenGL