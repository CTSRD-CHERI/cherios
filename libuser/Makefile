NAME=	$(shell basename `pwd`)

OBJDIR:=$(OBJDIR)/$(NAME)

#
# Source files of various flavours.
#
ASMS=							\
	init.S						\
	memcpy_c.S


SRCS=							\
	crtbeginC.c					\
	crtendC.c					\
	assert.c					\
	ctype.c						\
	debug.c						\
	libuser.c					\
	malloc.c					\
	memset.c					\
	object.c					\
	printf.c					\
	ssleep.c					\
	strcmp.c					\
	strlen.c					\
	subr_prf.c
	
	
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
		$(OBJDIR)/$(NAME).a

all: $(TARGETS)
clean:
	rm -f $(OBJDIR)/*

.PHONY: all clean
.SECONDARY: $(TARGETS) $(OBJS)


#
# Build rules
#
$(OBJDIR)/memcpy_c.o : src/memcpy_c.S
	$(AS) -c $(ASFLAGS) -o $@ $< 2>/dev/null #No warning

$(OBJDIR)/%.o : src/%.S
	$(AS) -c $(ASFLAGS) -o $@ $<

$(OBJDIR)/%.o : src/%.c $(HFILES)
	$(CC) -c $(CCFLAGS) -o $@ $<

DEPENDS=						\
	$(OBJS)						\
	$(OTHER_DEPENDS)

$(OBJDIR)/$(NAME).a: $(DEPENDS)
	${AR} ${ARFLAGS} $@ $(OBJS)

