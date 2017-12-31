#
# Copyright (C) 2017 Vincent Sallaberry
# vlib <https://github.com/vsallaberry/vlib>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
############################################################################################
#
# Simple C library for simple programs.
#
# Makefile
#
############################################################################################
# PROJECT SPECIFIC PART
#############################################################################################

# SRCDIR: Folder where sources are. Use '.' for current directory. MUST NEVER BE EMPTY !!
SRCDIR 		= src

# INCDIR: Folder where public includes are. It can be SRCDIR.
# Use '.' for current directory. MUST NEVER BE EMPTY !!
INCDIRS 	= include

# Where targets are created (OBJs, BINs, ...). Eg: '.' or 'build'. ONLY 'SRCDIR' is supported!
BUILDDIR	= $(SRCDIR)

# Name of the Package (DISTNAME, BIN, LIB depends on it)
NAME		= vlib

# Binary name and package name (prefix with '$(BUILDDIR)/' to put it in build folder).
# Fill LIB and set BIN empty to create a library, or clear LIB and set BIN to create a binary.
BIN		=
LIB		= lib$(NAME).a

# DISTDIR: where the dist packages zip/tar.xz are saved
DISTDIR		= ../../dist

# Project specific Flags (system specific flags are handled later)
WARN		= -Wall -W -pedantic # -Wno-ignored-attributes -Wno-attributes
ARCH		= -march=native # -arch i386 -arch x86_64
OPTI		= -O3 -pipe
DEBUG		= -Werror -O0 -ggdb3 -D_DEBUG -D_TEST
INCS		=
LIBS		=
FLAGS_C		= -std=c99 -D_GNU_SOURCE
FLAGS_CXX	= -D_GNU_SOURCE
FLAGS_OBJC	=

# System specific flags (WARN_$(sys),OPTI_$(sys),DEBUG_$(sys),LIBS_$(sys),INCS_$(sys))
# $(sys) is lowcase(`uname`), eg: LIBS_darwin	= -framework IOKit -framework Foundation

############################################################################################
# GENERIC PART
############################################################################################

AR		= ar
RANLIB		= ranlib
GREP		= grep
WHICH		= which
HEADN1		= head -n1
PRINTF		= printf
AWK		= awk
SED		= sed
GREP		= grep
RM		= rm -f
DATE		= date
TAR		= tar
ZIP		= zip
FIND		= find
PKGCONFIG	= pkg-config
TEST		= test
SORT		= sort
RMDIR		= rmdir
TOUCH		= touch
MV		= mv
TR		= tr

NO_STDERR	= 2> /dev/null

############################################################################################
# - On recent gnu make, "!=' is understood.
# - On gnu make 3.81 '!=' is not understood but it does NOT cause syntax error.
# - On {open,free,net}bsd $(shell cmd) is not understood but does NOT cause syntax error.
# - On gnu make 3.82, '!=' causes syntax error, then it is at the moment only case where
#   make-fallback is needed (make-fallback removes lines which cannot be parsed).
# Assuming that, the command to be run is put in a variable (cmd_...),
# then the '!=' is tried, and $(shell ..) will be done only on '!=' failure (?= $(shell ..).
# It is important to use a temporary name, like tmp_CC because CC is set by make at startup.
#
cmd_CC		= $(WHICH) $${CC} clang gcc cc $(CC) $(NO_STDERR) | $(HEADN1)
cmdCCtmp	:= $(cmd_CC)
tmp_CC		!= $(cmd_CC)
tmp_CC		?= $(shell $(cmd_CC))
CC		:= $(tmp_CC)

cmd_CXX		= $(WHICH) $${CXX} clang++ g++ c++ $(CXX) $(NO_STDERR) | $(HEADN1)
tmp_CXX		!= $(cmd_CXX)
tmp_CXX		?= $(shell $(cmd_CXX))
CXX		:= $(tmp_CXX)

CPP		= $(CC) -E
OBJC		= $(CC)

cmd_TIMESTAMP	= $(DATE) '+%Y-%m-%d_%Hh%M'
tmp_TIMESTAMP	!= $(cmd_TIMESTAMP)
tmp_TIMESTAMP	?= $(shell $(cmd_TIMESTAMP))
TIMESTAMP	:= $(tmp_TIMESTAMP)

cmd_SHELL	:= $(WHICH) bash sh $(SHELL) $(NO_STDERR) | $(HEADN1)
tmp_SHELL	!= $(cmd_SHELL)
tmp_SHELL	?= $(shell $(cmd_SHELL))
SHELL		:= $(tmp_SHELL)

