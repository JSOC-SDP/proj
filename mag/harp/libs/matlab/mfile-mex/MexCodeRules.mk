#
#	Rules and Definitions for MEXing
#
# mjt 13 march 1996
# mjt july 1997 revision for matlab v5
# mjt nov 2002 revision for c++, mex2c
# mjt sep 2009 revision for autogenerated docstrings
#

# cmex loading (these macros can be re-set by the invoking Makefile)
CMEX = mex -v
CMEXFLAGS = -outdir $(OUTDIR)/$(CSDIR)
CMEXLDFLAGS =
CMEXPPFLAGS = 'CC='$(MEX_CC) 'CFLAGS=$$CFLAGS -std=c99'
LINK.cmex = $(CMEX) $(CMEXFLAGS) $(CMEXPPFLAGS) $(CMEXLDFLAGS)
COMPILE.cmex = $(CMEX) -c $(CMEXFLAGS) $(CMEXPPFLAGS)

# make .mexFOO from .c (C sources)
#%.$(MEXEXT):	$(OUTDIR)/$(CSDIR)/%.$(MEXEXT):	Doc/%_docstring.h

# PROBLEM - you must use -outdir $(OUTDIR)/$(CSDIR) otherwise
# the compilation of .o from .c will result in files being placed
# in /home/arta/jsoctrees/JSOC/_linux_x86_64. For some reason,
# mex thinks this is the output/working directory, even though
# make thinks the output/working directory is
# /home/arta/jsoctrees/JSOC/_linux_x86_64/proj/mag/harp/libs/matlab/mfile-mex/standalone.
# But setting the -outdir argument
# causes problems with where mex puts the .mexa64 files. For some
# reason mex pre-pends the outdir to the FULL PATH to the mexa64 files.
# It doesn't do that with the .o files. I think what mex is doing
# is that is ALWAYS pre-pends its notion of OUTDIR to the target files.
# When doing a compile and link, the .o files have only a base name,
# so pre-pending the OUTDIR is the correct thing to do. But the .mexa64
# files, as listed in the target, have a full path already, so pre-pending
# OUTDIR is the wrong thing to do. And you cannot try to separate the
# compile from the link because the link will cause additional files to
# be compiled, like mexversion.o. If you try to remove -outdir from
# the link part of the make, then when mexversion.o is compiled, it
# will be placed in /home/arta/jsoctrees/JSOC/_linux_x86_64, because
# mex thinks the OUTDIR is /home/arta/jsoctrees/JSOC/_linux_x86_64.

# Kind of works when -outdir $(OUTDIR)/$(CSDIR) is set, but has problems
# as discussed above with the mexa64 paths.
# $(OUTDIR)/$(CSDIR)/%.$(MEXEXT):	%.c Doc/%_docstring.h
#	$(LINK.cmex) $(OUTPUT_OPTION) $<

