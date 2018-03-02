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

# Name of the Package (DISTNAME, BIN and LIB depends on it)
NAME		= vlib

# SRCDIR: Folder where sources are. Use '.' for current directory. MUST NEVER BE EMPTY !!
# Folders which contains a Makefile are ignored, you have to add them in SUBDIRS and update SUBLIBS.
# RESERVED FILES for internal use: ./build.h, ./version.h ./Makefile ./Build.java $(BUILDDIR)/_src_.c
SRCDIR 		= src

# SUBMODROOTDIR, allowing to group all submodules together instead of creating a complex tree
# in case the project (A) uses module B (which uses module X) and module C (which uses module X).
# Put empty value, or don't use it in sub directories' Makefile to disable this feature.
SUBMODROOTDIR	= ext

# SUBDIRS, put empty if there is no need to run make on sub directories.
SUBDIRS 	=
# SUBLIBS: libraries produced from SUBDIRS, needed correct build order. Put empty if none.
SUBLIBS		=

# INCDIRS: Folder where public includes are. It can be SRCDIR or even empty if
# headers are only in SRCDIR. Use '.' for current directory.
INCDIRS 	= include

# Where targets are created (OBJs, BINs, ...). Eg: '.' or 'build'. ONLY 'SRCDIR' is supported!
BUILDDIR	= $(SRCDIR)

# Binary name and library name (prefix with '$(BUILDDIR)/' to put it in build folder).
# Fill LIB and set BIN,JAR empty to create a library, or clear LIB,JAR and set BIN to create a binary.
BIN		=
LIB		= lib$(NAME).a
JAR		=

# DISTDIR: where the dist packages zip/tar.xz are saved
DISTDIR		= ../../dist

# PREFIX: where the application is to be installed
PREFIX		= /usr/local
INSTALL_FILES	=

# Project specific Flags (system specific flags are handled further)
# Choice between <flag>_RELEASE/_DEBUG is done according to BUILDINC / make debug
WARN_RELEASE	= -Wall -W -pedantic # -Wno-ignored-attributes -Wno-attributes
ARCH_RELEASE	= -march=native # -arch i386 -arch x86_64
OPTI_COMMON	= -pipe -fstack-protector
OPTI_RELEASE	= -O3 $(OPTI_COMMON)
INCS_RELEASE	=
LIBS_RELEASE	= $(SUBLIBS) -lpthread
MACROS_RELEASE	=
WARN_DEBUG	= $(WARN_RELEASE) # -Werror
ARCH_DEBUG	= $(ARCH_RELEASE)
OPTI_DEBUG	= -O0 -g $(OPTI_COMMON)
INCS_DEBUG	= $(INCS_RELEASE)
LIBS_DEBUG	= $(LIBS_RELEASE)
MACROS_DEBUG	= -D_DEBUG -D_TEST
# FLAGS_<lang> is global for one language (<lang>: C,CXX,OBJC,GCJ,GCJH,OBJCXX,LEX,YACC).
FLAGS_C		= -std=c99 -D_GNU_SOURCE
FLAGS_CXX	= -D_GNU_SOURCE -Wno-variadic-macros
FLAGS_OBJC	= -std=c99
FLAGS_OBJCXX	=
FLAGS_GCJ	=

# FLAGS_<lang>_<file> is specific to one file (eg:'FLAGS_CXX_Big.cc=-O0','FLAGS_C_src/a.c=-O1')

# System specific flags (WARN_$(sys),OPTI_$(sys),DEBUG_$(sys),LIBS_$(sys),INCS_$(sys))
# $(sys) is lowcase(`uname`), eg: 'LIBS_darwin=-framework IOKit -framework Foundation'
#  + For clang++ on darwin, use libstdc++ to have gnu extension __gnu_cxx::stdio_filebuf
#  + Comment '*_GNUCXX_XTRA_* = *' lines to use default libc++ and use '#ifdef __GLIBCXX__' in your code.
#FLAGS_GNUCXX_XTRA_darwin_/usr/bin/clangpppp=-stdlib=libstdc++
#LIBS_GNUCXX_XTRA_darwin_/usr/bin/clangpppp=-stdlib=libstdc++
INCS_darwin	= $(FLAGS_GNUCXX_XTRA_$(UNAME_SYS)_$(CXX:++=pppp))
LIBS_darwin	= -framework IOKit -framework Foundation $(LIBS_GNUCXX_XTRA_$(UNAME_SYS)_$(CXX:++=pppp))

# TESTS and DEBUG parameters
# VALGRIND_RUN_PROGRAM: how to run the program with valgrind (can be used to pass arguments to valgrind)
#   (eg: './$(BIN) arguments', '--trace-children=no ./$(BIN) arguments')
VALGRIND_RUN_PROGRAM = ./$(BIN)
# VALGRIND_MEM_IGNORE_PATTERN: awk regexp to ignore keyworks in LEAKS reports
VALGRIND_MEM_IGNORE_PATTERN = __CFInitialize|_objc_init|objc_msgSend|_NSInitializePlatform
# TEST_RUN_PROGRAM: what to run with 'make test' (eg: 'true', './test.sh $(BIN)', './$(BIN) --test'
TEST_RUN_PROGRAM = true

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
INSTALLBIN	= install -c -m 0755
INSTALLDIR	= install -c -d -m 0755
VALGRIND	= valgrind
MKTEMP		= mktemp
NO_STDERR	= 2> /dev/null
NO_STDOUT	= > /dev/null
STDOUT_TO_ERR	= 1>&2

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
BUILDINCJAVA	= Build.java
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

