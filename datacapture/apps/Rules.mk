# Standard things
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)

# Local variables

# Force icc compilation since this doesn't work for gcc because libhmicomp_egse was built with icc.
LOCALCC		= $(ICC_COMP)
LOCALLN		= $(ICC_LINK)

EXE_$(d)	:= $(addprefix $(d)/, ingest_tlm soc_pipe_scp)

OBJ_$(d)	:= $(EXE_$(d):%=%.o)
DEP_$(d)	:= $(EXE_$(d):%=%.o.d)

CLEAN		:= $(CLEAN) \
		   $(OBJ_$(d)) \
		   $(EXE_$(d)) \
		   $(DEP_$(d))

TGT_BIN	        := $(TGT_BIN) $(EXE_$(d))

S_$(d)		:= $(notdir $(EXE_$(d)))

ifeq ($(HOST),dcs0.jsoc.Stanford.EDU)
        ADD_TGT_$(d) := -DSUMDC -DDCS0
endif
ifeq ($(HOST),dcs1.jsoc.Stanford.EDU)
        ADD_TGT_$(d) := -DSUMDC -DDCS1
endif
ifeq ($(HOST),dcs2.jsoc.Stanford.EDU)
        ADD_TGT_$(d) := -DSUMDC -DDCS2
endif
ifeq ($(HOST),dcs3.jsoc.Stanford.EDU)
        ADD_TGT_$(d) := -DSUMDC -DDCS3
endif

# Local rules
# do not use $(LIBEGSEHMICOMP) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
# libegsehmicomp must go before other libs due to collision of definitions of functions
$(EXE_$(d)):	proj/libs/egsehmicomp/libegsehmicomp.a

# do not use $(LIBSUMSAPI) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
# do not use $(LIBCJSON) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
# do not use $(LIBSUM) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
# do not use $(LIBDSTRUCT) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
# do not use $(LIBMISC) since we can't be sure if its Rules.mk, which is where
# this variable gets set, has been read yet
$(EXE_$(d)):	base/sums/libs/api/libsumsapi.a base/libs/cjson/libcjson.a base/sums/libs/pg/libsumspg.a base/libs/dstruct/libdstruct.a base/libs/misc/libmisc.a

$(OBJ_$(d)):	$(SRCDIR)/$(d)/Rules.mk
$(OBJ_$(d)):	CF_TGT := $(CF_TGT) $(ADD_TGT_$(d)) -DCDIR="\"$(SRCDIR)/$(d)\"" -I$(SRCDIR)/$(d)/../../libs/egsehmicomp
$(OBJ_$(d)):	%.o:	%.c
		$(LOCALCC)

$(EXE_$(d)):	LL_TGT := -L $(POSTGRES_LIBS) -lecpg -lpq

$(EXE_$(d)):	%:	%.o $(EXELIBS)
		$(LOCALLN)
		$(SLBIN)

# Shortcuts
.PHONY:	$(S_$(d))
$(S_$(d)):	%:	$(d)/%

# Standard things
-include	$(DEP_$(d))

d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
