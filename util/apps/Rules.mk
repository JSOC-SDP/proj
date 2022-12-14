# Standard things
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)

# Local variables
MODEXE_$(d)	:= $(addprefix $(d)/, arithtool rebin2 ingest_from_fits hg_patch render_image index_convert hmi_limbdark drms2hdir pfss_backup jsoc_resize jsoc_rebin im_patch  hmi_fixCROTA2 aia_fixCROTA2 export_as_maproj hmib2ptr jsoc_getJP2images set_gaps_missing)

NONDEFAULTEXE_$(d) := $(addprefix $(d)/, store_lmbackup_v2)

MODEXE		:= $(MODEXE) $(MODEXE_$(d)) $(NONDEFAULTEXE_$(d))

MODEXE_SOCK_$(d):= $(MODEXE_$(d):%=%_sock)
MODEXE_SOCK	:= $(MODEXE_SOCK) $(MODEXE_SOCK_$(d)) 

CEXE_$(d)	:= $(d)/time_convert
CEXE		:= $(CEXE) $(CEXE_$(d))

SRCOBJ_$(d)     := $(addprefix $(d)/src/, heliographic_coords.o)

EXE_$(d)	:= $(MODEXE_$(d)) $(CEXE_$(d)) 
OBJ_$(d)	:= $(EXE_$(d):%=%.o) $(NONDEFAULTEXE_$(d):%=%.o)
DEP_$(d)	:= $(OBJ_$(d):%=%.d)
CLEAN		:= $(CLEAN) \
		   $(OBJ_$(d)) \
		   $(EXE_$(d)) \
		   $(MODEXE_SOCK_$(d)) \
		   $(NONDEFAULTEXE_$(d)) \
		   $(DEP_$(d)) \
                   $(SRCOBJ_$(d))


TGT_BIN	        := $(TGT_BIN) $(EXE_$(d)) $(MODEXE_SOCK_$(d)) 

S_$(d)		:= $(notdir $(EXE_$(d)) $(MODEXE_SOCK_$(d)) $(NONDEFAULTEXE_$(d)))

# Local rules
$(OBJ_$(d)):	$(SRCDIR)/$(d)/Rules.mk
$(OBJ_$(d)):	CF_TGT := $(CF_TGT) -DCDIR="\"$(SRCDIR)/$(d)\"" -I$(SRCDIR)/$(d)/../../libs/imrotate/ -I$(SRCDIR)/$(d)/../../libs/astro -I$(SRCDIR)/$(d)/../../libs/stats
$(MODEXE_$(d)) $(MODEXE_SOCK_$(d)):      LL_TGT := $(LL_TGT) -lpng

$(MODEXE_$(d)) $(MODEXE_SOCK_$(d)):	$(LIBASTRO) $(LIBSTATS) $(LIBIMROTATE)

# Shortcuts
.PHONY:	$(S_$(d))
$(S_$(d)):	%:	$(d)/%

# Standard things
-include	$(DEP_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
