#---------------------------------------------------------------------------------
# HomeTidy for PS3 (PSL1GHT). Needs the ps3dev toolchain: export PS3DEV=<path> PSL1GHT=<path>.
#   make        builds HomeTidy.self
#   make pkg    builds HomeTidy.pkg (installable with "Install Package Files" on a CFW console)
#   needs python3 with Pillow: the package art carries the version (tools/make_art.py)
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
MKROOT		:=	$(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# The release version (VERSION) is the single source: the app title bar, PARAM.SFO, the package art and the file names.
# PARAM.SFO only holds two numbers (NN.NN), so the second one is minor * 10 + patch (1.0.2 -> 01.02, 1.1.0 -> 01.10).
VERSION		:=	$(strip $(shell cat $(MKROOT)VERSION))
APP_VER		:=	$(shell sh $(MKROOT)tools/prepare.sh $(MKROOT) >/dev/null && cat $(MKROOT)build/app_ver)

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path to ps3dev>")
endif

include $(PSL1GHT)/ppu_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories with files linked into the binary (the fonts)
#---------------------------------------------------------------------------------
TARGET		:=	HomeTidy
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	source third_party/stb

TITLE		:=	HomeTidy
APPID		:=	HTDY00001
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000
SFOXML		:=	$(MKROOT)build/sfo.xml
ICON0		:=	$(MKROOT)build/art/ICON0.PNG
PKGFILES	:=	$(MKROOT)build/art/files

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS		=	-O2 -Wall -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS) $(CXXFLAGS_INIT) -std=gnu++20 -DAPP_VERSION=\"$(VERSION)\"

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS		:=	-lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lrsx -lgcm_sys -lio -lnet -lsysmodule -lsysutil -lrt -llv2 -lm

# the ps3dev portlibs hold libcurl, mbedTLS and zlib
LIBDIRS		:=	$(PORTLIBS)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export BUILDDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

# use CXX for linking C++ projects, CC for standard C
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(LIBPSL1GHT_INC) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					$(LIBPSL1GHT_LIB)

.PHONY: $(BUILD) clean pkg

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).fake.self $(OUTPUT).pkg $(OUTPUT).gnpdrm.pkg

run:
	ps3load $(OUTPUT).self

pkg: $(BUILD) $(OUTPUT).pkg

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
