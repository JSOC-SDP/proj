# Standard things
sp              := $(sp).x
dirstack_$(sp)  := $(d)
d               := $(dir)

# ALWAYS put libs subdirectory before other subdirectories.
ifeq ($(JSOC_MACHINE), linux_avx)
dir     := $(d)/libs
-include                $(SRCDIR)/$(dir)/Rules.mk
endif

# Subdirectories. Directory-specific rules are optional here. The
# order DOES matter, always define libraries before applications
# that use those libraries.
ifeq ($(JSOC_MACHINE), linux_avx)
dir     := $(d)/apps
-include                $(SRCDIR)/$(dir)/Rules.mk
endif

# Standard things
d               := $(dirstack_$(sp))
sp              := $(basename $(sp))
