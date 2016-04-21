#
# Makefile for CheriOS -- a minimal microkernel that demonstrates
# "clean-slate" CHERI memory protection and object capabilities.
#

TOOLCHAINDIR?=	/home/$(USER)/sdk/sdk/bin
OBJDIR=		$(shell realpath obj)

DEBUG=1

#
# Default files location
#
LDSCRIPTDIR=	$(shell realpath ldscripts)

GINCDIR=	$(shell realpath include)

HFILES=		$(wildcard $(GINCDIR)/*.h)

INCLUDEFLAGS=   -Iinclude/ 				\
		-I$(GINCDIR)
		
LIBDIR=		$(shell realpath $(OBJDIR)/libs)


		
#
# Configure a console driver at compile time.  Default to the UART found in
# the MALTA reference board ("malta").  Other options are "altera" for the
# ALTERA JTAG UART used for BERI on FPGA, and "gxemul" for the GXemul
# low-level console device.
#
CONSOLE?=malta
#CONSOLE=altera
		
#
# Default toolchain
#
AR=ar
ARFLAGS=-cr

AS=$(TOOLCHAINDIR)/cheri-unknown-freebsd-clang
ASFLAGS=						\
	$(INCLUDEFLAGS)					\
	-target cheri-unknown-freebsd			\
	-mcpu=mips4					\
	-mabi=sandbox					\
	-cheri-linker					\
	-integrated-as					\
	-msoft-float					\
	-g

CC=$(TOOLCHAINDIR)/cheri-unknown-freebsd-clang
CCFLAGS=						\
	-nostdinc					\
	$(INCLUDEFLAGS)					\
	-target cheri-unknown-freebsd			\
	-mcpu=mips4					\
	-mabi=sandbox					\
	-cheri-linker					\
	-integrated-as					\
	-msoft-float					\
	-std=c11					\
	-O2						\
	-G0						\
	-mxgot						\
	-Werror						\
	-g
	
CCFLAGSWARN=						\
	-Wall						\
	-Wextra						\
	-Wdisabled-optimization				\
	-Wformat=2					\
	-Winit-self					\
	-Winline					\
	-Wpointer-arith					\
	-Wredundant-decls				\
	-Wswitch-default				\
	-Wswitch-enum					\
	-Wundef						\
	-Wwrite-strings					\
	-Wshadow					
#	-Wcast-align					\
#	-Wcast-qual					\
#	-Wconversion					\
	
ifeq ($(DEBUG),1)
CCFLAGSWARN+=						\
	-Wno-unused-function				\
	-Wno-unused-variable				\
	-Wno-unused-parameter				
endif
	
#CCFLAGSWARN=
CCFLAGS+=$(CCFLAGSWARN)

LD=$(TOOLCHAINDIR)/cheri-unknown-freebsd-clang
LDFLAGS=						\
	-cheri-linker					\
	-G0						\
	-mxgot						\
	-nostartfiles					\
	-nodefaultlibs					\
	-e start					\
	-L$(LIBDIR)					\
	-L$(LDSCRIPTDIR)

OBJDUMP=$(TOOLCHAINDIR)/objdump
OBJDUMPFLAGS=						\
	-x						\
	-d						\
	-S

		
# Default variables for sub-makefiles
export TOOLCHAINDIR
export OBJDIR
export GINCDIR
export LIBDIR
export LDSCRIPTDIR
export HFILES
export INCLUDEFLAGS
export CONSOLE

export AR
export ARFLAGS
export AS
export ASFLAGS
export CAPSIZEFIX
export CC
export CCFLAGS
export LD
export LDFLAGS
export OBJDUMP
export OBJDUMPFLAGS

#
# ===================================================================
#

#TODO: avoid duplicate code for sandboxes dependancies

#
# Targets
#
TARGETS=						\
		$(OBJDIR)/cherios.elf

FSDIR=$(OBJDIR)/fs

all: $(TARGETS)

$(OBJDIR)/cherios.elf: date prga uart libuser sockets
#Don't make the fs too large, it might break the whole elf without warning
#Max tested ok: 1M TODO:find limit TODO:remove limit
	makefs -s 256k -t ffs -o version=2 -B big $(OBJDIR)/fs.img $(FSDIR)
	$(MAKE) -C kernel
	cp $(OBJDIR)/kernel/kernel.elf $@
	
date:
	echo `date` > $(FSDIR)/t1
	
prga: libuser
	$(MAKE) -C $@
	cp $(OBJDIR)/$@/$@.elf $(FSDIR)/$@.elf
	
uart: libuser
	$(MAKE) -C $@
	cp $(OBJDIR)/$@/$@.elf $(FSDIR)/$@.elf
	
sockets: libuser
	$(MAKE) -C $@
	cp $(OBJDIR)/$@/$@.elf $(FSDIR)/$@.elf
	
libuser:
	$(MAKE) -C $@
	cp $(OBJDIR)/$@/$@.a $(LIBDIR)/$@.a

clean:
	rm -f $(FSDIR)/*
	rm -f $(OBJDIR)/*
	#TODO: clean sub

.PHONY: date prga uart libuser
	

