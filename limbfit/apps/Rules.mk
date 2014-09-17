# Standard things
sp 			:= $(sp).x
dirstack_$(sp)		:= $(d)
d			:= $(dir)

# Local variables
MODEXE_USEF_$(d)	:= $(addprefix $(d)/, lfwrp)
SUPPOBJ_$(d)		:= $(addprefix $(d)/, limbfit do_one_limbfit)

MODEXE_USEF_TAS_$(d)    := $(addprefix $(d)/, lfwrp_tas)
SUPPOBJ_TAS_$(d)        := $(addprefix $(d)/, limbfit_tas do_one_limbfit_tas)

MODEXE_USEF_ANN_$(d)    := $(addprefix $(d)/, lfwrp_ann)
SUPPOBJ_ANN_$(d)        := $(addprefix $(d)/, do_one_limbfit_ann)

SUPPOBJ_COMM_$(d)	:= $(addprefix $(d)/, limb expmax expfit nrutil indexx sort)

MODEXE_USEF 		:= $(MODEXE_USEF) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_ANN_$(d))
#MODEXE_USEF_SOCK_$(d)	:= $(MODEXE_USEF_$(d):%=%_sock)
#MODEXE_USEF_SOCK	:= $(MODEXE_USEF_SOCK) $(MODEXE_USEF_SOCK_$(d))
SUPPOBJ_$(d)		:= $(SUPPOBJ_$(d):%=%.o)
SUPPOBJ_TAS_$(d)	:= $(SUPPOBJ_TAS_$(d):%=%.o)
SUPPOBJ_ANN_$(d)	:= $(SUPPOBJ_ANN_$(d):%=%.o)
SUPPOBJ_COMM_$(d)	:= $(SUPPOBJ_COMM_$(d):%=%.o)

OBJ_$(d)		:= $(MODEXE_USEF_$(d):%=%.o) $(MODEXE_USEF_TAS_$(d):%=%.o) $(MODEXE_USEF_ANN_$(d)) $(SUPPOBJ_$(d)) $(SUPPOBJ_TAS_$(d)) $(SUPPOBJ_ANN_$(d)) $(SUPPOBJ_COMM_$(d))
OBJ_$(d) :	 	CF_TGT := $(CF_TGT) -O3 -std=c99 -Wall

DEP_$(d)		:= $(OBJ_$(d):%=%.d)
CLEAN			:= $(CLEAN) \
			   $(OBJ_$(d)) \
			   $(MODEXE_USEF_$(d)) \
#			   $(MODEXE_USEF_SOCK_$(d)) \
			   $(DEP_$(d))

#TGT_BIN	        := $(TGT_BIN) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_SOCK_$(d))
TGT_BIN	        := $(TGT_BIN) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_ANN_$(d))

#S_$(d)			:= $(notdir $(MODEXE_USEF_$(d)) $(MODEXE_USEF_SOCK_$(d)))
S_$(d)			:= $(notdir $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_ANN_$(d)))

# Local rules
$(OBJ_$(d)):		$(SRCDIR)/$(d)/Rules.mk
$(OBJ_$(d)):		CF_TGT := $(CF_TGT) -DCDIR="\"$(SRCDIR)/$(d)\"" $(GSLH) 


$(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_ANN_$(d)):		LL_TGT := $(LL_TGT) $(GSLL) -lgsl -lgslcblas
$(MODEXE_USEF_$(d)):		$(SUPPOBJ_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_TAS_$(d)):	$(SUPPOBJ_TAS_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_ANN_$(d)):	$(SUPPOBJ_ANN_$(d)) $(SUPPOBJ_COMM_$(d))

# Shortcuts
.PHONY:	$(S_$(d))
$(S_$(d)):	%:	$(d)/%

# Standard things
-include	$(DEP_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