# Get system name
cmd_UNAME_SYS	= uname | $(TR) '[A-Z]' '[a-z]' | $(SED) -e 's/[^A-Za-z0-9]/_/g'
tmp_UNAME_SYS	!= $(cmd_UNAME_SYS)
tmp_UNAME_SYS	?= $(shell $(cmd_UNAME_SYS))
UNAME_SYS	:= $(tmp_UNAME_SYS)
SYSDEP_SUF	= $(UNAME_SYS)
SYSDEP_SUF_DEF	= default

#cmd_UNAME_ARCH	:= uname -m | $(TR) '[A-Z]' '[a-z]'
#tmp_UNAME_ARCH	!= $(cmd_UNAME_ARCH)
#tmp_UNAME_ARCH	?= $(shell $(cmd_UNAME_ARCH))
#UNAME_ARCH	:= $(tmp_UNAME_ARCH)

# Search bison 3 or later, fallback on bison, yacc.
cmd_YACC        = found=; for bin in $$($(WHICH) -a bison $(YACC) $(NO_STDERR)); do \
		      ver="$$($$bin -V 2>&1 | $(AWK) -F '.' '/[Bb][iI][sS][oO][nN].*[0-9]+(\.[0-9]+)+/ { \
		                                               $$0=substr($$0,match($$0,/[0-9]+(\.[0-9]+)+/)); \
		                                               print $$1*1000000 + $$2*1000 + $$3*1 }')"; \
		      $(TEST) -n "$$ver" && $(TEST) $$ver -ge 03000000 $(NO_STDERR) && found="$${bin}._have_bison3_" && break; \
		  done; $(TEST) -n "$$found" && $(PRINTF) "$$found" || $(WHICH) $(YACC) bison yacc $(NO_STDERR) | $(HEADN1) || true
tmp_YACC0	!= $(cmd_YACC)
tmp_YACC0	?= $(shell $(cmd_YACC))
tmp_YACC	:= $(tmp_YACC0)
BISON3		:= $(tmp_YACC)
tmp_YACC	:= $(tmp_YACC:._have_bison3_=)
BISON3$(tmp_YACC)._have_bison3_ := $(tmp_YACC)
BISON3		:= $(BISON3$(BISON3))
YACC		:= $(tmp_YACC)

# Search flex, lex, and find the location of corresponding FlexLexer.h needed by C++ Scanners.
# Depending on gcc include search paths, the wrong FlexLexer.h could be chosen if you have
# several flex on your system -> create link to correct FlexLexer.h.
# Particular case on MacOS where flex is a wrapper to xcode, meaning
# $(dirname flex)/../include/FlexLexer.h does not exist.
FLEXLEXER_INC	= FlexLexer.h
FLEXLEXER_LNK	= $(BUILDDIR)/$(FLEXLEXER_INC)
$(FLEXLEXER_LNK):
cmd_LEX		= lex=`$(WHICH) $(LEX) flex lex $(NO_STDERR) | $(HEADN1)`; \
		  $(TEST) -n "$$lex" -a \( ! -e "$(FLEXLEXER_LNK)" -o -L "$(FLEXLEXER_LNK)" \) \
		  && flexinc="`dirname $$lex`/../include/$(FLEXLEXER_INC)" \
		  && $(TEST) -e "$$flexinc" \
		  || { $(TEST) "$(UNAME_SYS)" = "darwin" \
		       && otool -L $$lex | $(GREP) -Eq 'libxcselect[^ ]*dylib' $(NO_STDERR) \
		       && flexinc="`xcode-select -p $(NO_STDERR)`/Toolchains/Xcodedefault.xctoolchain/usr/include/$(FLEXLEXER_INC)" \
		       && $(TEST) -e "$$flexinc"; } \
		  && ! $(TEST) $(FLEXLEXER_LNK) -ef "$$flexinc" && echo 1>&2 "$(NAME): create link $(FLEXLEXER_LNK) -> $$flexinc" \
		  && ln -sf "$$flexinc" "$(FLEXLEXER_LNK)" $(NO_STDERR) && $(TEST) -e $(BUILDINC) && $(TOUCH) $(BUILDINC); \
		  echo $$lex
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
# Common find pattern to include files in SRCDIR/sysdeps ONLY if suffixed with system name,
# or the one suffixed with 'default' if not found.
find_AND_SYSDEP	= -and \( \! -path '$(SRCDIR)/$(SYSDEPDIR)/*' \
		          -or -path '$(SRCDIR)/$(SYSDEPDIR)/*$(SYSDEP_SUF).*' \
		          -or \( -path '$(SRCDIR)/$(SYSDEPDIR)/*$(SYSDEP_SUF_DEF).*' \
		                 -and \! \( -exec $(SHELL) -c "echo \"{}\" \
		                   | $(SED) -e 's|$(SYSDEP_SUF_DEF)\(\.[^.]*\)$$|$(SYSDEP_SUF)\1|' \
		                   | xargs $(TEST) -e " \; \) \) \)

