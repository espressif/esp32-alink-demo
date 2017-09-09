#
# Component Makefile
#

ifdef CONFIG_ALINK_ENABLE

COMPONENT_ADD_INCLUDEDIRS := include adaptation/include
COMPONENT_SRCDIRS := adaptation application

ifdef CONFIG_ALINK_VERSION_EMBED
LIBS := $(dirname $(COMPONENT_PATH)) alink_agent tfspal
endif

ifdef CONFIG_ALINK_VERSION_SDS
LIBS := $(dirname $(COMPONENT_PATH)) alink_agent_sds tfspal
endif

COMPONENT_ADD_LDFLAGS += -L $(COMPONENT_PATH)/lib \
                           $(addprefix -l,$(LIBS))

endif
