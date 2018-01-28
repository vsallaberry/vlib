#
# Copyright (C) 2017-2018 Vincent Sallaberry
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
# Generic Makefile for GNU-like or BSD-like make (paths with spaces not supported).
#   demo of this Makefile with multiple languages: <https://github.com/vsallaberry/vmultilangdemo>
#
############################################################################################

# First, 'all' rule calling default_rule to allow user adding his own dependency
# rules in specific part below.
all: default_rule

#############################################################################################
# PROJECT SPECIFIC PART
#############################################################################################

# SRCDIR: Folder where sources are. Use '.' for current directory. MUST NEVER BE EMPTY !!
# Folders which contains a Makefile are ignored, you have to add them in SUBDIRS and update SUBLIBS.
SRCDIR 		= src

# SUBDIRS, put empty if there is no need to run make on sub directories.
SUBDIRS 	=
# SUBLIBS: libraries built from subdirs, needed for binary dependency. Put empty if none.
SUBLIBS		=

# INCDIRS: Folder where public includes are. It can be SRCDIR or even empty if
# headers are only in SRCDIR. Use '.' for current directory.
INCDIRS 	= include

# Where targets are created (OBJs, BINs, ...). Eg: '.' or 'build'. ONLY 'SRCDIR' is supported!
BUILDDIR	= $(SRCDIR)

# Name of the Package (DISTNAME, BIN and LIB depends on it)
NAME		= vlib

# Binary name and library name (prefix with '$(BUILDDIR)/' to put it in build folder).
# Fill LIB and set BIN,JAR empty to create a library, or clear LIB,JAR and set BIN to create a binary.
BIN		=
LIB		= lib$(NAME).a
JAR		=

# DISTDIR: where the dist packages zip/tar.xz are saved
DISTDIR		= ../../dist

# PREFIX: where the application is to be installed
PREFIX		= /usr/local

# Project specific Flags (system specific flags are handled further)
# Choice between <flag>_RELEASE/_DEBUG is done according to BUILDINC
WARN_RELEASE	= -Wall -W -pedantic # -Wno-ignored-attributes -Wno-attributes
ARCH_RELEASE	= -march=native # -arch i386 -arch x86_64
OPTI_RELEASE	= -O3 -pipe
INCS_RELEASE	=
LIBS_RELEASE	= $(SUBLIBS)
MACROS_RELEASE	=
WARN_DEBUG	= $(WARN_RELEASE) # -Werror
ARCH_DEBUG	= $(ARCH_RELEASE)
OPTI_DEBUG	= -O0 -ggdb3 -pipe
INCS_DEBUG	= $(INCS_RELEASE)
LIBS_DEBUG	= $(LIBS_RELEASE)
MACROS_DEBUG	= -D_DEBUG -D_TEST
# FLAGS_<lang> is global for one language (<lang>: C,CXX,OBJC,GCJ,GCJH,OBJCXX,LEX,YACC).
FLAGS_C		= -std=c99 -D_GNU_SOURCE
FLAGS_CXX	= -D_GNU_SOURCE
FLAGS_OBJC	= -std=c99
FLAGS_OBJCXX	=
FLAGS_GCJ	=
# FLAGS_<lang>_<file> is specific to one file (eg:'FLAGS_CXX_Big.cc=-O0','FLAGS_C_src/a.c=-O1')

# System specific flags (WARN_$(sys),OPTI_$(sys),DEBUG_$(sys),LIBS_$(sys),INCS_$(sys))
# $(sys) is lowcase(`uname`), eg: 'LIBS_darwin=-framework IOKit -framework Foundation'
LIBS_darwin	= -framework IOKit -framework Foundation

############################################################################################
# GENERIC PART - in most cases no need to change anything below until end of file
############################################################################################

AR		= ar
RANLIB		= ranlib
GREP		= grep
WHICH		= which
HEADN1		= head -n1
PRINTF		= printf
AWK		= awk
SED		= sed
RM		= rm -f
DATE		= date
TAR		= tar
ZIP		= zip
FIND		= find
PKGCONFIG	= pkg-config
TEST		= test
SORT		= sort
MKDIR		= mkdir
RMDIR		= rmdir
TOUCH		= touch
CP		= CP
MV		= mv
TR		= tr
GIT		= git
DIFF		= diff
UNIQ		= uniq
INSTALL		= install -c -m 0644
INSTALLBIN	= install -c
INSTALLDIR	= install -c -d
NO_STDERR	= 2> /dev/null
NO_STDOUT	= > /dev/null

############################################################################################
# About shell commands execution in this Makefile:
# - On recent gnu make (>=4.0 i guess), "!=' is understood.
# - On gnu make 3.81, '!=' is not understood but it does NOT cause syntax error.
# - On {open,free,net}bsd $(shell cmd) is not understood but does NOT cause syntax error.
# - On gnu make 3.82, '!=' causes syntax error, then it is at the moment only case where
#   make-fallback is needed (make-fallback removes lines which cannot be parsed).
# Assuming that, the command to be run is put in a variable (cmd_...),
# then the '!=' is tried, and $(shell ..) will be done only on '!=' failure (?= $(shell ..).
# It is important to use a temporary name, like tmp_CC because CC is set by make at startup.
# Generally, we finish the command by true as some bsd make raise warnings if not.
############################################################################################

# SHELL
cmd_SHELL	= $(WHICH) bash sh $(SHELL) $(NO_STDERR) | $(HEADN1)
tmp_SHELL	!= $(cmd_SHELL)
tmp_SHELL	?= $(shell $(cmd_SHELL))
SHELL		:= $(tmp_SHELL)

# Do not prefix with ., to not disturb dependencies and exclusion from include search.
BUILDINC	= build.h
VERSIONINC	= version.h
SYSDEPDIR	= sysdeps

# SRCINC containing source code is included if APP_INCLUDE_SOURCE is defined in VERSIONINC.
SRCINCNAME	= _src_.c
SRCINCDIR	= $(BUILDDIR)

cmd_SRCINC	= ! $(TEST) -e $(VERSIONINC) \
		  || $(GREP) -Eq '^[[:space:]]*\#[[:space:]]*define APP_INCLUDE_SOURCE([[:space:]]|$$)' \
	                                $(VERSIONINC) $(NO_STDERR) && echo $(SRCINCDIR)/$(SRCINCNAME) | $(SED) -e 's|^\./||' || true
tmp_SRCINC	!= $(cmd_SRCINC)
tmp_SRCINC	?= $(shell $(cmd_SRCINC))
SRCINC		:= $(tmp_SRCINC)