# Search Meta sources (used to generate sources)
# For yacc/bison and lex/flex:
#   - the basename of meta sources must be always different (have one grammar calc.y and one
#     lexer calc.l is not supported: prefer parse-calc.y and scan-calc.l).
#   - c++ source is generated with .ll and .yy, c source with .l and .y, java with .yyj
#   - .l,.ll included if LEX is found, .y,.yy included if YACC is found, .yyj included
#     if BISON3 AND ((GCJ and BIN are defined) OR (JAR defined)).
#   - yacc generates by default headers for lexer, therefore lexer files depends on parser files.
cmd_YACCSRC	= $(TEST) -n "$(YACC)" && $(FIND) $(SRCDIR) \( -name '*.y' -or -name '*.yy' \) \
		                            $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_LEXSRC	= $(TEST) -n "$(LEX)" && $(FIND) $(SRCDIR) \( -name '*.l' -or -name '*.ll' \) \
		                           $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_YACCJAVA	= $(TEST) \( \( -n "$(BIN)" -a -n "$(GCJ)" \) -o -n "$(JAR)" \) -a  -n "$(BISON3)" \
		  && $(FIND) $(SRCDIR) -name '*.yyj' \
		             $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
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
cmd_JAVASRC	= $(TEST) \( -n "$(BIN)" -a -n "$(GCJ)" \) -o -n "$(JAR)" \
		  && $(FIND) $(SRCDIR) \( -name '*.java' \) \
		       -and \! -path $(BUILDINCJAVA) -and \! -path ./$(BUILDINCJAVA) \
		       $(find_AND_NOGEN) $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
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
 		    $(find_AND_NOGEN) -and \! -path '$(SRCINC)' -and \! -path './$(SRCINC)' \
		    $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||'
cmd_INCLUDES	= $(FIND) $(INCDIRS) $(SRCDIR) \( -name '*.h' -or -name '*.hh' -or -name '*.hpp' \) \
		    $(find_AND_NOGEN) $(find_AND_NOGEN2) \
		    -and \! -path $(VERSIONINC) -and \! -path ./$(VERSIONINC) \
		    -and \! \( -path $(FLEXLEXER_LNK) -and -type l \) \
		    -and \! -path $(BUILDINC) -and \! -path ./$(BUILDINC) $(find_AND_SYSDEP) \
		    -print $(NO_STDERR) | $(SED) -e 's|^\./||'

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
tmp_OBJ1	:= $(SRC:.m=.o)
tmp_OBJ2	:= $(tmp_OBJ1:.mm=.o)
tmp_OBJ3	:= $(tmp_OBJ2:.cpp=.o)
tmp_OBJ4	:= $(tmp_OBJ3:.cc=.o)
OBJ_NOJAVA	:= $(tmp_OBJ4:.c=.o)
cmd_SRC_BUILD	:= echo " $(OBJ_NOJAVA)" | $(SED) -e 's| $(SRCDIR)/| $(BUILDDIR)/|g'; \
		  case " $(JAVASRC) " in *" "*".java "*) $(TEST) -n "$(GCJ)" -a -n "$(BIN)" && echo "$(JAVAOBJ)";; esac
tmp_SRC_BUILD	!= $(cmd_SRC_BUILD)
tmp_SRC_BUILD	?= $(shell $(cmd_SRC_BUILD))
OBJ		:= $(tmp_SRC_BUILD)

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

JAVA		= java
JARBIN		= jar
JAVAC		= javac
JAVAH		= javah

############################################################################################

sys_LIBS	= $(LIBS_$(SYSDEP_SUF))
sys_INCS	= $(INCS_$(SYSDEP_SUF))
sys_OPTI	= $(OPTI_$(SYSDEP_SUF))
sys_WARN	= $(WARN_$(SYSDEP_SUF))
sys_DEBUG	= $(DEBUG_$(SYSDEP_SUF))

############################################################################################
# Generic Build Flags, taking care of system specific flags (sys_*)
cmd_CPPFLAGS	= echo "-I."; $(TEST) "$(SRCDIR)" != "." && echo "-I$(SRCDIR)"; \
		  $(TEST) "$(SRCDIR)" != "$(BUILDDIR)" && echo "-I$(BUILDDIR)"; \
		  for dir in $(INCDIRS); do $(TEST) "$$dir" != "$(SRCDIR)" -a "$$dir" != "." && echo "-I$$dir"; done; true
tmp_CPPFLAGS	!= $(cmd_CPPFLAGS)
tmp_CPPFLAGS	?= $(shell $(cmd_CPPFLAGS))
tmp_CPPFLAGS	:= $(tmp_CPPFLAGS)

FLAGS_COMMON	= $(OPTI) $(WARN) $(ARCH)
CPPFLAGS	:= $(tmp_CPPFLAGS) $(sys_INCS) $(INCS) $(MACROS) -DHAVE_VERSION_H
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
BCOMPAT_SED_YYPREFIX=$(SED) -n -e \
	"s/^[[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*BCOMPAT_YYPREFIX[[:space:]][[:space:]]*\([A-Za-z_][A-Za-z0-9_]*\)/$${opt}\1/p" $<

