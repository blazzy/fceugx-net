.PHONY = default all wii gc wii-clean gc-clean wii-run gc-run

ifeq ($(strip $(DEVKITPRO)),)
export DEVKITPRO = $(realpath ./devkitpro)
export DEVKITPPC = $(DEVKITPRO)/devkitPPC
endif

default: wii server

all: wii gc server

clean: wii-clean gc-clean server

wii:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.wii

wii-clean:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.wii clean

wii-run:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.wii run

gc:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.gc

gc-clean:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.gc clean

gc-run:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.gc run

server:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.server

server-clean:
	@echo $@
	@$(MAKE) --no-print-directory -f Makefile.server clean
