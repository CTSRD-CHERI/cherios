NAME=	$(shell basename `pwd`)

OBJDIR:=$(OBJDIR)/$(NAME)

#
# Source files of various flavours.
#
ASMS=							\
	init.S

SRCS=							\
	main.c						\
	sockets.c					\
	
HFILES.=						\
	$(wildcard include/*.h)
	
OTHER_DEPENDS=						\

OBJS=							\
	$(addprefix $(OBJDIR)/,$(ASMS:.S=.o))		\
	$(addprefix $(OBJDIR)/,$(SRCS:.c=.o))
	
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

