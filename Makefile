# makefile for mini emacs, updated Fri Sep 12 2025

# Make the build silent by default
V =

ifeq ($(strip $(V)),)
	E = @echo
	Q = @
else
	E = @\#
	Q =
endif
export E Q

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

PROGRAM = em 
SRC     = emacs.c buffer.c display.c input.c
OBJDIR  = out
OBJ     = $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))
HDR     = includes/*
BINDIR  = /usr/local/bin

CC      = cc
WARNINGS= -Wall -Wextra
CFLAGS  = -g -O2 $(WARNINGS) -std=c99
LDFLAGS =
LIBS    = -lncurses

JUNK    = *.DS_Store *~ core a.out *.dSYM

# Default rule
all: $(PROGRAM)

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

# Ensure out/ exists, then build .o files there
$(OBJDIR)/%.o: %.c $(HDR)
	@mkdir -p $(OBJDIR)
	$(E) "  CC      " $<
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

clean:
	$(E) "  CLEAN"
	$(Q) rm -rf $(PROGRAM) $(OBJDIR) $(JUNK)

install: 
	$(E) "  INSTALL " $(PROGRAM)
	$(Q) cp $(PROGRAM) $(BINDIR)/$(PROGRAM)

run: $(PROGRAM)
	$(E) "  RUN     " $(PROGRAM)
	$(Q) ./$(PROGRAM)
