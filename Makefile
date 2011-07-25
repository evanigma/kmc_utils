#
# Makefile for kmc_utils 0.3.2
#

VERSION = 0.3.2
RPM_VERSION = 1

# **************** You may want to change these paths ****************
MANDIR = /usr/local/man
BINDIR = /usr/bin
DOCDIR = /usr/doc

# *********** You should not need to edit below this line ************
KMCDOCDIR = $(DOCDIR)/kmc_utils-$(VERSION)
TARDIR = kmc_utils-$(VERSION)
TARSTUFF = BUGS CHANGELOG LICENSE README TODO Makefile \
		kmc_scsi.c kmc_serial.c kmc_serial.h \
		kmc_read.c kmc_control.c kmc_speedtest.c kmc_play.c \
		man8 xpm

CC = gcc
CFLAGS = -g -O2 $(EXTRA)
LDIRS = -L/usr/X11R6/lib -L.
GTK_CFLAGS = -I/usr/X11R6/include -I/usr/lib/glib/include -pg
GTK_LIBS = -lgtk -lgdk -rdynamic -lgmodule -lglib -ldl -lXext -lX11 -lm -lkmc
GTK_LDIRS = -L/usr/lib -L/usr/X11R6/lib -L.



# kmc_read	This is the command line executable

all: kmc_read kmc_control kmc_speedtest

kmc_read: kmc_read.c libkmc.a 
	$(CC) $(CFLAGS) $(LDIRS) kmc_read.c -o kmc_read -lkmc

kmc_control: kmc_control.c libkmc.a 
	$(CC) $(CFLAGS) $(LDIRS) kmc_control.c -o kmc_control -lkmc

libkmc.a: kmc_scsi.o kmc_serial.o
	ar rs libkmc.a kmc_scsi.o kmc_serial.o

kmc_scsi.o: kmc_scsi.c 
	$(CC) -c $(CFLAGS) $*.c 

kmc_serial.o: kmc_serial.c 
	$(CC) -c $(CFLAGS) $*.c 

kmc_speedtest: kmc_speedtest.c libkmc.a
	$(CC) $(CFLAGS) $(LDIRS) kmc_speedtest.c -o kmc_speedtest -lkmc

kmc_play: kmc_play.c libkmc.a
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(LDIRS) $(GTK_LDIRS) $(GTK_LIBS) kmc_play.c -o kmc_play -lkmc

clean: 
	rm -f *.o *.a *.so kmc_read kmc_play kmc_speedtest kmc_control

install: 
	cp kmc_read $(BINDIR)
	cp kmc_control $(BINDIR)
	cp kmc_speedtest $(BINDIR)
	cp man8/kmc_read.8 $(MANDIR)/man8
	cp man8/kmc_control.8 $(MANDIR)/man8
	if [ ! -d $(KMCDOCDIR) ]; then mkdir -p $(KMCDOCDIR); fi
	cp README LICENSE TODO BUGS CHANGELOG $(KMCDOCDIR)

uninstall:
	rm -f $(BINDIR)/kmc_speedtest
	rm -f $(BINDIR)/kmc_read
	rm -f $(MANDIR)/man8/kmc_read.8
	rm -f $(MANDIR)/man8/kmc_control.8
	rm -rf $(KMCDOCDIR)

tar: 
	if [ ! -d $(TARDIR) ]; then mkdir -p $(TARDIR); fi
	cp -a $(TARSTUFF) $(TARDIR)
	tar cvzf kmc_utils-$(VERSION).tgz $(TARDIR)
	rm -rf $(TARDIR)

rpm: all tar kmc_utils-$(VERSION)-$(RPM_VERSION).spec
	cp kmc_utils-$(VERSION).tgz /usr/src/redhat/SOURCES
	cp kmc_utils-$(VERSION)-$(RPM_VERSION).spec /usr/src/redhat/SPECS
	cd /usr/src/redhat/SPECS
	rpm -ba kmc_utils-$(VERSION)-$(RPM_VERSION).spec
	cp /usr/src/redhat/RPMS/*/kmc_utils-$(VERSION)-$(RPM_VERSION).*.rpm .
	cp /usr/src/redhat/SRPMS/kmc_utils-$(VERSION)-$(RPM_VERSION).src.rpm .

distrib: rpm
	rm -rf kmc_utils-download
	mkdir kmc_utils-download
	cp kmc_utils-$(VERSION)-$(RPM_VERSION).*.rpm kmc_utils-download
	cp kmc_utils-$(VERSION)-$(RPM_VERSION).src.rpm kmc_utils-download
	cp kmc_utils-$(VERSION).tgz kmc_utils-download
	cp CHANGELOG kmc_utils-download
