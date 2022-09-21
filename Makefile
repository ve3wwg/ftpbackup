# $Id$

VERS=2.1C

PROJDIR=ftpbackup-$(VERS)
TOPDIR=../$(PROJDIR)

INSTDIR=/usr/local

BIN_DIR=$(INSTDIR)/bin
MAN_DIR=$(INSTDIR)/man/man1
DOC_DIR=$(INSTDIR)/doc

#----------------------------------------------------------------------
# For non Linux/GNU-enabled hosts, comment out the gcc line, and
# uncomment the cc line

#CC=	cc
CC=	gcc

#----------------------------------------------------------------------
# Only the DEBUG or the OPTZ should be uncommented at one time.
# Unless you are debugging, you should probably uncomment the OPTZ
# line, and comment out the DEBUG line.

#DEBUG=	-g
OPTZ=	-O2

CCOPTS=	-c -DHAVE_GETOPT_LONG $(DEBUG) $(OPTZ) -Wall
LDOPTS=	$(DEBUG)

.c.o:
	$(CC) $(CCOPTS) $<

OBJ	= ftpbackup.o ftp.o ftpio.o getpw.o

all: $(OBJ)
	$(CC) $(LDOPTS) $(OBJ) -o ftpbackup

clean:
	rm -f *.o core t.t errs.t *.exe

install: ftpbackup
	install -d -o root -g root -m 755 $(INSTDIR)
	install -d -o root -g root -m 755 $(BIN_DIR)
	install -o root -g bin -m 111 ftpbackup $(BIN_DIR)
	install -d -o root -g root -m 755 $(MAN_DIR)
	install -o root -g root -m 444 ftpbackup.1 $(MAN_DIR)
	install -d -o root -g root -m 755 $(DOC_DIR)/$(PROJDIR)
	install -o root -g root -m 444 ftpbackup.lsm READ.ME CHANGES $(DOC_DIR)/$(PROJDIR)

dist: distclean
	cd ../trunk/.. && mkdir $(PROJDIR)
	cd ../trunk/.  && cp * ../$(PROJDIR)
	cd ../trunk/.. && tar czf $(PROJDIR).tar.gz $(PROJDIR)
	@cd ../trunk/.. && rm -fr $(PROJDIR)
	tar tzvf ../$(PROJDIR).tar.gz

distclean: clean
	cd ../trunk/.. && rm -fr $(PROJDIR) $(PROJDIR).tar $(PROJDIR).tar.gz

clobber: distclean

# $Source$
