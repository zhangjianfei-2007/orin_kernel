#
# Copyright (C) 2021 HYZX
#

KBUILD_OUTPUT =

DEBUG	=
CC	= $(CROSS_COMPILE)gcc

CFLAGS	= -Wall -I. $(DEBUG) $(EXTRA_CFLAGS)
#LDLIBS	= -lm -lrt -pthread $(EXTRA_LDFLAGS)
LDLIBS	= -lm $(EXTRA_LDFLAGS)
PRG	=   dac_demo  date_update   mcu_adc mcu_version

OBJECTS	=  mcu_adc.o  mcu_version.o dac_demo.o  titan4_io.o date_update.o 
SRC	= $(OBJECTS:.o=.c)
DEPEND	= $(OBJECTS:.o=.d)
srcdir	:= $(dir $(lastword $(MAKEFILE_LIST)))

VPATH	= $(srcdir)

prefix	= /usr/local
sbindir	= $(prefix)/sbin
mandir	= $(prefix)/man
man8dir	= $(mandir)/man8

all: $(PRG)



mcu_version:mcu_version.o

mcu_adc:mcu_adc.o

dac_demo: dac_demo.o titan4_io.o

date_update: date_update.o log.o

install: $(PRG)
	install -p -m 755 -d $(DESTDIR)$(sbindir) $(DESTDIR)$(man8dir)
	install $(PRG) $(DESTDIR)$(sbindir)
	for x in $(PRG:%=%.8); do \
		[ -f $$x ] && install -p -m 644 -t $(DESTDIR)$(man8dir) $$x ; \
	done

clean:
	rm -f $(OBJECTS) $(DEPEND) $(PRG)

# Implicit rule to generate a C source file's dependencies.
%.d: %.c
	@echo DEPEND $<; \
	rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPEND)
endif

.PHONY: all clean
