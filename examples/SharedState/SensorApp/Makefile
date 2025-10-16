CONTIKI_PROJECT = node 
all: $(CONTIKI_PROJECT)

MODULES += os/services/sharedstate
MODULES += os/net/nullnet

CONTIKI = ../../..
DEFINES = PROJECT_CONF_H=\"project-conf.h\"
DEFINES += NETSTACK_CONF_NETWORK=nullnet_driver


include $(CONTIKI)/Makefile.include