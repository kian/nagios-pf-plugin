# $Id: Makefile 4 2007-03-08 06:18:34Z muskrat $
#
# Makefile for check_pf
#

CC = cc
CFLAGS  = -Wall
PROGRAM = check_pf
DESTDIR = /usr/local/libexec/nagios/

all:	$(PROGRAM)

$(PROGRAM):  
	$(CC) $(CFLAGS) -o $(PROGRAM) $(PROGRAM).c

install:
	install -m 755 -o root -g wheel $(PROGRAM) $(DESTDIR)/$(PROGRAM)

uninstall:
	rm -f $(DESTDIR)/$(PROGRAM)

clean:
	rm -f $(PROGRAM)
