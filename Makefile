.PHONY = default all wii gc wii-clean gc-clean wii-run gc-run

ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO = $(realpath ./devkitpro)
export DEVKITPPC = $(DEVKITPRO)/devkitPPC
endif

default: wii server

all: wii gc server

clean: wii-clean gc-clean server

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run:
	$(MAKE) -f Makefile.wii run

gc:
	$(MAKE) -f Makefile.gc

gc-clean:
	$(MAKE) -f Makefile.gc clean

gc-run:
	$(MAKE) -f Makefile.gc run

server:
	$(MAKE) --no-print-directory -f Makefile.server

server-clean:
	$(MAKE) --no-print-directory -f Makefile.server clean