cmd_UNAME_SYS	= uname | $(TR) '[A-Z]' '[a-z]'
tmp_UNAME_SYS	!= $(cmd_UNAME_SYS)
tmp_UNAME_SYS	?= $(shell $(cmd_UNAME_SYS))
UNAME_SYS	:= $(tmp_UNAME_SYS)

cmd_UNAME_ARCH	= uname -m | $(TR) '[A-Z]' '[a-z]'
tmp_UNAME_ARCH	!= $(cmd_UNAME_ARCH)
tmp_UNAME_ARCH	?= $(shell $(cmd_UNAME_ARCH))
UNAME_ARCH	:= $(tmp_UNAME_ARCH)

############################################################################################

SYSDEP_SUF	= $(UNAME_SYS)
sys_LIBS	= $(LIBS_$(SYSDEP_SUF))
sys_INCS	= $(INCS_$(SYSDEP_SUF))
sys_OPTI	= $(OPTI_$(SYSDEP_SUF))
sys_WARN	= $(WARN_$(SYSDEP_SUF))
sys_DEBUG	= $(DEBUG_$(SYSDEP_SUF))

############################################################################################

SRCINC		= $(BUILDDIR)/_src_.c
BUILDINC	= build.h

# Forbid space between '#' and 'define' so as word number is fixed (easy parsing)
cmd_VERSION	= $(AWK) '/^[ \t]*\#define APP_VERSION[ \s]/ { print $$3; }' $(BUILDINC) $(NO_STDERR) || true
tmp_VERSION	!= $(cmd_VERSION)
tmp_VERSION	?= $(shell $(cmd_VERSION))
VERSION		:= $(tmp_VERSION)

cmd_DEBUG_BUILD	= $(GREP) -Eq '^[[:space:]]*\#[[:space:]]*define APP_DEBUG([[:space:]]|$$)' \
				$(BUILDINC) $(NO_STDERR) && echo DEBUG || echo OPTI
tmp_DEBUG_BUILD	!= $(cmd_DEBUG_BUILD)
tmp_DEBUG_BUILD	?= $(shell $(cmd_DEBUG_BUILD))
DEBUG_BUILD	:= $(tmp_DEBUG_BUILD)

get_OPTI_OR_DEBUG= $($(DEBUG_BUILD))

cmd_TOPDIR	= pwd
tmp_TOPDIR	!= $(cmd_TOPDIR)
tmp_TOPDIR	?= $(shell $(cmd_TOPDIR))
TOPDIR		:= $(tmp_TOPDIR)

############################################################################################

# Command searching source files, $(SRCDIR/sysdeps/* not included unless suffixed with system name).
cmd_SRC		= $(FIND) $(SRCDIR) \( -iname '*.c' -or -iname '*.cc' -or -iname '*.cpp' -or -iname '*.m' \) \
		  -and \( \! -path '$(SRCDIR)/sysdeps/*' -or -path '$(SRCDIR)/sysdeps/*$(SYSDEP_SUF).*' \) \
		  -and \! -name '_src_.c'
cmd_INCLUDES	= $(FIND) $(INCDIRS) $(SRCDIR) \( -iname '*.h' -or -iname '*.hh' -or -iname '*.hpp' \) \
		  -and \( \! -path '$(SRCDIR)/sysdeps/*' -or -path '$(SRCDIR)/sysdeps/*$(SYSDEP_SUF).*' \)

# SRC variable
tmp_SRC		!= $(cmd_SRC)
tmp_SRC		?= $(shell $(cmd_SRC))
SRC		:= $(SRCINC) $(tmp_SRC)

# OBJ variable computed from SRC, replacing SRCDIR by BUILDDIR and extension by .o
cmd_SRC_BUILD	= echo $(SRC) | $(SED) -e 's|^$(SRCDIR)/|$(BUILDDIR)/|'
tmp_SRC_BUILD	!= $(cmd_SRC_BUILD)
tmp_SRC_BUILD	?= $(shell $(cmd_SRC_BUILD))
tmp_OBJ1	= $(tmp_SRC_BUILD:.m=.o)
tmp_OBJ2	= $(tmp_OBJ1:.cpp=.o)
tmp_OBJ3	= $(tmp_OBJ2:.cc=.o)
OBJ		= $(tmp_OBJ3:.c=.o)

# INCLUDE VARIABLE
tmp_INCLUDES	!= $(cmd_INCLUDES)
tmp_INCLUDES	?= $(shell $(cmd_INCLUDES))
INCLUDES	:= $(BUILDINC) $(tmp_INCLUDES)

############################################################################################
# Generic Build Flags, taking care of system specific flags (get_*)
cmd_CPPFLAGS	= { echo "-I."; test "$(SRCDIR)" != "." && echo "-I$(SRCDIR)"; \
		    test "$(SRCDIR)" != "$(BUILDDIR)" && echo "-I$(BUILDDIR)"; \
		    for dir in $(INCDIRS); do test "$$dir" != "$(SRCDIR)" && test "$$dir" != "." && echo "-I$$dir"; done; true; }