# Get Debug mode in build.h
cmd_RELEASEMODE	= $(GREP) -Eq '^[[:space:]]*\#[[:space:]]*define BUILD_DEBUG([[:space:]]|$$)' \
				$(BUILDINC) $(NO_STDERR) && echo DEBUG || echo RELEASE
tmp_RELEASEMODE	!= $(cmd_RELEASEMODE)
tmp_RELEASEMODE	?= $(shell $(cmd_RELEASEMODE))
RELEASE_MODE	:= $(tmp_RELEASEMODE)

WARN		= $(WARN_$(RELEASE_MODE))
OPTI		= $(OPTI_$(RELEASE_MODE))
ARCH		= $(ARCH_$(RELEASE_MODE))
INCS		= $(INCS_$(RELEASE_MODE))
LIBS		= $(LIBS_$(RELEASE_MODE))
MACROS		= $(MACROS_$(RELEASE_MODE))

# Search bison 3 or later, fallback on bison, yacc.
cmd_YACC	= { for bin in $$($(WHICH) -a bison $(YACC) $(NO_STDERR)); do \
		      ver="$$($$bin -V 2>&1 | $(AWK) -F '.' '/[0-9]+(\.[0-9]+)+/{$$0=substr($$0,match($$0,/[0-9]+(\.[0-9]+)+/));\
		                                                                 print $$1*1000000 + $$2*10000 + $$3*100 + $$4*1 }')"; \
		      $(TEST) $$ver -ge 03000000 $(NO_STDERR) && echo "$${bin}._have_bison3_" && break; done; $(WHICH) $(YACC) bison yacc $(NO_STDERR); } | $(HEADN1)
tmp_YACC	!= $(cmd_YACC)
tmp_YACC	?= $(shell $(cmd_YACC))
YACC		:= $(tmp_YACC)
BISON3		:= $(YACC)
YACC		:= $(YACC:._have_bison3_=)
BISON3$(YACC)._have_bison3_ := $(YACC)
BISON3		:= $(BISON3$(BISON3))

# Search flex, lex.
cmd_LEX		= $(WHICH) flex lex $(NO_STDERR) | $(HEADN1)
tmp_LEX		!= $(cmd_LEX)
tmp_LEX		?= $(shell $(cmd_LEX))
LEX		:= $(tmp_LEX)

# Search gcj compiler.
cmd_GCJ		= $(WHICH) ${GCJ} gcj gcj-mp gcj-mp-6 gcj-mp-5 gcj-mp-4.9 gcj-mp-4.8 gcj-mp-4.7 gcj-mp-4.6 $(NO_STDERR) | $(HEADN1)
tmp_GCJ		!= $(cmd_GCJ)
tmp_GCJ		?= $(shell $(cmd_GCJ))
GCJ		:= $(tmp_GCJ)

############################################################################################
# Scan for sources
############################################################################################
# Common find pattern to include files in SRCDIR/sysdeps ONLY if suffixed with system name.
find_AND_SYSDEP	= -and \( \! -path '$(SRCDIR)/$(SYSDEPDIR)/*' -or -path '$(SRCDIR)/$(SYSDEPDIR)/*$(SYSDEP_SUF).*' \)

# Search Meta sources (used to generate sources)
# For yacc/bison and lex/flex:
#   - the basename of meta sources must be always different (have one grammar calc.y and one
#     lexer calc.l is not supported: prefer parse-calc.y and scan-calc.l).
#   - c++ source is generated with .ll and .yy, c source with .l and .y, java with .yyj
#   - .l,.ll included if LEX is found, .y,.yy included if YACC is found, .yyj included
#     if BISON3 AND ((GCJ and BIN are defined) OR (JAR defined)).
#   - yacc generates by default headers for lexer, therefore lexer files depends on parser files.
cmd_YACCSRC	= [ -n "$(YACC)" ] && $(FIND) $(SRCDIR) \( -name '*.y' -or -name '*.yy' \) \
		  $(find_AND_SYSDEP) $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_LEXSRC	= [ -n "$(LEX)" ] && $(FIND) $(SRCDIR) \( -name '*.l' -or -name '*.ll' \) \
		  $(find_AND_SYSDEP) $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_YACCJAVA	= [ -n "$(BIN)" ] && [ -n "$(GCJ)" ] || [ -n "$(JAR)" ] \
		  && [ -n "$(BISON3)" ] && $(FIND) $(SRCDIR) -name '*.yyj' \
		                           $(find_AND_SYSDEP) $(NO_STDERR) | $(SED) -e 's|^\./||' || true
# METASRC variable, filled from the 'find' command (cmd_{YACC,LEX,..}SRC) defined above.
tmp_YACCSRC	!= $(cmd_YACCSRC)
tmp_YACCSRC	?= $(shell $(cmd_YACCSRC))
YACCSRC		:= $(tmp_YACCSRC)
tmp_LEXSRC	!= $(cmd_LEXSRC)
tmp_LEXSRC	?= $(shell $(cmd_LEXSRC))
LEXSRC		:= $(tmp_LEXSRC)
tmp_YACCJAVA	!= $(cmd_YACCJAVA)
tmp_YACCJAVA	?= $(shell $(cmd_YACCJAVA))
YACCJAVA	:= $(tmp_YACCJAVA)
METASRC		:= $(YACCSRC) $(LEXSRC) $(YACCJAVA)
# Transform meta sources into sources and objects
tmp_YACCGENSRC1	= $(YACCSRC:.y=.c)
YACCGENSRC	:= $(tmp_YACCGENSRC1:.yy=.cc)
tmp_LEXGENSRC1	= $(LEXSRC:.l=.c)
LEXGENSRC	:= $(tmp_LEXGENSRC1:.ll=.cc)
YACCGENJAVA	:= $(YACCJAVA:.yyj=.java)
tmp_YACCOBJ1	= $(YACCGENSRC:.c=.o)
YACCOBJ		:= $(tmp_YACCOBJ1:.cc=.o)
tmp_LEXOBJ1	= $(LEXGENSRC:.c=.o)
LEXOBJ		:= $(tmp_LEXOBJ1:.cc=.o)
YACCCLASSES	:= $(YACCGENJAVA:.java=.class)
tmp_YACCINC1	= $(YACCSRC:.y=.h)
YACCINC		:= $(tmp_YACCINC1:.yy=.hh)
# Set Global generated sources variable
GENSRC		:= $(YACCGENSRC) $(LEXGENSRC)
GENJAVA		:= $(YACCGENJAVA)
GENINC		:= $(YACCINC)
GENOBJ		:= $(YACCOBJ) $(LEXOBJ)
GENCLASSES	:= $(YACCCLASSES)

