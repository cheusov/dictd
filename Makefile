# Makefile -- Makefile for dict
# Created: Fri Dec  2 10:47:28 1994 by faith@cs.unc.edu
# Revised: Thu Aug 24 01:03:32 1995 by r.faith@ieee.org
# Copyright 1994, 1995 Rickard E. Faith (faith@cs.unc.edu)
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 1, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 675 Mass Ave, Cambridge, MA 02139, USA.
#
# NOTE: This program is not distributed with any data.  This program
# operates on many different data types.  You are responsible for
# obtaining any data upon which this program operates.
#

prefix=/usr
exec_prefix=$(prefix)/bin
man1_prefix=$(prefix)/man/man1

CC=         gcc
CFLAGS=     -Wall -g -O
LDFLAGS=
INSTALL=    install
BIN=        dict buildindex compressdict

all: $(BIN)

# Makefile is stupid so that it will work with broken (e.g., non-GNU) makes

dict: dict.c look.o engine.o output.o decode.o dict.h
	$(CC) $(CFLAGS) $(LDFLAGS) \
		dict.c look.o engine.o output.o decode.o -o $@
look.o: look.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c look.c -o $@

decode.o: decode.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c decode.c -o $@

encode.o: encode.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c encode.c -o $@

engine.o: engine.c dict.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c engine.c -o $@

output.o: output.c dict.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c output.c -o $@

buildindex: buildindex.c
	$(CC) $(CFLAGS) $(LDFLAGS) buildindex.c -o $@

compressdict: compressdict.c decode.o encode.o dict.h
	$(CC) $(CFLAGS) $(LDFLAGS) compressdict.c decode.o encode.o -o $@

install: dict buildindex
	$(INSTALL) -m 755 dict $(exec_prefix)
	$(INSTALL) -m 755 buildindex $(exec_prefix)
	$(INSTALL) -m 755 compressdict $(exec_prefix)
	$(INSTALL) -m 644 dict.1 $(man1_prefix)
	$(INSTALL) -m 644 buildindex.1 $(man1_prefix)
	$(INSTALL) -m 644 compressdict.1 $(man1_prefix)

clean:
	-rm -f $(BIN) *.o *~ core

distclean veryclean: clean
