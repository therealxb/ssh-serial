#!/usr/bin/make -f
#
# $Id$

.PHONY: all
all: ssh-serial.8

ssh-serial.8: ssh-serial.8.dbk
	docbook2x-man ssh-serial.8.dbk

.PHONY: clean
clean:
	$(RM) ssh-serial.8