# Create find ignore pattern for generated sources and for folders containing a makefile
cmd_FIND_NOGEN	= echo $(GENSRC) $(GENINC) $(GENJAVA) \
		       "$$($(FIND) $(SRCDIR) -mindepth 2 -name 'Makefile' \
		           | $(SED) -e 's|\([^[:space:]]*\)/[^[:space:]]*|\1/*|g')" \
		  | $(SED) -e 's|\([^[:space:]]*\)|-and \! -path "\1" -and \! -path "./\1"|g' || true
tmp_FIND_NOGEN	!= $(cmd_FIND_NOGEN)
tmp_FIND_NOGEN	?= $(shell $(cmd_FIND_NOGEN))
find_AND_NOGEN	:= $(tmp_FIND_NOGEN)

# Search non-generated sources and headers. Extensions must be in low-case.
# Include java only if a JAR is defined as output or if BIN and GCJ are defined.
cmd_JAVASRC	= [ -n "$(BIN)" ] && [ -n "$(GCJ)" ] || [ -n "$(JAR)" ] \
		  && $(FIND) $(SRCDIR) \( -name '*.java' \) $(find_AND_SYSDEP) \
		             $(find_AND_NOGEN) $(NO_STDERR) | $(SED) -e 's|^\./||' || true
# JAVASRC variable, filled from the 'find' command (cmd_JAVA) defined above.
tmp_JAVASRC	!= $(cmd_JAVASRC)
tmp_JAVASRC	?= $(shell $(cmd_JAVASRC))
tmp_JAVASRC	:= $(tmp_JAVASRC)
JAVASRC		:= $(tmp_JAVASRC) $(GENJAVA)
JCNIINC		:= $(JAVASRC:.java=.hh)
JCNISRC		:= $(JAVASRC:.java=.cc)
GENINC		:= $(GENINC) $(JCNIINC)
CLASSES		:= $(JAVASRC:.java=.class)

# Add Java CNI headers to include search exclusion.
cmd_FIND_NOGEN2	= echo $(JCNIINC) | $(SED) -e 's|\([^[:space:]]*\)|-and \! -path "\1" -and \! -path "./\1"|g'
tmp_FIND_NOGEN2	!= $(cmd_FIND_NOGEN2)
tmp_FIND_NOGEN2	?= $(shell $(cmd_FIND_NOGEN2))
find_AND_NOGEN2	:= $(tmp_FIND_NOGEN2)
# Other non-generated sources and headers. Extension must be in low-case.
cmd_SRC		= $(FIND) $(SRCDIR) \( -name '*.c' -or -name '*.cc' -or -name '*.cpp' -or -name '*.m' -or -name '*.mm' \) \
		  $(find_AND_SYSDEP) $(find_AND_NOGEN) -and \! -path '$(SRCINC)' -and \! -path './$(SRCINC)' \
		  $(NO_STDERR) | $(SED) -e 's|^\./||'
cmd_INCLUDES	= $(FIND) $(INCDIRS) $(SRCDIR) \( -name '*.h' -or -name '*.hh' -or -name '*.hpp' \) \
		  $(find_AND_SYSDEP) $(find_AND_NOGEN) $(find_AND_NOGEN2) \
		  -and \! -path $(VERSIONINC) -and \! -path ./$(VERSIONINC) \
		  -and \! -path $(BUILDINC) -and \! -path ./$(BUILDINC) $(NO_STDERR) | $(SED) -e 's|^\./||'

# SRC variable, filled from the 'find' command (cmd_SRC) defined above.
tmp_SRC		!= $(cmd_SRC)
tmp_SRC		?= $(shell $(cmd_SRC))
tmp_SRC		:= $(tmp_SRC)
SRC		:= $(SRCINC) $(tmp_SRC) $(GENSRC)

# OBJ variable computed from SRC, replacing SRCDIR by BUILDDIR and extension by .o
# Add Java.o if BIN, GCJ and JAVASRC are defined.
JAVAOBJNAME	:= Java.o
JAVAOBJ$(BUILDDIR) := $(BUILDDIR)/$(JAVAOBJNAME)
JAVAOBJ.	:= $(JAVAOBJNAME)
JAVAOBJ		:= $(JAVAOBJ$(BUILDDIR))
TMPCLASSESDIR	= $(BUILDDIR)/.tmp_classes
cmd_SRC_BUILD	:= echo " $(SRC)" | $(SED) -e 's| $(SRCDIR)/| $(BUILDDIR)/|g'; \
		  case " $(JAVASRC) " in *" "*".java "*) [ -n "$(GCJ)" ] && [ -n "$(BIN)" ] && echo "$(JAVAOBJ)";; esac
tmp_SRC_BUILD	!= $(cmd_SRC_BUILD)
tmp_SRC_BUILD	?= $(shell $(cmd_SRC_BUILD))
tmp_OBJ1	:= $(tmp_SRC_BUILD:.m=.o)
tmp_OBJ2	:= $(tmp_OBJ1:.mm=.o)
tmp_OBJ3	:= $(tmp_OBJ2:.cpp=.o)
tmp_OBJ4	:= $(tmp_OBJ3:.cc=.o)
OBJ		:= $(tmp_OBJ4:.c=.o)

# INCLUDE VARIABLE, filled from the 'find' command (cmd_INCLUDES) defined above.
tmp_INCLUDES	!= $(cmd_INCLUDES)
tmp_INCLUDES	?= $(shell $(cmd_INCLUDES))
tmp_INCLUDES	:= $(tmp_INCLUDES)
INCLUDES	:= $(VERSIONINC) $(BUILDINC) $(tmp_INCLUDES)

# Search compilers: choice might depend on what we have to build (eg: use gcc if using gcj)
cmd_CC		= case " $(OBJ) " in *" $(JAVAOBJ) "*) gccgcj=$$(echo "$(GCJ) gcc" | sed -e 's|gcj\([^/ ]*\)|gcc\1|');; esac; \
		  $(WHICH) $${gccgcj} $${CC} clang gcc cc $(CC) $(NO_STDERR) | $(HEADN1)
tmp_CC		!= $(cmd_CC)
tmp_CC		?= $(shell $(cmd_CC))
CC		:= $(tmp_CC)