############################################################################################
# GCC -MD management (dependencies generation)
# make on some BSD systems 1) does not support '-include' or 'sinclude', 2) does not support
# including several files in one include statement, and 3) does not see a file created before
# inclusion by a shell command '!=' or '$(shell ...)'.
# Here, if .alldeps does not exit, we include version.h (containing only lines starting with
# dash(#), so that it can be parsed by make and do nothing), and the OBJs will depends on
# $(OBJDEPS_version.h). In the same time, we create .alldeps.d containing inclusion of
# all .d files, created with default headers dependency (OBJ depends on all includes), that
# will be used on next 'make' and overrided by gcc -MMD.
# Additionnaly, we use this command to populate git submodules if needed.
#
OBJDEPS_version.h= $(INCLUDES) $(GENINC) $(ALLMAKEFILES)
DEPS		:= $(OBJ:.o=.d)
INCLUDEDEPS	:= .alldeps.d
cmd_SINCLUDEDEPS= inc=1; if $(TEST) -e $(INCLUDEDEPS); then echo "$(INCLUDEDEPS)"; \
		  else inc=; echo version.h; fi; \
		  for f in $(DEPS:.d=); do \
		      if $(TEST) -z "$$inc" -o ! -e "$$f.d"; then \
		           $(TEST) "$$f.o" = "$(JAVAOBJ)" && echo "" > $$f.d \
			                                  || echo "$$f.o: $(OBJDEPS_version.h)" > $$f.d; \
		           echo "include $$f.d" >> $(INCLUDEDEPS); \
	      	      fi; \
		  done; \
		  ret=true; $(TEST) -x "$(GIT)" || true && for d in $(SUBDIRS); do \
		      if ! $(TEST) -e "$$d/Makefile" && $(GIT) submodule status "$$d" $(NO_STDERR) | $(GREP) -Eq "^-.*$$d"; then \
		          $(GIT) submodule update --init "$$d" $(STDOUT_TO_ERR) || ret=false; \
		      fi; \
		  done; $$ret
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

# RECURSEMAKEARGS, see doc for SUBMODROOTDIR above.
# If the submodule is fetched alone, it will use its own submodules, if it is fetched as a
# submodule, it will use the root submodule directory, redefined when recursing in SUBDIRS.
RECURSEMAKEARGS	= $(TEST) -n "$(SUBMODROOTDIR)" && recargs="SUBMODROOTDIR=\"`echo $${recdir} \
				| $(SED) -e 's/[^/][^/]*/../g'`/$(SUBMODROOTDIR)\"" || recargs=; \
		  echo "cd $${recdir} && $(MAKE) $${rectarget} $${recargs}"

############################################################################################
# .POSIX: for bsd-like dependency management
# .PHONY: .WAIT and .EXEC for compatibility, when not supported.
# .EXEC is needed on some bsdmake, so as
# phony targets don't taint to outdated the files which depend on them.
# .WAIT might not be mandatory
.POSIX:
.PHONY: .WAIT .EXEC
default_rule: update-$(BUILDINC) $(BUILDDIRS) .WAIT $(BIN) $(LIB) $(JAR) gentags

$(SUBDIRS): $(BUILDDIRS)
$(SUBLIBS): $(BUILDDIRS)
	@true
$(BUILDDIRS): .EXEC
	@recdir=$(@:-build=); rectarget=; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs}

# --- clean : remove objects and generated files
clean: cleanme $(CLEANDIRS)
cleanme:
	$(RM) $(OBJ:.class=*.class) $(SRCINC) $(GENSRC) $(GENINC) $(GENJAVA) $(CLASSES:.class=*.class) $(DEPS) $(INCLUDEDEPS)
	@$(TEST) -L "$(FLEXLEXER_LNK)" && { cmd="$(RM) $(FLEXLEXER_LNK)"; echo "$$cmd"; $$cmd ; } || true
$(CLEANDIRS):
	@recdir=$(@:-clean=); rectarget=clean; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} clean

# --- distclean : remove objects, binaries and remove DEBUG flag in build.h
distclean: cleanme $(DISTCLEANDIRS)
	$(RM) $(BIN) $(LIB) $(BUILDINC) $(BUILDINCJAVA) valgrind_*.log `$(FIND) . -name '.*.swp' -or -name '.*.swo' -or -name '*~' -or -name '\#*' $(NO_STDERR)`
	$(RM) -R $(BIN).dSYM || true
	@$(TEST) "$(BUILDDIR)" != "$(SRCDIR)" && $(RMDIR) `$(FIND) $(BUILDDIR) -type d | $(SORT) -r` $(NO_STDERR) || true
	@$(PRINTF) "$(NAME): distclean done, debug disabled.\n"
$(DISTCLEANDIRS):
	@recdir=$(@:-distclean=); rectarget=distclean; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} distclean

