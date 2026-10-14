# MarkPS4 Themes - the PS4 dashboard app package.
#
# Adapted from the OpenOrbis toolchain's own sample Makefile, so the build steps are the ones the
# toolchain documents rather than something invented here. Driven by build-wsl.sh, which fetches
# the toolchain and sets OO_PS4_TOOLCHAIN.
#
# TITLE_ID matches the PS5 tile's (PKGM00001) on purpose: one app, one identity, whichever console
# it is on. It cannot collide with a retail game - those are CUSA/NPXS - and IV0000 is the publisher
# prefix homebrew uses.

TITLE       := MarkPS4 Themes
VERSION     := 02.00
TITLE_ID    := PKGM00001
CONTENT_ID  := IV0000-PKGM00001_00-MARKPS4THEMES001

# ONLY WHAT THE TOOLCHAIN'S OWN WORKING SAMPLE LINKS. hello_world, the one sample known to launch
# on this console family, needs libkernel alone; everything else is resolved at run time in main.c.
#
# THIS IS NOT A DIAGNOSIS OF CE-32930-7. An earlier version of this comment said the extra
# SCE_NEEDED_MODULE entries were the cause; that does not survive checking - the modules named are
# resident on the console and the toolchain ships working samples with the same shape. Matching the
# known-good reference is free and cannot hurt, which is the whole argument for it.
# -lSceSystemService is what makes HideSplashScreen and LoadExec real calls instead of NULL
# pointers. An application sandbox cannot open /system/common/lib, so linking is the ONLY way
# it reaches these - the native renderer lives in pms/main.cpp. It is one extra needed module, and the
# old theory that extra needed modules stopped this app launching is disproven: the launch
# blocker was a missing BGFT task, and the app launches.
LIBS        := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceBgft

ASSETS      := $(wildcard assets/**/*)
LIBMODULES  := $(wildcard sce_module/*)

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
CXX         := clang++
PROJDIR     := pms
COMMONDIR   := $(TOOLCHAIN)/samples/_common
INTDIR      := $(PROJDIR)/x64/Debug

CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
COMMONFILES := $(COMMONDIR)/graphics.cpp
OBJS        := $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES)) $(patsubst $(COMMONDIR)/%.cpp, $(INTDIR)/common-%.o, $(COMMONFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -I$(COMMONDIR) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib \
               $(LIBS) $(TOOLCHAIN)/lib/crt1.o

_unused     := $(shell mkdir -p $(INTDIR))

CC          := clang
LD          := ld.lld
CDIR        := linux

TARGET      := $(CONTENT_ID).pkg

all: $(TARGET)

$(TARGET): pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build pkg.gp4 .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/$(PROJDIR).elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/$(PROJDIR).elf -out=$(INTDIR)/$(PROJDIR).oelf \
		--eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/common-%.o: $(COMMONDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) pkg.gp4 sce_sys/param.sfo eboot.bin \
		$(INTDIR)/$(PROJDIR).elf $(INTDIR)/$(PROJDIR).oelf $(INTDIR)/*.o

.PHONY: all clean
