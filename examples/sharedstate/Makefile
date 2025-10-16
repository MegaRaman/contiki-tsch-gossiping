CONTIKI_PROJECT = sharedstate
all: $(CONTIKI_PROJECT)

MODULES += os/net/nullnet

CONTIKI = ../..
DEFINES = PROJECT_CONF_H=\"project-conf.h\"
DEFINES += NETSTACK_CONF_NETWORK=nullnet_driver

MAKE_MAC = MAKE_MAC_TSCH
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include