tmp_CPPFLAGS	!= $(cmd_CPPFLAGS)
tmp_CPPFLAGS	?= $(shell $(cmd_CPPFLAGS))

FLAGS_COMMON	= $(get_OPTI_OR_DEBUG) $(WARN) $(ARCH)
CPPFLAGS	= $(tmp_CPPFLAGS) $(sys_INCS) $(INCS)
CFLAGS		= $(FLAGS_C) $(FLAGS_COMMON)
CXXFLAGS	= $(FLAGS_CXX) $(FLAGS_COMMON)
OBJCFLAGS	= $(FLAGS_OBJC) $(FLAGS_COMMON)
LDFLAGS		= $(ARCH) $(get_OPTI_OR_DEBUG) $(sys_LIBS) $(LIBS)
ARFLAGS		= r

############################################################################################

MAKEFILES	= Makefile
DISTNAME	= $(NAME)_$(VERSION)_$(TIMESTAMP)
SRCINC_CONTENT	= LICENSE README.md $(tmp_SRC) $(INCLUDES) $(MAKEFILES)

############################################################################################
.PHONY: all clean distclean dist test info debug

all: $(BIN) $(LIB)

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(BIN)
	@$(PRINTF) "increment build number\n"
	@$(AWK) '{if(/^[ \t]*#define[ \t][ \t]*APP_BUILD_NUMBER/) \
		    {print $$1 " " $$2 " " ($$3 + 1);}else print $$0;}' \
		$(BUILDINC) > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(TOUCH) -r $(SRCINC) $(BUILDINC)

$(LIB): $(OBJ)
	$(AR) $(ARFLAGS) $(LIB) $(OBJ)
	$(RANLIB) $(LIB)
	@$(PRINTF) "increment build number\n"
	@$(AWK) '{if(/^[ \t]*#define[ \t][ \t]*APP_BUILD_NUMBER/) \
		    {print $$1 " " $$2 " " ($$3 + 1);}else print $$0;}' \
		$(BUILDINC) > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(TOUCH) -r $(SRCINC) $(BUILDINC)

##########################################################################################

clean:
	$(RM) $(OBJ) $(SRCINC)

