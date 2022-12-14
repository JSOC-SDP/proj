# Standard things
sp 			:= $(sp).x
dirstack_$(sp)		:= $(d)
d			:= $(dir)

# Local variables
MODEXE_USEF_$(d)	:= $(addprefix $(d)/, lfwrp)
SUPPOBJ_$(d)		:= $(addprefix $(d)/, limbfit.o do_one_limbfit.o limb.o)

MODEXE_USEF_TAS_$(d)    := $(addprefix $(d)/, lfwrp_tas)
# the following fails
MODEXE_USEF_TAS2_$(d)   := $(addprefix $(d)/, lfwrp_tas2)
MODEXE_USEF_TAS512_$(d)         := $(addprefix $(d)/, lfwrp_tas512)
MODEXE_USEF_TAS1024_$(d)        := $(addprefix $(d)/, lfwrp_tas1024)
SUPPOBJ_TAS_$(d)        := $(addprefix $(d)/, limbfit_tas.o do_one_limbfit_tas.o limb.o)
SUPPOBJ_TAS512_$(d)	:= $(addprefix $(d)/, limbfit_tas.o do_one_limbfit_tas.o limb_ab512.o)
SUPPOBJ_TAS1024_$(d)	:= $(addprefix $(d)/, limbfit_tas.o do_one_limbfit_tas.o limb_ab1024.o)

MODEXE_ANN_$(d)    	:= $(addprefix $(d)/, lfwrp_ann)
SUPPOBJ_ANN_$(d)        := $(addprefix $(d)/, do_one_limbfit_ann.o)

SUPPOBJ_COMM_$(d)	:= $(addprefix $(d)/, expmax.o expfit.o nrutil.o indexx.o sort.o)

MODEXE_USEF 		:= $(MODEXE_USEF) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_TAS2_$(d)) $(MODEXE_USEF_TAS512_$(d)) $(MODEXE_USEF_TAS1024_$(d))
MODEXE			:= $(MODEXE) $(MODEXE_ANN_$(d))

OBJ_$(d)		:= $(MODEXE_USEF_$(d):%=%.o) $(MODEXE_USEF_TAS_$(d):%=%.o) $(MODEXE_USEF_TAS2_$(d):%=%.o) $(MODEXE_USEF_TAS512_$(d):%=%.o) $(MODEXE_USEF_TAS1024_$(d):%=%.o) $(MODEXE_ANN_$(d):%=%.o) $(SUPPOBJ_$(d)) $(SUPPOBJ_TAS_$(d)) $(SUPPOBJ_TAS512_$(d)) $(SUPPOBJ_TAS1024_$(d)) $(SUPPOBJ_ANN_$(d)) $(SUPPOBJ_COMM_$(d)) 

OBJ_$(d) :	 	CF_TGT := $(CF_TGT) -O3 -std=c99 -Wall

DEP_$(d)		:= $(OBJ_$(d):%=%.d)
CLEAN			:= $(CLEAN) \
			   $(OBJ_$(d)) \
			   $(MODEXE_USEF_$(d)) \
			   $(MODEXE_USEF_TAS_$(d)) \
			   $(MODEXE_USEF_TAS2_$(d)) \
			   $(MODEXE_USEF_TAS512_$(d)) \
			   $(MODEXE_USEF_TAS1024_$(d)) \
			   $(MODEXE_ANN_$(d)) \
			   $(DEP_$(d))

TGT_BIN	 	       	:= $(TGT_BIN) $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_TAS2_$(d)) $(MODEXE_USEF_TAS512_$(d)) $(MODEXE_USEF_TAS1024_$(d)) $(MODEXE_ANN_$(d))
S_$(d)			:= $(notdir $(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_TAS2_$(d)) $(MODEXE_USEF_TAS512_$(d)) $(MODEXE_USEF_TAS1024_$(d)) $(MODEXE_ANN_$(d)))

# Local rules
$(OBJ_$(d)):		$(SRCDIR)/$(d)/Rules.mk
$(OBJ_$(d)):		CF_TGT := $(CF_TGT) -DCDIR="\"$(SRCDIR)/$(d)\"" $(GSLH) 

$(MODEXE_USEF_TAS512_$(d):%=%.o):	CF_TGT := $(CF_TGT) -DAB512
$(MODEXE_USEF_TAS1024_$(d):%=%.o):	CF_TGT := $(CF_TGT) -DAB1024

# Save the 512 verson of lfwrp_tas2.c to lfwrp_tas512.o, and the 1024 version to lfwrp_tas1024.o.
$(MODEXE_USEF_TAS512_$(d):%=%.o):	$(MODEXE_USEF_TAS2_$(d):%=%.c)
					$(COMP)
$(MODEXE_USEF_TAS1024_$(d):%=%.o):	$(MODEXE_USEF_TAS2_$(d):%=%.c)
					$(COMP)

$(MODEXE_USEF_$(d)) $(MODEXE_USEF_TAS_$(d)) $(MODEXE_USEF_TAS2_$(d)) $(MODEXE_ANN_$(d)):		LL_TGT := $(LL_TGT) $(GSLL) -lgsl -lgslcblas
$(MODEXE_USEF_TAS512_$(d)) $(MODEXE_USEF_TAS1024_$(d)):			LL_TGT := $(LL_TGT) $(GSLL) -lgsl -lgslcblas

$(MODEXE_USEF_$(d)):		$(SUPPOBJ_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_TAS_$(d)):	$(SUPPOBJ_TAS_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_TAS2_$(d)):	$(SUPPOBJ_TAS_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_TAS512_$(d)):	$(SUPPOBJ_TAS512_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_USEF_TAS1024_$(d)):	$(SUPPOBJ_TAS1024_$(d)) $(SUPPOBJ_COMM_$(d))
$(MODEXE_ANN_$(d)):		$(SUPPOBJ_ANN_$(d)) $(SUPPOBJ_COMM_$(d))

# Shortcuts
.PHONY:	$(S_$(d))
$(S_$(d)):	%:	$(d)/%

# Standard things
-include	$(DEP_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
