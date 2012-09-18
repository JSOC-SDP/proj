# Standard things
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)

# Local variables

# Common utilities
EXTRADEPS_$(d)		:= $(addprefix $(d)/, )

# NOTE: Add the base of the module's filename below (next to mymod)
MODEXE_$(d)	:= $(addprefix $(d)/, fdlos2radial resizemappingmag)
MODEXE		:= $(MODEXE) $(MODEXE_$(d))

MODEXE_SOCK_$(d):= $(MODEXE_$(d):%=%_sock)
MODEXE_SOCK	:= $(MODEXE_SOCK) $(MODEXE_SOCK_$(d))

# Modules with external libraries
MODEXE_USEF_$(d)	:= $(addprefix $(d)/, mag2helio)
MODEXE_USEF		:= $(MODEXE_USEF) $(MODEXE_USEF_$(d))

MODEXE_USEF_SOCK_$(d)	:= $(MODEXE_USEF_$(d):%=%_sock)
MODEXE_USEF_SOCK	:= $(MODEXE_USEF_SOCK) $(MODEXE_USEF_SOCK_$(d))

EXE_$(d)	:= $(MODEXE_$(d)) $(MODEXE_USEF_$(d))
OBJ_$(d)	:= $(EXE_$(d):%=%.o) 
DEP_$(d)	:= $(OBJ_$(d):%=%.d)
CLEAN		:= $(CLEAN) \
		   $(OBJ_$(d)) \
		   $(EXE_$(d)) \
		   $(MODEXE_SOCK_$(d))\
		   $(MODEXE_USEF_SOCK_$(d)) \
		   $(DEP_$(d))

TGT_BIN	        := $(TGT_BIN) $(EXE_$(d)) $(MODEXE_SOCK_$(d)) $(MODEXE_USEF_SOCK_$(d))

S_$(d)		:= $(notdir $(EXE_$(d)) $(MODEXE_SOCK_$(d)))

# Local rules
$(OBJ_$(d)):		$(SRCDIR)/$(d)/Rules.mk
$(OBJ_$(d)):		CF_TGT := -I$(SRCDIR)/$(d)/../../../libs/astro -I$(SRCDIR)/$(d)/../../../libs/stats -I$(SRCDIR)/$(d)/src/ $(FMATHLIBSH) -I$(SRCDIR)/lib_third_party/include
$(OBJ_$(d)):		CF_TGT := $(CF_TGT) -DCDIR="\"$(SRCDIR)/$(d)\""

$(EXTRADEPS_$(d)):	CF_TGT := $(CF_TGT) -I/home/jsoc/include -I$(SRCDIR)/$(d) -openmp

MKL     := -lmkl

ifeq ($(COMPILER), icc)
  NOIPO_$(d)    := -no-ipo
endif

ifeq ($(JSOC_MACHINE), linux_x86_64)
  MKL     := $(NOIPO_$(d)) -lmkl_em64t
endif

ifeq ($(JSOC_MACHINE), linux_ia32)
  MKL     := $(NOIPO_$(d)) -lmkl_lapack -lmkl_ia32
endif

SVML_$(d)       :=
GUIDE_$(d)      :=

ifeq ($(COMPILER), icc)
  SVML_$(d)     := #-lsvml
  GUIDE_$(d)    := #-lguide
endif

ALL_$(d)	:= $(MODEXE_$(d)) $(MODEXE_SOCK_$(d)) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_SOCK_$(d))
$(ALL_$(d)) : $(EXTRADEPS_$(d))
$(ALL_$(d)) : $(LIBASTRO) $(LIBSTATS)
$(ALL_$(d)) : LF_TGT := $(LF_TGT) -openmp $(MKL)
$(ALL_$(d)) : LL_TGT := $(LL_TGT) $(GSLLIBS) $(CFITSIOLIBS) $(FMATHLIBS) $(SVML_$(d)) $(MKL) $(GUIDE_$(d))

# Shortcuts
.PHONY:	$(S_$(d))
$(S_$(d)):	%:	$(d)/%

# Standard things
ifneq ($(DEP_$(d)),)
  -include	$(DEP_$(d))
endif

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))