cmd_CXX		= case " $(OBJ) " in *" $(JAVAOBJ) "*) gccgcj=$$(echo "$(GCJ) g++" | sed -e 's|gcj\([^/ ]*\)|g++\1|');; esac; \
		  $(WHICH) $${gccgcj} $${CXX} clang++ g++ c++ $(CXX) $(NO_STDERR) | $(HEADN1)
tmp_CXX		!= $(cmd_CXX)
tmp_CXX		?= $(shell $(cmd_CXX))
CXX		:= $(tmp_CXX)

cmd_GCJH	= echo "$(GCJ)" | $(SED) -e 's|gcj\([^/]*\)$$|gcjh\1|'
tmp_GCJH	!= $(cmd_GCJH)
tmp_GCJH	?= $(shell $(cmd_GCJH))
GCJH		:= $(tmp_GCJH)

# CCLD: use $(GCJ) if Java.o, use $(CXX) if .cc,.cpp,.mm files, otherwise use $(CC).
cmd_CCLD	= case " $(OBJ) $(SRC) " in *" $(JAVAOBJ) "*) echo $(GCJ) ;; \
		                            *" "*".cpp "*|*" "*".cc "*|*" "*".mm "*) echo $(CXX);; *) echo $(CC) ;; esac
tmp_CCLD	!= $(cmd_CCLD)
tmp_CCLD	?= $(shell $(cmd_CCLD))
CCLD		:= $(tmp_CCLD)

CPP		= $(CC) -E
OBJC		= $(CC)
OBJCXX		= $(CXX)

cmd_UNAME_SYS	= uname | $(TR) '[A-Z]' '[a-z]'
tmp_UNAME_SYS	!= $(cmd_UNAME_SYS)
tmp_UNAME_SYS	?= $(shell $(cmd_UNAME_SYS))
UNAME_SYS	:= $(tmp_UNAME_SYS)

#cmd_UNAME_ARCH	:= uname -m | $(TR) '[A-Z]' '[a-z]'
#tmp_UNAME_ARCH	!= $(cmd_UNAME_ARCH)
#tmp_UNAME_ARCH	?= $(shell $(cmd_UNAME_ARCH))
#UNAME_ARCH	:= $(tmp_UNAME_ARCH)

JAVA		= java
JARBIN		= jar
JAVAC		= javac
JAVAH		= javah

############################################################################################

SYSDEP_SUF	= $(UNAME_SYS)
sys_LIBS	= $(LIBS_$(SYSDEP_SUF))
sys_INCS	= $(INCS_$(SYSDEP_SUF))
sys_OPTI	= $(OPTI_$(SYSDEP_SUF))
sys_WARN	= $(WARN_$(SYSDEP_SUF))
sys_DEBUG	= $(DEBUG_$(SYSDEP_SUF))

############################################################################################
# Generic Build Flags, taking care of system specific flags (sys_*)
cmd_CPPFLAGS	= { echo "-I."; $(TEST) "$(SRCDIR)" != "." && echo "-I$(SRCDIR)"; \
		    $(TEST) "$(SRCDIR)" != "$(BUILDDIR)" && echo "-I$(BUILDDIR)"; \
		    for dir in $(INCDIRS); do $(TEST) "$$dir" != "$(SRCDIR)" && $(TEST) "$$dir" != "." && echo "-I$$dir"; done; true; }
tmp_CPPFLAGS	!= $(cmd_CPPFLAGS)
tmp_CPPFLAGS	?= $(shell $(cmd_CPPFLAGS))
tmp_CPPFLAGS	:= $(tmp_CPPFLAGS)

FLAGS_COMMON	= $(OPTI) $(WARN) $(ARCH)
CPPFLAGS	:= $(tmp_CPPFLAGS) $(sys_INCS) $(INCS) $(MACROS)
CFLAGS		= -MMD $(FLAGS_C) $(FLAGS_COMMON)
CXXFLAGS	= -MMD $(FLAGS_CXX) $(FLAGS_COMMON)
OBJCFLAGS	= -MMD $(FLAGS_OBJC) $(FLAGS_COMMON)
OBJCXXFLAGS	= -MMD $(FLAGS_OBJCXX) $(FLAGS_COMMON)
JFLAGS		= $(FLAGS_GCJ) $(FLAGS_COMMON) -I$(BUILDDIR)
JHFLAGS		= -I$(BUILDDIR)
LIBFORGCJ$(GCJ)	= -lstdc++
LDFLAGS		= $(ARCH) $(OPTI) $(LIBS) $(sys_LIBS) $(LIBFORGCJ$(CCLD))
ARFLAGS		= r
LFLAGS		=
LCXXFLAGS	= $(LFLAGS)
LJFLAGS		=
YFLAGS		= -d
YCXXFLAGS	= $(YFLAGS)
YJFLAGS		=
############################################################################################
# GCC -MD management (dependencies generation)
# make on some BSD systems 1) does not support '-include' or 'sinclude', 2) does not support
# including several files in one include statement, and 3) does not see a file created before
# inclusion by a shell command.
# Here, if .alldeps does not exit, we include version.h (contains only lines starting with
# dash(#)), and we create .alldeps.d containing inclusion of all .d files, created with
# default headers dependency. This method works thanks to MAKE recursion. Without recursion,
# some stuff would be needed to guaranty the good order of the first build.
#
DEPS		:= $(OBJ:.o=.d)
INCLUDEDEPS	:= .alldeps.d
cmd_SINCLUDEDEPS= inc=1; if [ -e $(INCLUDEDEPS) ]; then echo "$(INCLUDEDEPS)"; \
		  else inc=; echo version.h; fi; \
		  for f in $(DEPS:.d=); do \
		      if [ -z "$$inc" ] || ! [ -e "$$f.d" ]; then \
		           [ "$$f.o" = "$(JAVAOBJ)" ] && echo "" > $$f.d || echo "$$f.o: $(INCLUDES) $(GENINC)" > $$f.d; \
		           echo "include $$f.d" >> $(INCLUDEDEPS); \
	      	      fi; \
		  done
tmp_SINCLUDEDEPS != $(cmd_SINCLUDEDEPS)
tmp_SINCLUDEDEPS ?= $(shell $(cmd_SINCLUDEDEPS))
SINCLUDEDEPS := $(tmp_SINCLUDEDEPS)
include $(SINCLUDEDEPS)

############################################################################################

ALLMAKEFILES	= Makefile
LICENSE		= LICENSE
README		= README.md
CLANGCOMPLETE	= .clang_complete
SRCINC_CONTENT	= $(LICENSE) $(README) $(METASRC) $(tmp_SRC) $(tmp_JAVASRC) $(INCLUDES) $(ALLMAKEFILES)

