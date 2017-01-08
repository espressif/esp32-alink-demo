#
# Component Makefile
#

COMPONENT_SRCDIRS := src/platform src/product src/app
COMPONENT_ADD_INCLUDEDIRS := include include/platform include/product include/app

LIBS := alink-sdk
COMPONENT_ADD_LDFLAGS += -L./lib $(addprefix -l,$(LIBS))
