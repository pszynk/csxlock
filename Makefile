# csxlock - simple colored X screen locker
# © 2016 Safronov Kirill
# Based on csxlock
# © 2013-2016 Jakub Klinkovský
# Based on sflock
# © 2010-2011 Ben Ruijl
# Based on slock
# © 2006-2008 Anselm R. Garbe, Sander van Dijk

NAME := csxlock
VERSION := $(shell ./version.sh)

CC := $(CC) -std=c99

base_CFLAGS := -Wall -Wextra -pedantic -O2
base_LIBS := -lpam

pkgs := x11 xext xrandr
pkgs_CFLAGS := $(shell pkg-config --cflags $(pkgs))
pkgs_LIBS := $(shell pkg-config --libs $(pkgs))

CPPFLAGS += -DPROGNAME=\"${NAME}\" -DVERSION=\"${VERSION}\" -D_XOPEN_SOURCE=500
CFLAGS := $(base_CFLAGS) $(pkgs_CFLAGS) $(CFLAGS)
LDLIBS := $(base_LIBS) $(pkgs_LIBS)

all: csxlock

csxlock: csxlock.c

clean:
	$(RM) csxlock

install: csxlock
	install -Dm4755 csxlock $(DESTDIR)/usr/bin/csxlock
	install -Dm644 csxlock.pam $(DESTDIR)/etc/pam.d/csxlock

nosuidinstall: csxlock
	install -Dm755 csxlock $(DESTDIR)/usr/bin/csxlock
	install -Dm644 csxlock.pam $(DESTDIR)/etc/pam.d/csxlock

remove:
	rm -f $(DESTDIR)/usr/bin/csxlock
	rm -f $(DESTDIR)/etc/pam.d/csxlock

.PHONY: all clean install nosuidinstall remove