############################################################################################
# For make recursion through sub-directories
BUILDDIRS	= $(SUBDIRS:=-build)
INSTALLDIRS	= $(SUBDIRS:=-install)
DISTCLEANDIRS	= $(SUBDIRS:=-distclean)
CLEANDIRS	= $(SUBDIRS:=-clean)
TESTDIRS	= $(SUBDIRS:=-test)
DEBUGDIRS	= $(SUBDIRS:=-debug)
DOCDIRS		= $(SUBDIRS:=-doc)

############################################################################################

# --- default_rule : build all
default_rule: update-$(BUILDINC) $(BUILDDIRS)
	@# This is not the most beautilful thing here, but the best portable stuff i found yet,
	@# because some make consider dependents of PHONY targets always outdated.
	@$(MAKE) build_all

build_all: $(BIN) $(LIB) $(JAR) gentags

$(SUBDIRS): $(BUILDDIRS)
$(BUILDDIRS):
	cd $(@:-build=) && $(MAKE)

# --- clean : remove objects and generated files
clean: $(CLEANDIRS)
	$(RM) $(OBJ:.class=*.class) $(SRCINC) $(GENSRC) $(GENINC) $(GENJAVA) $(CLASSES:.class=*.class) $(DEPS) $(INCLUDEDEPS)
$(CLEANDIRS):
	cd $(@:-clean=) && $(MAKE) clean

# --- distclean : remove objects, binaries and remove DEBUG flag in build.h
distclean: $(DISTCLEANDIRS) clean
	$(RM) $(BIN) $(LIB) $(BUILDINC) `$(FIND) . -name '.*.swp' -or -name '.*.swo' -or -name '*~' -or -name '\#*' $(NO_STDERR)`
	$(RM) -R $(BIN).dSYM || true
	@$(TEST) "$(BUILDDIR)" != "$(SRCDIR)" && $(RMDIR) `$(FIND) $(BUILDDIR) -type d | $(SORT) -r` $(NO_STDERR) || true
	@$(PRINTF) "$(NAME): distclean done, debug disabled.\n"
$(DISTCLEANDIRS):
	cd $(@:-distclean=) && $(MAKE) distclean

# --- debug : set DEBUG flag in build.h and rebuild
debug: update-$(BUILDINC) $(DEBUGDIRS)
	@{ $(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(BUILD_DEBUG|BUILD_TEST)([[:space:]]|$$)' $(BUILDINC) $(NO_STDERR); \
		$(PRINTF) "#define BUILD_DEBUG\n#define BUILD_TEST\n"; } > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(PRINTF) "$(NAME): debug enabled ('make distclean' to disable it).\n"
	@$(MAKE)
$(DEBUGDIRS):
	cd $(@:-debug=) && $(MAKE) debug
# Code to disable debug without deleting BUILDINC:
# @$(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(BUILD_DEBUG|BUILD_TEST)([[:space:]]|$$)' $(BUILDINC) \
#	    > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)

# --- doc : generate doc
doc: $(DOCDIRS)
$(DOCDIRS):
	cd $(@:-doc=) && $(MAKE) doc

# --- install ---
install: $(INSTALLDIRS) all
$(INSTALLDIRS):
	cd $(@:-install=) && $(MAKE) install

# --- test ---
test: $(TESTDIRS) all
$(TESTDIRS):
	cd $(@:-test=) && $(MAKE) test

# --- build bin&lib ---
$(BIN): $(OBJ) $(SUBLIBS) $(JCNIINC)
	$(CCLD) $(OBJ:.class=*.class) $(LDFLAGS) -o $@
	@$(PRINTF) "$@: build done.\n"

$(LIB): $(OBJ) $(SUBLIBS) $(JCNIINC)
	$(AR) $(ARFLAGS) $@ $(OBJ:.class=*.class)
	$(RANLIB) $@
	@$(PRINTF) "$@: build done.\n"

# Build Java.o : $(JAVAOBJ)
$(CLASSES): $(JAVAOBJ)
	@true # Used to override implicit rule .java.class:
$(JAVAOBJ): $(JAVASRC)
	@# All classes generated/overriten at once. Generate them in tmp dir then check changed ones.
	@$(MKDIR) -p $(TMPCLASSESDIR)
	$(GCJ) $(JFLAGS) -d $(TMPCLASSESDIR) -C $(JAVASRC)
	@#$(GCJ) $(JFLAGS) -d $(BUILDDIR) -C $(JAVASRC)
	@for f in `$(FIND) "$(TMPCLASSESDIR)" -type f`; do \
	     dir=`dirname $$f | $(SED) -e 's|$(TMPCLASSESDIR)||'`; \
	     file=`basename $$f`; \
	     $(DIFF) -q "$(BUILDDIR)/$$dir/$$file" "$$f" $(NO_STDERR) $(NO_STDOUT) \
	       || { $(MKDIR) -p "$(BUILDDIR)/$$dir"; mv "$$f" "$(BUILDDIR)/$$dir"; }; \
	 done; $(RM) -Rf "$(TMPCLASSESDIR)"
	$(GCJ) $(JFLAGS) -d $(BUILDDIR) -c -o $@ $(CLASSES:.class=*.class)
$(JCNIINC): $(ALLMAKEFILES) $(BUILDINC)

#$(JCNISRC:.cc=.o) : $(JCNIINC) # usefull without -MD
#$(JCNIOBJ): $(JCNIINC) # Useful without -MD

# This is a TODO and a TOSTUDY
$(MANIFEST):
$(JAR): $(JAVASRC) $(SUBLIBS) $(MANIFEST) $(ALLMAKEFILES)
	@echo "TODO !!"
	javac $(JAVASRC) -classpath $(SRCDIR) -d $(BUILDDIR)
	jar uf $(MANIFEST) $@ $(CLASSES:.class=*.class)
	@$(PRINTF) "$@: build done.\n"

##########################################################################################
.SUFFIXES: .o .c .h .cpp .hpp .cc .hh .m .mm .java .class .y .l .yy .ll .yyj .llj

#### WITHOUT -MD
# OBJS are rebuilt on Makefile or headers update. Alternative: could use gcc -MD and sinclude.
#$(OBJ): $(INCLUDES) $(ALLMAKEFILES)
# LEX can depend on yacc generated header: not perfect as all lex are rebuilt on yacc file update
#$(LEXGENSRC): $(YACCOBJ)
# Empty rule for YACCGENSRC so that make keeps intermediate yacc generated sources
#$(YACCGENSRC): $(ALLMAKEFILES) $(BUILDINC)
#$(YACCOBJ): $(ALLMAKEFILES) $(BUILDINC)
#$(YACCCLASSES): $(ALLMAKEFILES) $(BUILDINC)
#$(YACCGENJAVA): $(ALLMAKEFILES) $(BUILDINC)

### WITH -MD
$(OBJ): $(ALLMAKEFILES) $(BUILDINC)
$(GENSRC): $(ALLMAKEFILES) $(BUILDINC)
$(GENJAVA): $(ALLMAKEFILES) $(BUILDINC)
$(CLASSES): $(ALLMAKEFILES) $(BUILDINC)

# Implicit rules: old-fashionned double suffix rules to be compatible with most make.
.m.o:
	$(OBJC) $(OBJCFLAGS) $(FLAGS_OBJC_$<) $(CPPFLAGS) -c $< -o $@
.mm.o:
	$(OBJCXX) $(OBJCXXFLAGS) $(FLAGS_OBJCXX_$<) $(CPPFLAGS) -c $< -o $@
.c.o:
	$(CC) $(CFLAGS) $(FLAGS_C_$<) $(CPPFLAGS) -c $< -o $@
.cpp.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c $< -o $@
.cc.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c $< -o $@
.java.o:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) -c $< -o $@
.java.class:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) -d $(BUILDDIR) -C $<
.class.hh:
	$(GCJH) $(JHFLAGS) $(FLAGS_GCJH_$<) $< -o $@
	@$(TOUCH) $@ || true
