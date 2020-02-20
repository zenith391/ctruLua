#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
# ROMFS: if set, use the files at this path to build a ROMFS
#---------------------------------------------------------------------------------
TARGET		:=	ctruLua
BUILD		:=	build
SOURCES		:=	source libs/lua-5.3.3/src libs/tremor
DATA		:=	data
INCLUDES	:=	include libs/lua-5.3.3/src libs/3ds_portlibs/zlib-1.2.8 libs/lzlib libs/tremor libs/3ds_portlibs/libogg-1.3.2/include
#ROMFS		:=	romfs
ROOT			:=	sdmc:/3ds/ctruLua/

APP_TITLE		:= ctruLua
APP_DESCRIPTION	:= Lua for the 3DS. Yes, it works.
APP_AUTHOR		:= Reuh,Ihamfp,Nodyn
APP_PRODUCT_CODE	:= CTR-P-ULUA
APP_UNIQUE_ID		:= 0xB00B5

ICON 			:= icon.png
BANNER			:= banner.png
JINGLE			:= jingle.wav
APP_VERSION		:= $(shell git describe --abbrev=0 --tags)
LASTCOMMIT		:= $(shell git rev-parse HEAD)

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard

CFLAGS	:=	-g -Wall -O2 -mword-relocations -std=gnu11 \
			-fomit-frame-pointer -ffast-math \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS -DCTR_VERSION=\"$(APP_VERSION)\" -DCTR_BUILD=\"$(LASTCOMMIT)\"
ifneq ($(ROMFS),)
	CFLAGS += -DROMFS
endif
ifneq ($(ROOT),)
	CFLAGS += -DROOT=\"$(ROOT)\"
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -ljpeg -lfreetype -lpng16 -lz -lsf2d -lcitro3d -lctru -logg -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(PORTLIBS) \
			$(CURDIR)/libs/3ds_portlibs/ \
			$(CURDIR)/libs/sf2dlib/libsf2d \
			$(CURDIR)/libs/sftdlib/libsftd \
			$(CURDIR)/libs/sfillib/libsfil \
			$(CURDIR)/libs/stb \
			$(CURDIR)/libs/citro3d

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/.libs)
export LIBPATHS := $(LIBPATHS) -L$(DEVKITPRO)/portlibs/armv6k/lib

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

export CIA_ARGS := -DAPP_TITLE=$(APP_TITLE) -DAPP_PRODUCT_CODE=$(APP_PRODUCT_CODE) \
	-DAPP_UNIQUE_ID=$(APP_UNIQUE_ID) \
	-elf $(OUTPUT).elf -rsf "$(TOPDIR)/ctrulua.rsf" \
	-icon $(TOPDIR)/icon.bin -banner $(TOPDIR)/banner.bin -exefslogo -target t

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
	export CIA_ARGS += -DAPP_ROMFS_DIR=$(ROMFS)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD)-cia:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile $(OUTPUT).cia

build-portlibs:
	@make -C libs/3ds_portlibs zlib install-zlib freetype libjpeg-turbo libpng libogg install
	rm $(DEVKITPRO)/portlibs/armv6k/lib/freetype.so # to avoid LD errors

build-citro3d:
	@make -C libs/citro3d install

build-sf2dlib: # WILL BE REPLACED!!
	#@make -C libs/sf2dlib/libsf2d build

build-sftdlib: # REQUIRES SF2D and will be REPLACED!!
	#@make -C libs/sftdlib/libsftd build

build-sfillib: # same as above
	#@make -C libs/sfillib/libsfil build

build-all:
	@echo Building 3ds_portlibs...
	@make build-portlibs
	@echo Building citro3D
	@make build-citro3d
	@echo Building sf2dlib...
	@make build-sf2dlib
	@echo Building sftdlib...
	@make build-sftdlib
	@echo Building sfillib...
	@make build-sfillib
	@echo Building ctruLua...
	@make build

build-doc:
	@echo Building HTML documentation...
	@make build-doc-html
	@echo Building SublimeText documentation...
	@make build-doc-st

build-doc-html:
	@cd doc/ && ldoc . && cd ..

build-doc-st:
	@cd doc/ && ldoc . --template ./ --ext sublime-completions --dir ./sublimetext/ && cd ..

#---------------------------------------------------------------------------------
clean:
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf banner.bin icon.bin

clean-portlibs:
	@make -C libs/3ds_portlibs clean

clean-citro3d:
	@make -C libs/citro3d clean

clean-sf2dlib:
	@make -C libs/sf2dlib/libsf2d clean

clean-sftdlib:
	@make -C libs/sftdlib/libsftd clean

clean-sfillib:
	@make -C libs/sfillib/libsfil clean

clean-all:
	@echo Cleaning 3ds_portlibs...
	@make clean-portlibs
	@echo Cleaning sf2dlib...
	@make clean-sf2dlib
	@echo Cleaning citro3d...
	@make clean-citro3d
	@echo Cleaning sftdlib...
	@make clean-sftdlib
	@echo Cleaning sfillib...
	@make clean-sfillib
	@echo Cleaning ctruLua...
	@make clean

clean-doc:
	@echo Cleaning HTML documentation...
	@make clean-doc-html
	@echo Cleaning SublimeText documentation...
	@make clean-doc-st

clean-doc-html:
	@rm -rf doc/html

clean-doc-st:
	@rm -rf doc/sublimetext

tt:
	echo $(LIBPATHS)


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

all	:	$(OUTPUT).3dsx $(OUTPUT).cia

ifeq ($(strip $(NO_SMDH)),)
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh
else
$(OUTPUT).3dsx	:	$(OUTPUT).elf
endif

icon.bin	:
	bannertool makesmdh -s $(APP_TITLE) -l $(APP_TITLE) -p $(APP_AUTHOR) -i $(TOPDIR)/$(ICON) -o $(TOPDIR)/icon.bin -f visible allow3d

banner.bin	:
	bannertool makebanner -i $(TOPDIR)/$(BANNER) -a $(TOPDIR)/$(JINGLE) -o $(TOPDIR)/banner.bin

$(OUTPUT).elf	:	$(OFILES)

$(OUTPUT).cia	:	$(OUTPUT).elf icon.bin banner.bin
	makerom -f cia -o $(OUTPUT).cia $(CIA_ARGS)
	

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
%.ttf.o	:	%.ttf
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

# WARNING: This is not the right way to do this! TODO: Do it right!
#---------------------------------------------------------------------------------
%.vsh.o	:	%.vsh
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@python $(CURDIR)/libs/aemstro/aemstro_as.py $< ../$(notdir $<).shbin
	@bin2s ../$(notdir $<).shbin | $(PREFIX)as -o $@
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u32" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@rm ../$(notdir $<).shbin

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