distclean: clean
	$(RM) $(BIN) $(LIB) `$(FIND) . -name '.*.swp' -or -name '.*.swo' -or -name '*~' -or -name '\#*' $(NO_STDERR)`
	$(RM) -R $(BIN).dSYM || true
	@$(TEST) "$(BUILDDIR)" != "$(SRCDIR)" && $(RMDIR) `$(FIND) $(BUILDDIR) -type d | $(SORT) -r` $(NO_STDERR) || true
	@$(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(APP_DEBUG|APP_TEST)([[:space:]]|$$)' $(BUILDINC) \
	    > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(PRINTF) "distclean done, debug disabled.\n"

debug:
	@{ $(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(_DEBUG|_TEST)([[:space:]]|$$)' $(BUILDINC) $(NO_STDERR); \
		$(PRINTF) "#define APP_DEBUG\n#define APP_TEST\n"; } > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(PRINTF) "debug enabled ('make distclean' to disable it).\n"
	@$(MAKE)
	@#cd lib && $(MAKE)

##########################################################################################
.SUFFIXES: .o .c .h .cpp .hpp .cc .hh .m

$(OBJ): $(INCLUDES) $(MAKEFILES)
.m.o:
	$(OBJC) $(OBJCFLAGS) $(CPPFLAGS) -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.cc.o:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

############################################################################################

dist:
	@mkdir -p "$(DISTDIR)/$(DISTNAME)"
	@cp -Rf . "$(DISTDIR)/$(DISTNAME)"
	@$(RM) -R `$(FIND) "$(DISTDIR)/$(DISTNAME)" -type d -and \( -name '.git' -or 'CVS' -or -name '.hg' -or -name '.svn' \) $(NO_STDERR)`
	@$(PRINTF) "building dist...\n"
	@cd "$(DISTDIR)/$(DISTNAME)" && $(MAKE) distclean && $(MAKE) && $(MAKE) distclean
	@cd "$(DISTDIR)" && { $(TAR) cJf "$(DISTNAME).tar.xz" "$(DISTNAME)" && targz=true || targz=false; \
     			      $(TAR) czf "$(DISTNAME).tar.gz" "$(DISTNAME)" || $${targz}; }
	@#cd "$(DISTDIR)" && ($(ZIP) -q -r "$(DISTNAME).zip" "$(DISTNAME)" || true)
	@$(RM) -R "$(DISTDIR)/$(DISTNAME)"
	@$(PRINTF) "archives created: $$(ls $(DISTDIR)/$(DISTNAME).*)\n"

$(SRCINC): $(SRCINC_CONTENT)
	@# Generate $(SRCINC) containing all sources.
	@$(PRINTF) "generate $@\n"
	@mkdir -p $(@D)
	@$(PRINTF) "/* generated content */\n" > $(SRCINC) ; \
		$(AWK) 'BEGIN { dbl_bkslash="\\"; gsub(/\\/, "\\\\\\", dbl_bkslash); o="awk on ubuntu 12.04"; \
	                        if (dbl_bkslash=="\\\\") dbl_bkslash="\\\\\\"; else dbl_bkslash="\\\\"; \
				print "#include <stdlib.h>\n#include \"build.h\"\n#ifdef INCLUDE_SOURCE\n" \
				      "static const char * const s_program_source[] = {"; } \
		   function printblk() { \
	               gsub(/\\/, dbl_bkslash, blk); \
                       gsub(/"/, "\\\"", blk); \
	               gsub(/\n/, "\\n\"\n\"", blk); \
	               print "\"" blk "\\n\","; \
	           } { \
		       if (curfile != FILENAME) { \
		           curfile="/* FILE: $(NAME)/" FILENAME " */"; blk=blk "\n" curfile; curfile=FILENAME; \
	               } if (length($$0 " " blk) > 2000) { \
	                   printblk(); blk=$$0; \
                       } else \
		           blk=blk "\n" $$0; \
		   } END { \
		       printblk(); print "NULL };\nconst char *const* $(NAME)_get_source() { return s_program_source; }\n#endif"; \
		   }' $(SRCINC_CONTENT) >> $(SRCINC) ; \
	     $(CC) -I. -c $(SRCINC) -o $(SRCINC).tmp.o $(NO_STDERR) \
	         || $(PRINTF) "#include <stdlib.h>\n#include \"build.h\"\n#ifdef INCLUDE_SOURCE\nstatic const char * const s_program_source[] = {\n  \"cannot include source. check awk version or antivirus or bug\\\\n\", NULL};\nconst char *const* $(NAME)_get_source() { return s_program_source; }\n#endif\n" > $(SRCINC); \
	     $(RM) -f $(SRCINC).*

$(BUILDINC):
	@test -e $(BUILDINC) || $(PRINTF) "#define APP_BUILD_NUMBER 0\n#define APP_VERSION 0.1\n#define INCLUDE_SOURCE\n" > $(BUILDINC)

info:
	@echo "UNAME_SYS        : $(UNAME_SYS)"
	@echo "UNAME_ARCH       : $(UNAME_ARCH)"
	@echo "MAKE             : $(MAKE)"
	@echo "SHELL            : $(SHELL)"
	@echo "FIND             : $(FIND)"
	@echo "AWK              : $(AWK)"
	@echo "GREP             : $(GREP)"
	@echo "SED              : $(SED)"
	@echo "TAR              : $(TAR)"
	@echo "DATE             : $(DATE)"
	@echo "PKGCONFIG        : $(PKGCONFIG)"
	@echo "CC               : $(CC)"
	@echo "CXX              : $(CXX)"
	@echo "OBJC             : $(OBJC)"
	@echo "CPP              : $(CPP)"
	@echo "CFLAGS           : $(CFLAGS)"
	@echo "CXXFLAGS         : $(CXXFLAGS)"
	@echo "OBJCFLAGS        : $(OBJCFLAGS)"
	@echo "CPPCFLAGS        : $(CPPFLAGS)"
	@echo "LDFLAGS          : $(LDFLAGS)"
	@echo "SRCDIR           : $(SRCDIR)"
	@echo "DISTDIR          : $(DISTDIR)"
	@echo "BUILDDIR         : $(BUILDDIR)"
	@echo "DISTNAME         : $(DISTNAME)"
	@echo "BIN              : $(BIN)"
	@echo "INCLUDES         : $(INCLUDES)"
	@echo "SRC              : $(SRC)"
	@echo "OBJ              : $(OBJ)"

#$(BUILDDIR)/%.o: $(SRCDIR)/%.m $(INCLUDES) $(MAKEFILES)
#	@mkdir -p $(@D)
#	$(OBJC) -c $< $(OBJCFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(INCLUDES) $(MAKEFILES)
#	@mkdir -p $(@D)
#	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(INCLUDES) $(MAKEFILES)
#	@mkdir -p $(@D)
#	$(CXX) -c $< $(CXXFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.cc $(INCLUDES) $(MAKEFILES)
#	@mkdir -p $(@D)
#	$(CXX) -c $< $(CXXFLAGS) $(CPPFLAGS) -o $@