.l.c:
	$(LEX) $(LFLAGS) $(FLAGS_LEX_$<) -o$@ $<
.ll.cc:
	$(LEX) $(LCXXFLAGS) $(FLAGS_LEX_$<) -o$@ $<
.llj.java:
	$(LEX) $(LJFLAGS) $(FLAGS_LEX_$<) -o$@ $<
.y.c:
	$(YACC) $(YFLAGS) $(FLAGS_YACC_$<) -o $@ $<
	@case " $(YFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
	     if [ -e "$(@D)/y.tab.h" ]; then cmd='$(MV) $(@D)/y.tab.h $(@:.c=.h)'; echo "$$cmd"; $$cmd; fi ;; \
	 esac
.yy.cc:
	$(YACC) $(YCXXFLAGS) $(FLAGS_YACC_$<) -o $@ $<
	@case " $(YFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
	     if [ -e "$(@:.cc=.h)" ]; then cmd='$(MV) $(@:.cc=.h) $(@:.cc=.hh)'; echo "$$cmd"; $$cmd; \
	     elif [ -e "$(@D)/y.tab.h" ]; then cmd='$(MV) $(@D)/y.tab.h $(@:.cc=.hh)'; echo "$$cmd"; $$cmd; fi; \
	 esac
.yyj.java:
	$(YACC) $(YJFLAGS) $(FLAGS_YACC_$<) -o $@ $<
.y.h:
	@[ ! -e $@ ] && $(TOUCH) $@ && $(MAKE) $(@:.h=.c) || true
.yy.hh:
	@[ ! -e $@ ] && $(TOUCH) $@ && $(MAKE) $(@:.hh=.cc) || true
############################################################################################

#@#cd "$(DISTDIR)" && ($(ZIP) -q -r "$${distname}.zip" "$${distname}" || true)
dist:
	@version=`$(GREP) -E '^[[:space:]]*\#define APP_VERSION[[:space:]][[:space:]]*"' $(VERSIONINC) | $(SED) -e 's/^.*"\([^"]*\)".*/\1/'` \
	 && distname="$(NAME)_$${version}_`$(DATE) '+%Y-%m-%d_%Hh%M'`" \
	 && topdir=`pwd` \
	 && $(MKDIR) -p "$(DISTDIR)/$${distname}" \
	 && cp -Rf . "$(DISTDIR)/$${distname}" \
	 && $(RM) -R `$(FIND) "$(DISTDIR)/$${distname}" -type d -and \( -name '.git' -or -name 'CVS' -or -name '.hg' -or -name '.svn' \) $(NO_STDERR)` \
	 && true || { ver="$(DISTDIR)/$${distname}/$(VERSIONINC)"; rev=`$(SED) -n -e 's/GITREV\(.*\)/DIST_GITREV\1/p' $(BUILDINC) | tr '\n' '@'`;\
	      $(SED) -e "s,^\([[:space:]]*#[[:space:]]*include\),$${rev}@\1," "$$ver" | tr '@' '\n' > "$${ver}.tmp" && $(MV) "$${ver}.tmp" "$$ver"; false; } \
	 && $(PRINTF) "$(NAME): building dist...\n" \
	 && cd "$(DISTDIR)/$${distname}" && $(MAKE) distclean && $(MAKE) && $(MAKE) distclean && cd "$$topdir" \
	 && cd "$(DISTDIR)" && { $(TAR) czf "$${distname}.tar.gz" "$${distname}" && targz=true || targz=false; \
     			         $(TAR) cJf "$${distname}.tar.xz" "$${distname}" || $${targz}; } && cd "$$topdir" \
	 && $(RM) -R "$(DISTDIR)/$${distname}" \
	 && $(PRINTF) "$(NAME): archives created: $$(ls $(DISTDIR)/$${distname}.* | $(TR) '\n' ' ')\n"

