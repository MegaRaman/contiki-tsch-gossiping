CONTIKI_PROJECT = initializer 
all: $(CONTIKI_PROJECT)

CONTIKI = ../..
DEFINES = PROJECT_CONF_H=\"project-conf.h\"

# Force NullMAC for native builds
#MAKE_MAC = MAKE_MAC_NULL
# Add these lines to your Makefile
MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET

# Explicitly include nullmac.c for native target
# CONTIKI_TARGET_SOURCEFILES += core/mac/nullmac/nullmac.c

include $(CONTIKI)/Makefile.include