# --- debug : set DEBUG flag in build.h and rebuild
debug: update-$(BUILDINC) $(DEBUGDIRS)
	@{ $(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(BUILD_DEBUG|BUILD_TEST)([[:space:]]|$$)' $(BUILDINC) $(NO_STDERR); \
		$(PRINTF) "#define BUILD_DEBUG\n#define BUILD_TEST\n"; } > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)
	@$(PRINTF) "$(NAME): debug enabled ('make distclean' to disable it).\n"
	@$(PRINTF) "$(NAME): "
	$(MAKE) SUBMODROOTDIR="$(SUBMODROOTDIR)"
$(DEBUGDIRS):
	@recdir=$(@:-debug=); rectarget=debug; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} debug
# Code to disable debug without deleting BUILDINC:
# @$(GREP) -Ev '^[[:space:]]*\#[[:space:]]*define[[:space:]]+(BUILD_DEBUG|BUILD_TEST)([[:space:]]|$$)' $(BUILDINC) \
#	    > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC)

# --- doc : generate doc
doc: $(DOCDIRS)
$(DOCDIRS):
	@recdir=$(@:-doc=); rectarget=doc; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} doc

# --- install ---
installme: all
	@for f in $(INSTALL_FILES); do \
	     case "$$f" in \
	         *.h|*.hh)    install="$(INSTALL)"; dest="$(PREFIX)/include" ;; \
	         *.a|*.so)    install="$(INSTALL)"; dest="$(PREFIX)/lib" ;; \
	         *)           if $(TEST) -x "$$f"; then \
	                          install="$(INSTALLBIN)"; dest="$(PREFIX)/bin"; \
		              else \
			          install="$(INSTALL)"; dest="$(PREFIX)/share/$(NAME)"; \
	                      fi ;; \
	     esac; \
	     if $(TEST) -n "$$install" -a -n "$$dest"; then \
	         dir=`dirname "$$dest"`; \
	         if ! $(TEST) -d "$$dir"; then cmd="$(INSTALLDIR) $$dir"; echo "$$cmd"; $$cmd; fi; \
		 cmd="$$install $$f $$dest"; echo "$$cmd"; \
		 $$cmd; \
	     fi; \
	 done
install: installme $(INSTALLDIRS)
$(INSTALLDIRS):
	@recdir=$(@:-install=); rectarget=install; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} install

# --- test ---
test: $(TESTDIRS) all
	$(TEST_RUN_PROGRAM)
$(TESTDIRS):
	@recdir=$(@:-test=); rectarget=test; $(RECURSEMAKEARGS); cd $${recdir} && $(MAKE) $${recargs} test

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
$(OBJ): $(ALLMAKEFILES) $(VERSIONINC) $(BUILDINC)
$(OBJ_NOJAVA): $(OBJDEPS_$(SINCLUDEDEPS))
$(GENSRC): $(ALLMAKEFILES) $(VERSIONINC) $(BUILDINC)
$(GENJAVA): $(ALLMAKEFILES) $(VERSIONINC) $(BUILDINC)
$(CLASSES): $(ALLMAKEFILES) $(VERSIONINC) $(BUILDINC)

# Implicit rules: old-fashionned double suffix rules to be compatible with most make.
.m.o:
	$(OBJC) $(OBJCFLAGS) $(FLAGS_OBJC_$<) $(CPPFLAGS) -c -o $@ $<
.mm.o:
	$(OBJCXX) $(OBJCXXFLAGS) $(FLAGS_OBJCXX_$<) $(CPPFLAGS) -c -o $@ $<
.c.o:
	$(CC) $(CFLAGS) $(FLAGS_C_$<) $(CPPFLAGS) -c -o $@ $<
.cpp.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
.cc.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
.java.o:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) $< -o $@ $<
.java.class:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) -d $(BUILDDIR) -C $<
.class.hh:
	$(GCJH) $(JHFLAGS) $(FLAGS_GCJH_$<) -o $@ $<
	@$(TOUCH) $@ || true
.l.c:
	@opt='-P'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
	 cmd="$(LEX) $(LFLAGS) $$args $(FLAGS_LEX_$<) -o$@ $<"; \
	 echo "$$cmd"; \
	 $$cmd
.ll.cc:
	@opt='-P'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
	 cmd="$(LEX) $(LCXXFLAGS) $$args $(FLAGS_LEX_$<) -o$@ $<"; \
	 echo "$$cmd"; \
	 $$cmd
.llj.java:
	$(LEX) $(LJFLAGS) $(FLAGS_LEX_$<) -o$@ $<
.y.c:
	@opt='-p'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
	 cmd="$(YACC) $(YFLAGS) $$args $(FLAGS_YACC_$<) -o $@ $<"; \
	 echo "$$cmd"; \
	 $$cmd
	@case " $(YFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
	     if $(TEST) -e "$(@D)/y.tab.h"; then cmd='$(MV) $(@D)/y.tab.h $(@:.c=.h)'; echo "$$cmd"; $$cmd; fi ;; \
	 esac
.yy.cc:
	@opt='-p'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
	 cmd="$(YACC) $(YCXXFLAGS) $$args $(FLAGS_YACC_$<) -o $@ $<"; \
	 echo "$$cmd"; \
	 $$cmd
	@case " $(YCXXFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
	     if $(TEST) -e "$(@:.cc=.h)"; then cmd='$(MV) $(@:.cc=.h) $(@:.cc=.hh)'; echo "$$cmd"; $$cmd; \
	     elif $(TEST) -e "$(@D)/y.tab.h"; then cmd='$(MV) $(@D)/y.tab.h $(@:.cc=.hh)'; echo "$$cmd"; $$cmd; fi; \
	 esac