$(SRCINC): $(SRCINC_CONTENT)
	@# Generate $(SRCINC) containing all sources.
	@$(PRINTF) "$(NAME): generate $@\n"
	@$(MKDIR) -p $(@D)
	@$(PRINTF) "/* generated content */\n" > $(SRCINC) ; \
		$(AWK) 'BEGIN { dbl_bkslash="\\"; gsub(/\\/, "\\\\\\", dbl_bkslash); o="awk on ubuntu 12.04"; \
	                        if (dbl_bkslash=="\\\\") dbl_bkslash="\\\\\\"; else dbl_bkslash="\\\\"; \
				print "#include <stdlib.h>\n#include \"$(VERSIONINC)\"\n#ifdef APP_INCLUDE_SOURCE\n" \
				      "static const char * const s_program_source[] = {"; } \
		   function printblk() { \
	               gsub(/\\/, dbl_bkslash, blk); \
                       gsub(/"/, "\\\"", blk); \
	               gsub(/\n/, "\\n\"\n\"", blk); \
	               print "\"" blk "\\n\","; \
	           } { \
		       if (curfile != FILENAME) { \
		           curfile="/* FILE: $(NAME)/" FILENAME " */"; blk=blk "\n" curfile; curfile=FILENAME; \
	               } if (length($$0 " " blk) > 500) { \
	                   printblk(); blk=$$0; \
                       } else \
		           blk=blk "\n" $$0; \
		   } END { \
		       printblk(); print "NULL };\nconst char *const* $(NAME)_get_source() { return s_program_source; }\n#endif"; \
		   }' $(SRCINC_CONTENT) >> $(SRCINC) ; \
	     $(CC) -I. -c $(SRCINC) -o $(SRCINC).tmp.o $(NO_STDERR) \
	         || $(PRINTF) "#include <stdlib.h>\n#include \"$(VERSIONINC)\"\n#ifdef APP_INCLUDE_SOURCE\nstatic const char * const s_program_source[] = {\n  \"cannot include source. check awk version or antivirus or bug\\\\n\", NULL};\nconst char *const* $(NAME)_get_source() { return s_program_source; }\n#endif\n" > $(SRCINC); \
	     $(RM) -f $(SRCINC).*

$(LICENSE):
	@echo "$(NAME): create $(LICENSE)"
	@$(PRINTF) "GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007 - http://www.gnu.org/licenses/\n" > $(LICENSE)

$(README):
	@echo "$(NAME): create $(README)"
	@$(PRINTF) "## $(NAME)\n---------------\n\n* [Overview](#overview)\n* [License](#license)\n\n" > $(README)
	@$(PRINTF) "## Overview\nTODO !\n\n## License\nGPLv3 or later. See LICENSE file.\n" >> $(README)

$(VERSIONINC):
	@echo "$(NAME): create $(VERSIONINC)"
	@$(PRINTF) "#ifndef APP_VERSION_H\n#define APP_VERSION_H\n#define APP_VERSION \"0.1\"\n" > $(VERSIONINC)
	@$(PRINTF) "#define APP_INCLUDE_SOURCE\n#define APP_BUILD_NUMBER 1\n#include \"build.h\"\n#endif\n" >> $(VERSIONINC)

$(BUILDINC): $(VERSIONINC)
	@if ! $(TEST) -e $(BUILDINC); then \
	     echo "$(NAME): create $(BUILDINC)"; \
	     build=`$(SED) -n -e 's/^[[:space:]]*#define[[:space:]]APP_BUILD_NUMBER[[:space:]][[:space:]]*\([0-9][0-9]*\).*/\1/p' $(VERSIONINC)`; \
	     $(PRINTF) "#define BUILD_APPNAME \"\"\n#define BUILD_NUMBER $$build\n#define BUILD_GITREV \"\"\n" > $(BUILDINC); \
	     $(PRINTF) "#define BUILD_FULLGITREV \"\"\n#define BUILD_PREFIX \"\"\n#define BUILD_SRCPATH \"\"\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_CC_CMD \"\"\n#define BUILD_CXX_CMD \"\"\n#define BUILD_OBJC_CMD \"\"\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_GCJ_CMD \"\"\n#define BUILD_CCLD_CMD \"\"\n#define BUILD_JAVAOBJ 0\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_JAR 0\n#define BUILD_BIN 0\n#define BUILD_LIB 0\n" >> $(BUILDINC); \
     	     $(PRINTF) "#define BUILD_YACC 0\n#define BUILD_LEX 0\n#define BUILD_BISON3 0\n" >> $(BUILDINC); \
	 fi;

#fullgitrev=`$(GIT) describe --match "v[0-9]*" --always --tags --dirty --abbrev=0 $(NO_STDERR)`

update-$(BUILDINC): $(BUILDINC)
	@if gitstatus=`$(GIT) status --untracked-files=no --ignore-submodules=untracked --short --porcelain $(NO_STDERR)`; then \
	     i=0; for rev in `$(GIT) show --quiet --ignore-submodules=untracked --format="%h %H" HEAD $(NO_STDERR)`; do \
	         case $$i in 0) gitrev="$$rev";; 1) fullgitrev="$$rev" ;; esac; \
       	         i=$$((i+1)); \
	     done; if [ -n "$$gitstatus" ]; then gitrev="$${gitrev}-dirty"; fullgitrev="$${fullgitrev}-dirty"; fi; \
	 else gitrev="unknown"; fullgitrev="$${gitrev}"; fi; \
 	 case " $(OBJ) " in *" $(JAVAOBJ) "*) javaobj=1;; *) javaobj=0;; esac; \
	 [ -n "$(JAR)" ] && jar=1 || jar=0; \
	 [ -n "$(BIN)" ] && bin=1 || bin=0; \
	 [ -n "$(LIB)" ] && lib=1 || lib=0; \
	 [ -n "$(YACC)" ] && yacc=1 || yacc=0; \
	 [ -n "$(LEX)" ] && lex=1 || lex=0; \
	 [ -n "$(BISON3)" ] && bison3=1 || bison3=0; \
	 if $(SED) -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREV[[:space:]]\).*|\1\"$${gitrev}\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_FULLGITREV[[:space:]]\).*|\1\"$${fullgitrev}\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_PREFIX[[:space:]]\).*|\1\"$(PREFIX)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SRCPATH[[:space:]]\).*|\1\"$$PWD\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_APPNAME[[:space:]]\).*|\1\"$(NAME)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CC_CMD[[:space:]]\).*|\1\"$(CC) $(CFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CXX_CMD[[:space:]]\).*|\1\"$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_OBJC_CMD[[:space:]]\).*|\1\"$(OBJC) $(OBJCFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GCJ_CMD[[:space:]]\).*|\1\"$(GCJ) $(JFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CCLD_CMD[[:space:]]\).*|\1\"$(CCLD) $(LDFLAGS)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAVAOBJ[[:space:]]\).*|\1$${javaobj}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAR[[:space:]]\).*|\1$${jar}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_BIN[[:space:]]\).*|\1$${bin}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_LIB[[:space:]]\).*|\1$${lib}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_YACC[[:space:]]\).*|\1$${yacc}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_LEX[[:space:]]\).*|\1$${lex}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_BISON3[[:space:]]\).*|\1$${bison3}|" \
	        $(BUILDINC) > $(BUILDINC).tmp \
	 ; then \
	    if $(DIFF) -q $(BUILDINC) $(BUILDINC).tmp $(NO_STDOUT); then $(RM) $(BUILDINC).tmp; \
            else $(MV) $(BUILDINC).tmp $(BUILDINC) && echo "$(NAME): $(BUILDINC) updated"; fi; \
	 fi

