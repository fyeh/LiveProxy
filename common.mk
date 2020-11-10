

.DEFAULT_GOAL=all

# users can specify a different location of ffmpeg include files and libs
ifeq ($(FFMPEGINC),)
#FFMPEGINC:=../../../../iva-ffmpeg/ffmpeg-4.0
FFMPEGINC:=$(ROOTREL)/../FFMPEG
endif
ifeq ($(FFMPEGLIB),)
#FFMPEGLIB:=../../../../iva-ffmpeg/ffmpeg-4.0/bin
FFMPEGLIB:=$(ROOTREL)/../FFMPEG
endif

# makefiles can overrride - for example if they are using c files
SRCEXT?=cpp

# mods can set specific compile flags
MODCXXFLAGS?=
MODCFLAGS?=
MODINC?=
MODLIBS?=
OBJDIR?=

CPU_ARCH := $(shell uname -m)

ifeq ($(OS),Windows_NT)
#CXX=msvc
CXX=cl
CXXFLAGS=-D_LP_FOR_WINDOWS_ -D_WIN32_WINNT=_WIN32_WINNT_MAXVER -EHsc -DNOMINMAX $(MODCXXFLAGS)
DEBUGFLAG=-D_DEBUG
OUTFLAG=-Fo
LIBS=
SLIBEXT=.dll
APPEXT=.exe
OBJEXT=obj
SHARED_LFLAGS=
LINKER_STGRP=
LINKER_ENDGP=
else
CXX=g++
CXXFLAGS=-Wall -fPIC -std=c++11 -m64 -D_LP_FOR_LINUX_ $(MODCXXFLAGS)
ifeq ($(CPU_ARCH),aarch64)
	CXXFLAGS=-Wall -fPIC -std=c++11 -D_LP_FOR_LINUX_ $(MODCXXFLAGS)
endif
DEBUGFLAG=-g -D_DEBUG
OUTFLAG=-o
CFLAGS=-Wall -fPIC -x c -D_LP_FOR_LINUX_ $(MODCFLAGS)
LIBS=-ldl -lz -lstdc++ -lpthread -lcurl -lncurses $(MODLIBS)
SLIBEXT=.so
APPEXT=
OBJEXT=o
SHARED_CFLAGS=
SHARED_LFLAGS=-shared
LINKER_STGRP=
LINKER_ENDGP=
UNAME_RES := $(shell uname -s)
	ifeq ($(UNAME_RES),Darwin)
	else
  	LINKER_STGRP=-Wl,--start-group
		LINKER_ENDGP=-Wl,--end-group
		ifeq ($(shell lscpu | grep -oP 'Architecture:\s*\K.+'),ppc64le)
			CXXFLAGS=-Wall -fPIC -std=c++11 -m64 -D_LP_FOR_LINUX_ $(MODCXXFLAGS)
    endif
	endif
endif

SSEINCS=-I. -I$(ROOTREL) $(MODINC)

# these are per module for the compiled objects, excluded by .gitignore
ifeq ($(OBJDIR),)
DEBUGO:=Obj/Debug
RELEASEO:=Obj/Release
else
# for dockerized cross compile set different folders
DEBUGO:=$(OBJDIR)$(CURDIR)/Obj/Debug
RELEASEO:=$(OBJDIR)$(CURDIR)/Obj/Release
endif

DEBUGL:=Lib/Debug
RELEASEL:=Lib/Release

# these are at the root level for the archive libs, empty versions exist in the repo
ifeq ($(LOCALLIB),)
AR_DEBUG:=$(ROOTREL)/$(DEBUGL)
AR_RELEASE:=$(ROOTREL)/$(RELEASEL)
else
# local library - these are within the source dir, not at the root level
ifeq ($(OBJDIR),)
else
DEBUGL:=$(OBJDIR)$(CURDIR)/Lib/Debug
RELEASEL:=$(OBJDIR)$(CURDIR)/Lib/Release
endif
INITLOCAL=$(DEBUGL) $(RELEASEL)
AR_DEBUG:=$(DEBUGL)
AR_RELEASE:=$(RELEASEL)
endif
AR_D=$(AR_DEBUG)/$(LIB).a
AR_R=$(AR_RELEASE)/$(LIB).a

# these are at the root level for the binaries libs, empty versions exist in the repo
BIN_DEBUG:=$(ROOTREL)/Bin/Debug
BIN_RELEASE:=$(ROOTREL)/Bin/Release

# only set these if an app was specified, otherwise clean removes the entire folder
ifneq ($(APP),)
AR_D=
AR_R=
BIN_D=$(BIN_DEBUG)/$(APP)$(APPEXT)
BIN_R=$(BIN_RELEASE)/$(APP)$(APPEXT)
LINKFLAGS=
EXTRACFLAGS=
endif

# only set these if a shared lib was specified, otherwise clean removes the entire folder
ifneq ($(SLIB),)
AR_D=
AR_R=
BIN_D=$(BIN_DEBUG)/$(SLIB)$(SLIBEXT)
BIN_R=$(BIN_RELEASE)/$(SLIB)$(SLIBEXT)
LINKFLAGS=$(SHARED_LFLAGS)
EXTRACFLAGS=$(SHARED_CFLAGS)
endif

