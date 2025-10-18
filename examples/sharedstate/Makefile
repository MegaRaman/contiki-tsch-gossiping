CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)
CONTIKI = ../../../../contiki-ng
MODULES_REL += gchmac
MODULES += os/net/nullnet

#use this to enable TSCH: MAKE_MAC = MAKE_MAC_TSCH
MAKE_MAC ?= MAKE_MAC_TSCH

MAKE_NET = MAKE_NET_NULLNET
include $(CONTIKI)/Makefile.include