.yyj.java:
	$(BISON3) $(YJFLAGS) $(FLAGS_YACC_$<) -o $@ $<
.y.h:
	@true #! $(TEST) -e $@ && $(TOUCH) $@ && $(MAKE) $(@:.h=.c) || true
.yy.hh:
	@true #! $(TEST) -e $@ && $(TOUCH) $@ && $(MAKE) $(@:.hh=.cc) || true
############################################################################################

#@#cd "$(DISTDIR)" && ($(ZIP) -q -r "$${distname}.zip" "$${distname}" || true)
dist:
	@version=`$(GREP) -E '^[[:space:]]*\#define APP_VERSION[[:space:]][[:space:]]*"' $(VERSIONINC) | $(SED) -e 's/^.*"\([^"]*\)".*/\1/'` \
	 && distname="$(NAME)_$${version}_`$(DATE) '+%Y-%m-%d_%Hh%M'`" \
	 && topdir=`pwd` \
	 && $(MKDIR) -p "$(DISTDIR)/$${distname}" \
	 && cp -Rf . "$(DISTDIR)/$${distname}" \
	 && $(RM) -R `$(FIND) "$(DISTDIR)/$${distname}" -type d -and \( -name '.git' -or -name 'CVS' -or -name '.hg' -or -name '.svn' \) $(NO_STDERR)` \
	 && { for d in . $(SUBDIRS); do ver="$(DISTDIR)/$${distname}/$$d/$(VERSIONINC)"; cd "$$d" && $(MAKE) update-$(BUILDINC); cd "$${topdir}"; \
	      pat=`$(SED) -n -e "s|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*BUILD_\(GIT[^[:space:]]*\)[[:space:]]*\"\(.*\)|-e 's,DIST_\1 .*,DIST_\1 \"?-from:\2,'|p" \
	           "$$d/$(BUILDINC)" | $(TR) '\n' ' '`; \
	      mv "$${ver}" "$${ver}.tmp" && eval "$(SED) $$pat $${ver}.tmp" > "$${ver}" && $(RM) "$${ver}.tmp"; done; } \
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
	@$(PRINTF) "GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007 - http://gnu.org/licenses/gpl.html\n" > $(LICENSE)

$(README):
	@echo "$(NAME): create $(README)"
	@$(PRINTF) "## $(NAME)\n---------------\n\n* [Overview](#overview)\n* [License](#license)\n\n" > $(README)
	@$(PRINTF) "## Overview\nTODO !\n\n## License\nGPLv3 or later. See LICENSE file.\n" >> $(README)

$(VERSIONINC):
	@echo "$(NAME): create $(VERSIONINC)"
	@$(PRINTF) "#ifndef APP_VERSION_H\n#define APP_VERSION_H\n#define APP_VERSION \"0.1\"\n" > $(VERSIONINC)
	@$(PRINTF) "#define APP_INCLUDE_SOURCE\n#define APP_BUILD_NUMBER 1\n#define DIST_GITREV \"unknown\"\n" >> $(VERSIONINC)
	@$(PRINTF) "#define DIST_GITREVFULL \"unknown\"\n#define DIST_GITREMOTE \"unknown\"\n" >> $(VERSIONINC)
	@$(PRINTF) "#include \"build.h\"\n#endif\n" >> $(VERSIONINC)

# As defined above, everything depends on $(BUILDINC), and we want they wait for update-$(BUILDINC)
# create-$(BUILDINC) and update-$(BUILDINC) have .EXEC so that some bsd-make don' taint to outdated
# the files which depends on them.
$(BUILDINC): update-$(BUILDINC)
	@true

create-$(BUILDINC): $(VERSIONINC) $(ALLMAKEFILES) .EXEC
	@if ! $(TEST) -e $(BUILDINC); then \
	     echo "$(NAME): create $(BUILDINC)"; \
	     build=`$(SED) -n -e 's/^[[:space:]]*#define[[:space:]]APP_BUILD_NUMBER[[:space:]][[:space:]]*\([0-9][0-9]*\).*/\1/p' $(VERSIONINC)`; \
	     $(PRINTF) "#define BUILD_APPNAME \"\"\n#define BUILD_NUMBER $$build\n#define BUILD_PREFIX \"\"\n" > $(BUILDINC); \
	     $(PRINTF) "#define BUILD_GITREV \"\"\n#define BUILD_GITREVFULL \"\"\n#define BUILD_GITREMOTE \"\"\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_APPRELEASE \"\"\n#define BUILD_SYSNAME \"\"\n#define BUILD_SYS_unknown\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_CC_CMD \"\"\n#define BUILD_CXX_CMD \"\"\n#define BUILD_OBJC_CMD \"\"\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_GCJ_CMD \"\"\n#define BUILD_CCLD_CMD \"\"\n#define BUILD_SRCPATH \"\"\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_JAVAOBJ 0\n#define BUILD_JAR 0\n#define BUILD_BIN 0\n#define BUILD_LIB 0\n" >> $(BUILDINC); \
	     $(PRINTF) "#define BUILD_YACC 0\n#define BUILD_LEX 0\n#define BUILD_BISON3 0\n#define BUILD_CURSES 1\n" >> $(BUILDINC); \
	     $(PRINTF) "#ifdef __cplusplus\nextern \"C\" {\n#endif\nconst char *const* $(NAME)_get_source();\n#ifdef __cplusplus\n}\n#endif\n" >> $(BUILDINC); \
	 fi;