SRCFILES="*.$(SRCEXT)"

# setting MODSKIP allows files that would normally match *.cpp to be skipped, see usage in MILSClientCpp
ifeq ($(MODSKIP),)
SRC:=$(shell find $(MODDIR) -maxdepth 1 -name $(SRCFILES) )
else
SRC:=$(shell find $(MODDIR) -maxdepth 1 -name $(SRCFILES) ! -name $(MODSKIP) )
endif

ifeq ($(SRCEXT),cpp)
OBJECTS_D:=$(patsubst %.cpp,$(DEBUGO)/%.$(OBJEXT),$(SRC))
OBJECTS_R:=$(patsubst %.cpp,$(RELEASEO)/%.$(OBJEXT),$(SRC))
endif
ifeq ($(SRCEXT),c)
OBJECTS_D:=$(patsubst %.c,$(DEBUGO)/%.$(OBJEXT),$(SRC))
OBJECTS_R:=$(patsubst %.c,$(RELEASEO)/%.$(OBJEXT),$(SRC))
endif
ifeq ($(SRCEXT),c*)
OBJECTS_D:=$(patsubst %.cpp,$(DEBUGO)/%.$(OBJEXT),$(patsubst %.c,$(DEBUGO)/%.o,$(SRC)))
OBJECTS_R:=$(patsubst %.cpp,$(RELEASEO)/%.$(OBJEXT),$(patsubst %.c,$(RELEASEO)/%.o,$(SRC)))
endif

LIB_D:= $(shell find $(AR_DEBUG) -maxdepth 1 -name "*.a")
LIB_R:= $(shell find $(AR_RELEASE) -maxdepth 1 -name "*.a")


# for debugging
echosrc:
	@echo $(SRC)
echolib:
	@echo $(LIB_D)


init: $(DEBUGO) $(RELEASEO) $(INITLOCAL) $(BIN_DEBUG) $(BIN_RELEASE)

$(DEBUGO):
	mkdir -p $(DEBUGO)

$(RELEASEO):
	mkdir -p $(RELEASEO)

$(DEBUGL):
	mkdir -p $(DEBUGL)

$(RELEASEL):
	mkdir -p $(RELEASEL)

$(BIN_DEBUG):
	mkdir -p $(BIN_DEBUG)

$(BIN_RELEASE):
	mkdir -p $(BIN_RELEASE)

$(DEBUGO)/%.$(OBJEXT): %.cpp
	$(CXX) $(CXXFLAGS) $(EXTRACFLAGS) $(SSEINCS) $(DEBUGFLAG) -c $(OUTFLAG)$@ $<

$(RELEASEO)/%.$(OBJEXT): %.cpp
	$(CXX) $(CXXFLAGS) $(EXTRACFLAGS) $(SSEINCS) -c $(OUTFLAG)$@ $<

$(DEBUGO)/%.$(OBJEXT): %.c
	$(CXX) $(CFLAGS) $(EXTRACFLAGS) $(SSEINCS) $(DEBUGFLAG) -c $(OUTFLAG)$@ $<

$(RELEASEO)/%.$(OBJEXT): %.c
	$(CXX) $(CFLAGS) $(EXTRACFLAGS) $(SSEINCS) -c $(OUTFLAG)$@ $<

$(BIN_D): $(OBJECTS_D) $(LIB_D) $(MODLIBS_D)
	$(CXX) $(CXXFLAGS) $(LINKFLAGS) $(OUTFLAG)$@ $(LINKER_STGRP) $(LIB_D) $(LIBS) $(MODLIBS_D) $(OBJECTS_D) $(LINKER_ENDGP)

$(BIN_R): $(OBJECTS_R) $(LIB_R) $(MODLIBS_R)
	$(CXX) $(CXXFLAGS) $(LINKFLAGS) $(OUTFLAG)$@ $(LINKER_STGRP) $(LIB_R) $(LIBS) $(MODLIBS_R) $(OBJECTS_R) $(LINKER_ENDGP)


# if a lib target was specified, add that to the build targets
ifneq ($(LIB),)
TARGET_DL=$(AR_D)
TARGET_RL=$(AR_R)
endif

# if a shared library target was specified, add that to the build targets
ifneq ($(SLIB),)
TARGET_DS=$(BIN_D)
TARGET_RS=$(BIN_R)
endif

# if a app target was specified, add that to the build targets
ifneq ($(APP),)
TARGET_DA=$(BIN_D)
TARGET_RA=$(BIN_R)
endif

debug: init  $(OBJECTS_D) $(TARGET_DL) $(TARGET_DS) $(TARGET_DA)

release: init $(OBJECTS_R) $(TARGET_RL) $(TARGET_RS) $(TARGET_RA)

$(AR_D): $(OBJECTS_D)
	ar rcvs $(AR_D) $(OBJECTS_D)

$(AR_R): $(OBJECTS_R)
	ar rcvs $(AR_R) $(OBJECTS_R)

clean:
	rm -rf $(DEBUGO) $(RELEASEO) $(AR_D) $(AR_R) $(BIN_D) $(BIN_R)
