NAME=	$(shell basename `pwd`)

OBJDIR:=$(OBJDIR)/$(NAME)

#
# Source files of various flavours.
#
KERNEL_LDSCRIPT=kernel.ld

LIBKERNEL_ASMS=						\
	exception.S					\
	fs.S						\
	init.S

LIBKERNEL_SRCS=						\
	ccall.c						\
	cp0.c						\
	elf_loader.c					\
	heap.c						\
	kernel_exceptions.c				\
	kernel_printf.c					\
	kernel_utils.c					\
	malloc.c					\
	syscalls.c					\
	timer.c						\
	uart.c						\
	ufs_module.c					\
	ufs_read.c
	
LINKKERNEL_SRCS=					\
	crtbeginC.c					\
	crtendC.c

KERNEL_ASMS=

KERNEL_SRCS=						\
	main.c

OTHER_DEPENDS=						\
	$(LDSCRIPTDIR)/mips.ld
	
HFILES=							\
	$(wildcard include/*.h)				\
	$(wildcard include/ufs/*.h)	


ifeq ($(CONSOLE), altera)
LIBKERNEL_SRCS+=					\
	uart_altera.c
endif
ifeq ($(CONSOLE), gxemul)
LIBKERNEL_SRCS+=					\
	uart_gxemul.c
endif
ifeq ($(CONSOLE), malta)
LIBKERNEL_SRCS+=					\
	uart_malta.c
endif

LIBKERNEL_OBJS=						\
	$(addprefix $(OBJDIR)/,$(LIBKERNEL_ASMS:.S=.o))	\
	$(addprefix $(OBJDIR)/,$(LIBKERNEL_SRCS:.c=.o))

KERNEL_OBJS=						\
	$(addprefix $(OBJDIR)/,$(KERNEL_ASMS:.S=.o))	\
	$(addprefix $(OBJDIR)/,$(KERNEL_SRCS:.c=.o))
	
LINKKERNEL_OBJS=						\
	$(addprefix $(OBJDIR)/,$(LINKKERNEL_SRCS:.c=.o))
	
	
INCLUDEFLAGS = 						\
	-Iinclude/					\
	-I$(GINCDIR)

#
# Targets
#
TARGETS=						\
		$(OBJDIR)/libkernel.a			\
		$(OBJDIR)/kernel.elf			\
		$(OBJDIR)/kernel.dump

all: $(TARGETS)
clean:
	rm -f $(OBJDIR)/*

.PHONY: all clean
.SECONDARY: $(TARGETS) $(LIBKERNEL_OBJS) $(KERNEL_OBJS) $(LINKKERNEL_OBJS)

#
# Toolchain
#
LDFLAGS+=						\
	-T$(KERNEL_LDSCRIPT)

#
# Build rules
#

$(OBJDIR)/fs.o : src/fs.S $(OBJDIR)/../fs.img
	rm -f fs.img
	ln -s $(OBJDIR)/../fs.img fs.img
	$(AS) -c $(ASFLAGS) -o $@ $<
	rm -f fs.img

$(OBJDIR)/%.o : src/%.S
	$(AS) -c $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o : src/%.c $(HFILES)
	$(CC) -c $(CCFLAGS) -o $@ $<

KERNEL_DEPENDS=						\
	$(LIBKERNEL_OBJS)				\
	$(LINKKERNEL_OBJS)				\
	$(KERNEL_OBJS)					\
	$(LDSCRIPTDIR)/$(KERNEL_LDSCRIPT)		\
	$(OTHER_DEPENDS)

$(OBJDIR)/kernel.elf: $(KERNEL_DEPENDS)
	${LD} ${LDFLAGS} -o $@ $(KERNEL_OBJS) $(LIBKERNEL_OBJS) -luser

$(OBJDIR)/kernel.dump: $(OBJDIR)/kernel.elf
	${OBJDUMP} ${OBJDUMPFLAGS} $< > $@

LIBKERNEL_DEPENDS=					\
	$(LIBKERNEL_OBJS)				\
	$(LDSCRIPTDIR)/$(KERNEL_LDSCRIPT)		\
	$(OTHER_DEPENDS)

$(OBJDIR)/libkernel.a: $(LIBKERNEL_DEPENDS)
	${AR} ${ARFLAGS} $@ $(LIBKERNEL_OBJS)
