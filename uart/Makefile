NAME=	$(shell basename `pwd`)

OBJDIR:=$(OBJDIR)/$(NAME)

#
# Source files of various flavours.
#
ASMS=							\
	init.S

SRCS=							\
	main.c						\
	printf.c					\
	uart.c
	
HFILES.=						\
	$(wildcard include/*.h)
	
OTHER_DEPENDS=						\

OBJS=							\
	$(addprefix $(OBJDIR)/,$(ASMS:.S=.o))		\
	$(addprefix $(OBJDIR)/,$(SRCS:.c=.o))

#
# Console
#
ifeq ($(CONSOLE), altera)
SRCS+=							\
	uart_altera.c
endif
ifeq ($(CONSOLE), gxemul)
SRCS+=							\
	uart_gxemul.c
endif
ifeq ($(CONSOLE), malta)
SRCS+=							\
	uart_malta.c
endif
	
#
# Targets
#
TARGETS=						\
		$(OBJDIR)/$(NAME).elf			\
		$(OBJDIR)/$(NAME).dump

all: $(TARGETS)
clean:
	rm -f $(OBJDIR)/*

.PHONY: all clean
.SECONDARY: $(TARGETS) $(OBJS)

#
# Toolchain
#
LDFLAGS+=						\
	-Tsandbox.ld

#
# Build rules
#
$(OBJDIR)/%.o : src/%.S
	$(AS) -c $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o : src/%.c $(HFILES)
	$(CC) -c $(CCFLAGS) -o $@ $<

DEPENDS=						\
	$(OBJS)						\
	$(LDSCRIPT)					\
	$(OTHER_DEPENDS)

$(OBJDIR)/$(NAME).elf: $(DEPENDS)
	${LD} ${LDFLAGS} -o $@ $(OBJS) -luser

$(OBJDIR)/$(NAME).dump: $(OBJDIR)/$(NAME).elf
	${OBJDUMP} ${OBJDUMPFLAGS} $< > $@