#fullgitrev=`$(GIT) describe --match "v[0-9]*" --always --tags --dirty --abbrev=0 $(NO_STDERR)`

update-$(BUILDINC): create-build.h .EXEC
	@if gitstatus=`$(GIT) status --untracked-files=no --ignore-submodules=untracked --short --porcelain $(NO_STDERR)`; then \
	     i=0; for rev in `$(GIT) show --quiet --ignore-submodules=untracked --format="%h %H" HEAD $(NO_STDERR)`; do \
	         case $$i in 0) gitrev="$$rev";; 1) fullgitrev="$$rev" ;; esac; \
       	         i=$$((i+1)); \
	     done; if $(TEST) -n "$$gitstatus"; then gitrev="$${gitrev}-dirty"; fullgitrev="$${fullgitrev}-dirty"; fi; \
	     gitremote="\"`$(GIT) remote get-url origin`\""; \
	     gitrev="\"$${gitrev}\""; fullgitrev="\"$${fullgitrev}\""; \
	     else gitrev="DIST_GITREV"; fullgitrev="DIST_GITREVFULL"; gitremote="DIST_GITREMOTE"; fi; \
 	 case " $(OBJ) " in *" $(JAVAOBJ) "*) javaobj=1;; *) javaobj=0;; esac; \
	 $(TEST) -n "$(JAR)" && jar=1 || jar=0; \
	 $(TEST) -n "$(BIN)" && bin=1 || bin=0; \
	 $(TEST) -n "$(LIB)" && lib=1 || lib=0; \
	 $(TEST) -n "$(YACC)" && yacc=1 || yacc=0; \
	 $(TEST) -n "$(LEX)" && lex=1 || lex=0; \
	 $(TEST) -n "$(BISON3)" && bison3=1 || bison3=0; \
	 $(TEST) -n "$(SRCINC)" && appsource=true || appsource=false; \
	 if $(SED) -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREV[[:space:]]\).*|\1$${gitrev}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREVFULL[[:space:]]\).*|\1$${fullgitrev}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREMOTE[[:space:]]\).*|\1$${gitremote}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_PREFIX[[:space:]]\).*|\1\"$(PREFIX)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SRCPATH[[:space:]]\).*|\1\"$$PWD\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_APPNAME[[:space:]]\).*|\1\"$(NAME)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_APPRELEASE[[:space:]]\).*|\1\"$(RELEASE_MODE)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SYSNAME[[:space:]]\).*|\1\"$(SYSDEP_SUF)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SYS_\).*|\1$(SYSDEP_SUF)|" \
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
            else $(MV) $(BUILDINC).tmp $(BUILDINC) && echo "$(NAME): $(BUILDINC) updated" \
	    && if $(TEST) "$$javaobj" = "1" || $(TEST) "$$jar" = "1"; then \
	        debug=false;test=false;echo " $(MACROS) " | $(GREP) -q -- ' -D_TEST ' && test=true; echo " $(MACROS) " | $(GREP) -q -- ' -D_DEBUG ' && debug=true; \
	        { $(PRINTF) "public final class Build {\n" && \
	        $(SED) -n -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\(BUILD_GIT[^[:space:]]*\)[[:space:]][[:space:]]*\(.*\).*|    public static final String  \1 = \2;|p' \
	                  -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\([^[:space:]]*\)[[:space:]][[:space:]]*\(".*"\).*|    public static final String  \1 = \2;|p' \
	                  -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\([^[:space:]]*\)[[:space:]][[:space:]]*\([^[:space:]]*\).*|    public static final int     \1 = \2;|p' \
	                   $(VERSIONINC) $(BUILDINC) \
	        && $(PRINTF) "    public static final String  BUILD_SYS = \"$(UNAME_SYS)\";\n" \
	        && $(PRINTF) "    public static final boolean BUILD_DEBUG = $$debug;\n" \
	        && $(PRINTF) "    public static final boolean BUILD_TEST = $$test;\n" \
		&& $(PRINTF) "    public static final String  BUILD_DATE = \"`date '+%Y-%m-%d %H:%M:%S %Z'`\";\n" \
	        && $(PRINTF) "    public static final boolean APP_INCLUDE_SOURCE = $$appsource;\n}\n"; \
	        } > $(BUILDINCJAVA); \
	       fi; \
	    fi; \
	 fi

.gitignore:
	@{ cat .gitignore $(NO_STDERR); \
	   for f in $(BIN) $(BIN).dSYM $(LIB) $(JAR) $(GENSRC) $(GENJAVA) $(GENINC) $(SRCINC) \
	            $(BUILDINC) $(BUILDINCJAVA) $(CLANGCOMPLETE) \
	            `$(TEST) -L $(FLEXLEXER_LNK) && echo $(FLEXLEXER_LNK) || true` \
	            '*.o' '*.d' '*.class' '*.a' '*~' '.*.swo' '.*.swp' 'valgrind_*.log'; do \
	       $(PRINTF) "$$f\n" | $(SED) -e 's|^./||'; done; \
	 } | $(SORT) | $(UNIQ) > .gitignore

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
	     file=make-fallback; target="`dirname $${makefile}`/$${file}"; if $(TEST) -e "$$file" -a -e "$$target"; then \
	         $(GREP) -E -i -B10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$$target" > "$${target}.tmp" \
	         && $(GREP) -E -i -A10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$$file" | tail -n +2 >> "$${target}.tmp" \
	         && mv "$${target}.tmp" "$${target}" && echo "merged $${target}" && chmod +x "$$target" || echo "! cannot merge $${target}" && $(RM) -f "$${target}.tmp"; \
	     fi; \
	 done

#to generate makefile displaying shell command beeing run
debug-makefile:
	@sed -e 's/^\(cmd_[[:space:]0-9a-zA-Z_]*\)=/\1= ls $(NAME)\/\1 || time /' Makefile > Makefile.debug
	@$(MAKE) -f Makefile.debug

# Run Valgrind filter output
valgrind: all
	@logfile=`$(MKTEMP) ./valgrind_XXXXXX` && $(MV) "$${logfile}" "$${logfile}.log"; logfile="$${logfile}.log"; \
	 $(VALGRIND) --leak-check=full --log-file="$${logfile}" $(VALGRIND_RUN_PROGRAM) || true; \
	 $(AWK) '/[0-9]+[[:space:]]+bytes[[:space:]]+/ { block=1; blockignore=0; blockstr=$$0; } \
	         //{ \
	             if (block) { \
	                 blockstr=blockstr "\n" $$0; \
	                 if (/$(VALGRIND_MEM_IGNORE_PATTERN)/) blockignore=1; \
	             } else { print $$0; } \
	         } \
	         /=+[0-9]+=+[[:space:]]*$$/ { \
	             if (block) { \
	                 if (!blockignore) print blockstr; \
	                 block=0; \
	             } \
	         } \
	         ' $${logfile} > $${logfile%.log}_filtered.log && cat $${logfile%.log}_filtered.log \
	 && echo "* valgrind output in $${logfile} and $${logfile%.log}_filtered.log (will be deleted by 'make distclean')"

info:
	@echo "NAME             : $(NAME)"
	@echo "UNAME_SYS        : $(UNAME_SYS)  [`uname -a`]"
	@#echo "UNAME_ARCH       : $(UNAME_ARCH)"
	@echo "MAKE             : $(MAKE)  [`$(MAKE) --version $(NO_STDERR) | $(HEADN1) || $(MAKE) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "SHELL            : $(SHELL)"
	@echo "FIND             : $(FIND)  [`$(FIND) --version $(NO_STDERR) | $(HEADN1) || $(FIND) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "AWK              : $(AWK)  [`$(AWK) --version $(NO_STDERR) | $(HEADN1) || $(AWK) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "GREP             : $(GREP)  [`$(GREP) --version $(NO_STDERR) | $(HEADN1) || $(GREP) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "SED              : $(SED)  [`$(SED) --version $(NO_STDERR) | $(HEADN1) || $(SED) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "TAR              : $(TAR)  [`$(TAR) --version $(NO_STDERR) | $(HEADN1) || $(TAR) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "DATE             : $(DATE)  [`$(DATE) --version $(NO_STDERR) | $(HEADN1) || $(DATE) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "PKGCONFIG        : $(PKGCONFIG)"
	@echo "CC               : $(CC)  [`$(CC) --version $(NO_STDERR) | $(HEADN1)`]"
	@echo "CXX              : $(CXX)  [`$(CXX) --version $(NO_STDERR) | $(HEADN1)`]"
	@echo "OBJC             : $(OBJC)"
	@echo "GCJ              : $(GCJ)  [`$(GCJ) --version $(NO_STDERR) | $(HEADN1)`]"
	@echo "GCJH             : $(GCJH)"
	@echo "CPP              : $(CPP)"
	@echo "CCLD             : $(CCLD)"
	@echo "YACC             : $(YACC)  [`$(YACC) --version $(NO_STDERR) | $(HEADN1) || $(YACC) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "BISON3           : $(BISON3)  [`$(BISON3) --version $(NO_STDERR) | $(HEADN1) || $(BISON3) -V $(NO_STDERR) | $(HEADN1)`]"
	@echo "LEX              : $(LEX)  [`$(LEX) --version $(NO_STDERR) | $(HEADN1) || $(LEX) -V $(NO_STDERR) | $(HEADN1)`]"
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
rinfo: info
	old="$$PWD"; for d in $(SUBDIRS); do cd "$$d" && $(MAKE) rinfo; cd "$$old"; done

.PHONY: subdirs $(SUBDIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(TESTDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: subdirs $(DISTCLEANDIRS)
.PHONY: subdirs $(DEBUGDIRS)
.PHONY: subdirs $(DOCDIRS)
.PHONY: default_rule all build_all cleanme clean distclean dist test info rinfo \
	doc installme install debug gentags update-$(BUILDINC) create-$(BUILDINC) \
	.gitignore merge-makefile debug-makefile valgrind

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