# This works, despite its inelegance!
#
# The files %.$(MEXEXT) do not ever exist. They are needed because mex needs them. mex prepends what it considers the output
# directory to these targets. If instead the rule that includes the mex recipe had $(OUTDIR)/$(CSDIR)/%.$(MEXEXT) as the target
# (which is what appears as a prerequisite in another target), then mex would attempt to create $(OUTDIR)/$(CSDIR)/$(OUTDIR)/$(CSDIR)/%.$(MEXEXT).
# When mex creates the .o from the .c files, it also prepends the output directory to the .o files. So the rules for the %.$(MEXEXT) targets below
# work just fine, except that the files %.$(MEXEXT) never exist! To cope with this, I created a .PHONY rule for %.$(MEXEXT). This way,
# whenever there is an attempt to make $(OUTDIR)/$(CSDIR)/%.$(MEXEXT), this will result in an attempt to make %.$(MEXEXT), which will
# ALWAYS result in the build of the .o files from the .c files, and it will create the *.mexa64 files (even if the *.mexa64 files
# somehow ended up existing in /home/arta/jsoctrees/JSOC/_linux_x86_64, which is the place where make is looking for them).
#
# Apparently, every single target must have a recipe - the rule for $(OUTDIR)/$(CSDIR)/%.$(MEXEXT) is no exception. If you don't provide
# a recipe in any target, then make does not know how to build the target, and when it comes time to build the target (because it
# appears as a prerequisite in another rule), make will fail. But to make $(OUTDIR)/$(CSDIR)/%.$(MEXEXT) from %.$(MEXEXT), make doesn't
# have to do anything because %.$(MEXEXT) doesn't really exist. So, instead of providing a recipe that does something, I just put a
# semicolon at the end of the prerequsite for $(OUTDIR)/$(CSDIR)/%.$(MEXEXT) - an "empty" recipe that does nothing. If you
# do not provide a recipe at all, then thinks there is no rule for $(OUTDIR)/$(CSDIR)/%.$(MEXEXT) at all, as mentioned already.
#
.PHONY: %.$(MEXEXT)
%.$(MEXEXT): %.c Doc/%_docstring.h
	$(LINK.cmex) $(OUTPUT_OPTION) $<
# LHS - full path /home/arta/jsoctrees/JSOC/_linux_x86_64/proj/mag/harp/libs/matlab/mfile-mex/standalone/*.mexa64
# RHS - base name *.mexa64 (these files do not actually exist - they would be /home/arta/jsoctrees/JSOC/_linux_x86_64/*.mexa64)
$(OUTDIR)/$(CSDIR)/%.$(MEXEXT): %.$(MEXEXT) ;
# end this works

# This doesn't work
#$(OUTDIR)/$(CSDIR)/%.$(MEXEXT):	%.c Doc/%_docstring.h
#	$(CMEX) -c $(CMEXFLAGS) -outdir $(OUTDIR)/$(CSDIR) $(CMEXPPFLAGS) $(OUTPUT_OPTION) $<
#	$(LINK.cmex) $(OUTPUT_OPTION) $(OUTDIR)/$(CSDIR)/$(<:%.c=%.o)

#
# some extras for the mex2c interface
#

# libs for use in command line interface
# (these macros can be re-set by the invoking Makefile)
# FITSIO_LIBS = -lcfitsio -lsocket -lnsl # on solaris (old)
FITSIO_LIBS = -lcfitsio_gcc_fpic -lcurl# in JSOC
# FITSIO_LIBS = -lcfitsio -lSystem # on darwin
MEX2C_LIBS =

# to use the rules below, the user must define where the includes
# and libraries are, and insert into CPPFLAGS and LDFLAGS, e.g.
## COMMON = ../../mex
## LDFLAGS += -L$(COMMON)/lib
## CPPFLAGS += -I$(COMMON)/include2
## MEX2C_LIBS = -lmexrng -lmextools -lm

# make MW.o from .c (C sources -> matlab-compatible .o)
# ART - the only library that this applies to is libhmihelpMW.a in hmi-mask-patch
$(OUTDIR)/$(CSDIR)/%MW.o : %.c Doc/%_docstring.h
	$(COMPILE.cmex) "CFLAGS=-DUSING_MEX2LIB -DMEX2C_TAIL_HOOK -DmexFunction=local_$* -I$(OUTDIR)/$(CSDIR)/../../mex/src/util/"' $$CFLAGS' $<
	mv $(OUTDIR)/$(CSDIR)/$*.o $@

# make libX.a from .c (C sources -> library)
# ART - I don't think this rule is currently used. The only non-MW library built
# in the subdirectories is libmatch.a, and there is no match.c.
lib%.a: %.c Doc/%_docstring.h
	$(COMPILE.c) -DMEX2C_TAIL_HOOK -DUSING_MEX2LIB -DStaticP=static $<
	ar cr $@ $*.o
	$(RM) $*.o