#.gitignore: $(ALLMAKEFILES)
.gitignore:
	@{ cat .gitignore $(NO_STDERR); for f in $(BIN) $(BIN).dSYM $(LIB) $(JAR) $(GENSRC) $(GENJAVA) $(GENINC) $(SRCINC) $(BUILDINC) $(CLANGCOMPLETE) \
	                                         '*.o' '*.d' '*.class' '*.a' '*~' '.*.swo' '.*.swp'; do \
	    $(PRINTF) "$$f\n" | $(SED) -e 's|^./||'; done; } \
	 | $(SORT) | $(UNIQ) > .gitignore

gentags: $(CLANGCOMPLETE)
$(CLANGCOMPLETE): $(ALLMAKEFILES) $(BUILDINC)
	@echo "$(NAME): update $(CLANGCOMPLETE)"
	@src=`echo $(SRCDIR) | $(SED) -e 's|\.|\\\.|g'`; $(TEST) -e $(CLANGCOMPLETE) \
		&& $(SED) -e "s%^[^#]*-I$$src[[:space:]].*%$(CPPFLAGS) %" -e "s%^[^#]*-I$$src$$%$(CPPFLAGS)%" \
	             "$(CLANGCOMPLETE)" $(NO_STDERR)  > "$(CLANGCOMPLETE).tmp" \
	        && mv "$(CLANGCOMPLETE).tmp" "$(CLANGCOMPLETE)" \
	    || echo "$(CPPFLAGS)" > $(CLANGCOMPLETE)

# to spread 'generic' makefile part to sub-directories
merge-makefile:
	@for makefile in `$(FIND) $(SUBDIRS) -name Makefile | $(SORT) | $(UNIQ)`; do \
	     $(GREP) -E -i -B10000 '^[[:space:]]*#[[:space:]]*generic[[:space:]]part' "$${makefile}" > "$${makefile}.tmp" \
	     && $(GREP) -E -i -A10000 '^[[:space:]]*#[[:space:]]*generic[[:space:]]part' Makefile | tail -n +2 >> "$${makefile}.tmp" \
	     && mv "$${makefile}.tmp" "$${makefile}" && echo "merged $${makefile}" || echo "! cannot merge $${makefile}" && $(RM) -f "$${makefile}.tmp"; \
	     file=make-fallback; target="`dirname $${makefile}`/$${file}"; if [ -e "$$file" ] && [ -e "$$target" ]; then \
	         $(GREP) -E -i -B10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$$target" > "$${target}.tmp" \
	         && $(GREP) -E -i -A10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$$file" | tail -n +2 >> "$${target}.tmp" \
	         && mv "$${target}.tmp" "$${target}" && echo "merged $${target}" && chmod +x "$$target" || echo "! cannot merge $${target}" && $(RM) -f "$${target}.tmp"; \
	     fi; \
	 done

#to generate makefile displaying shell command beeing run
debug-makefile:
	@sed -e 's/^\(cmd_[[:space:]0-9a-zA-Z_]*\)=/\1= ls $(NAME)\/\1 || time /' Makefile > Makefile.debug
	@$(MAKE) -f Makefile.debug

info:
	@echo "UNAME_SYS        : $(UNAME_SYS)"
	@#echo "UNAME_ARCH       : $(UNAME_ARCH)"
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
	@echo "GCJ              : $(GCJ)"
	@echo "GCJH             : $(GCJH)"
	@echo "CPP              : $(CPP)"
	@echo "CCLD             : $(CCLD)"
	@echo "YACC             : $(YACC)"
	@echo "BISON3           : $(BISON3)"
	@echo "LEX              : $(LEX)"
	@echo "CFLAGS           : $(CFLAGS)"
	@echo "CXXFLAGS         : $(CXXFLAGS)"
	@echo "OBJCFLAGS        : $(OBJCFLAGS)"
	@echo "JFLAGS           : $(JFLAGS)"
	@echo "CPPFLAGS         : $(CPPFLAGS)"
	@echo "LDFLAGS          : $(LDFLAGS)"
	@echo "YFLAGS           : $(YFLAGS)"
	@echo "YCXXFLAGS        : $(YCXXFLAGS)"
	@echo "YJFLAGS          : $(YJFLAGS)"
	@echo "LFLAGS           : $(LFLAGS)"
	@echo "LCXXFLAGS        : $(LCXXFLAGS)"
	@echo "LJFLAGS          : $(LJFLAGS)"
	@echo "SRCDIR           : $(SRCDIR)"
	@echo "DISTDIR          : $(DISTDIR)"
	@echo "BUILDDIR         : $(BUILDDIR)"
	@echo "PREFIX           : $(PREFIX)"
	@echo "BIN              : $(BIN)"
	@echo "LIB              : $(LIB)"
	@echo "METASRC          : $(METASRC)"
	@echo "GENINC           : $(GENINC)"
	@echo "GENSRC           : $(GENSRC)"
	@echo "GENJAVA          : $(GENJAVA)"
	@echo "INCLUDES         : $(INCLUDES)"
	@echo "SRC              : $(SRC)"
	@echo "JAVASRC          : $(JAVASRC)"
	@echo "OBJ              : $(OBJ)"
	@echo "CLASSES          : $(CLASSES)"

.PHONY: subdirs $(SUBDIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(TESTDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: subdirs $(DISTCLEANDIRS)
.PHONY: subdirs $(DEBUGDIRS)
.PHONY: subdirs $(DOCDIRS)
.PHONY: default_rule all build_all clean distclean dist test info debug doc install \
	gentags update-$(BUILDINC) .gitignore merge-makefile debug-makefile

############################################################################
#$(BUILDDIR)/%.o: $(SRCDIR)/%.m $(INCLUDES) $(ALLMAKEFILES)
#	@$(MKDIR) -p $(@D)
#	$(OBJC) -c $< $(OBJCFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(INCLUDES) $(ALLMAKEFILES)
#	@$(MKDIR) -p $(@D)
#	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(INCLUDES) $(ALLMAKEFILES)
#	@$(MKDIR) -p $(@D)
#	$(CXX) -c $< $(CXXFLAGS) $(CPPFLAGS) -o $@

#$(BUILDDIR)/%.o: $(SRCDIR)/%.cc $(INCLUDES) $(ALLMAKEFILES)
#	@$(MKDIR) -p $(@D)
#	$(CXX) -c $< $(CXXFLAGS) $(CPPFLAGS) -o $@
