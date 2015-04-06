#!/usr/bin/make -f
#
# $Id$

CFLAGS=-g

.PHONY: all
all: ssh-serial

ssh-serial: ssh-serial.o

ssh-serial.o: ssh-serial.c

.PHONY: clean
clean:
	$(RM) ssh-serial.o
	$(RM) ssh-serial
