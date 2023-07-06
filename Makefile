#
# Copyright (C) 2017-2020 Vincent Sallaberry
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
# RESERVED for internal use: ./obj/ ./build.h, ./version.h ./Makefile ./Build.java $(BUILDDIR)/_src_.c
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

# FOREIGN_MAIN: if the main() is not a C/OBJC/C++ function (eg: Java or ada),
#  specify the path of _source_ file containing ADA or JAVA main, eg: FOREIGN_MAIN=src/Main.java
#  For Java, this file (class) must contain 'public static void main(String[] args)'
#  For Ada, file with single Procedure outside package, without arguments or pragma export.
# When FOREIGN_MAIN is not empty, the MACRO BUILD_FOREIGN_MAIN is ON in build.h.
#FOREIGN_MAIN	= adamain.adb
#FOREIGN_MAIN	= JMain.java
FOREIGN_MAIN	?=

# DISTDIR: where the dist packages zip/tar.xz are saved
DISTDIR		= ../../dist

# PREFIX: where the application is to be installed
PREFIX		= /usr/local
INSTALL_FILES	= $(LIB) include

# CONFIG_CHECK	= all zlib ncurses libcrypto applecrypto openssl sigqueue sigrtmin
#	          libcrypt crypt.h crypt_gnu crypt_des_ext libintl
# If a feature is prefixed with '+' (eg: +openssl), this makes it MANDATORY
# and make will fail if the feature is not available.
CONFIG_CHECK	= zlib ncurses

# Project specific Flags (system specific flags are set in $(sys_{LIBS,WARN,INCS,OPTI,DEBUG})
# if you set LIBS_<system>, or similar. They are added here to make you control the order of arguments).
# Choice between <flag>_RELEASE/_DEBUG/_TEST is done according to BUILDINC / make debug / make test
WARN_RELEASE	= -Wall -W -pedantic $(sys_WARN)
ARCH_RELEASE	= -march=native
OPTI_COMMON	= -pipe -fstack-protector
OPTI_RELEASE	= -O3 $(OPTI_COMMON) $(sys_OPTI)
INCS_RELEASE	= $(sys_INCS)
LIBS_RELEASE	= $(SUBLIBS) $(sys_LIBS) $(CONFIG_CURSES) $(CONFIG_ZLIB) -lpthread
MACROS_RELEASE	= -DNDEBUG
WARN_DEBUG	= $(WARN_RELEASE)
ARCH_DEBUG	= $(ARCH_RELEASE)
OPTI_DEBUG	= -O0 -g $(OPTI_COMMON)
INCS_DEBUG	= $(INCS_RELEASE)
LIBS_DEBUG	= $(LIBS_RELEASE)
MACROS_DEBUG	= -D_DEBUG -D_TEST
# FLAGS_<lang> is global for one language (<lang>: C,CXX,OBJC,GCJ,GCJH,OBJCXX,LEX,YACC,ADA).
FLAGS_C		= -std=c99 -D_GNU_SOURCE
FLAGS_CXX	= -D_GNU_SOURCE -Wno-variadic-macros
FLAGS_OBJC	= -std=c99
FLAGS_OBJCXX	=
FLAGS_GCJ	=
FLAGS_ADA	=
# Some other flags: ARCH(-arch i386 -arch x86_64), WARN(-Werror), OPTI(-gdwarf -g3), ...

# FLAGS_<lang>_<file> is specific to one file (eg:'FLAGS_CXX_Big.cc=-O0','FLAGS_C_src/a.c=-O1')

# System specific flags (WARN_$(sys),OPTI_$(sys),DEBUG_$(sys),LIBS_$(sys),INCS_$(sys))
# $(sys) is lowcase(`uname`), eg: 'LIBS_darwin=-framework IOKit -framework Foundation'
#  + For clang++ on darwin, use libstdc++ to have gnu extension __gnu_cxx::stdio_filebuf
#  + Comment '*_GNUCXX_XTRA_* = *' lines to use default libc++ and use '#ifdef __GLIBCXX__' in your code.
# FLAGS_GNUCXX_XTRA_darwin_/usr/bin/clangpppp=-stdlib=libstdc++
# LIBS_GNUCXX_XTRA_darwin_/usr/bin/clangpppp=-stdlib=libstdc++
FLAGS_CXX	+= $(FLAGS_GNUCXX_XTRA_$(UNAME_SYS)_$(CXX:++=pppp))
FLAGS_OBJCXX	+= $(FLAGS_GNUCXX_XTRA_$(UNAME_SYS)_$(CXX:++=pppp))
LIBS_darwin	= -framework IOKit -framework Foundation $(LIBS_GNUCXX_XTRA_$(UNAME_SYS)_$(CXX:++=pppp))
LIBS_linux	= -lrt

# TESTS and DEBUG parameters
# VALGRIND_RUN: how to run the program with valgrind (can be used to pass arguments to valgrind)
#   (eg: './$(BIN) arguments', '--trace-children=no ./$(BIN) arguments')
VALGRIND_RUN	= ./$(BIN)
# VALGRIND_MEM_IGNORE_PATTERN: awk regexp to ignore keyworks in LEAKS reports (sure valgrind --suppressions=<file> is better)
#VALGRIND_MEM_IGNORE_PATTERN = ImageLoader::recursiveInitialization|ImageLoaderMachO::doInitialization|ImageLoaderMachO::instantiateFromFile|_objc_init|_NSInitializePlatform
VALGRIND_MEM_IGNORE_PATTERN =
# CHECK_RUN: what to run with 'make check' (eg: 'true', './test.sh $(BIN)', './$(BIN) --test'
#   if tests are only built with macro _TEST, you can insert 'make debug' or 'make test'
CHECK_RUN	= echo "vlib tests are in https://github.com/vsallaberry/vsensorsdemo"

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
CAT		= cat
LS		= ls
CP		= cp
MV		= mv
TR		= tr
GIT		= git
DIFF		= diff
UNIQ		= uniq
OD		= od
GZIP		= gzip
INSTALL		= install -m 0644
INSTALLBIN	= install -m 0755
INSTALLDIR	= install -d -m 0755
VALGRIND	= valgrind
VALGRIND_ARGS	= --leak-check=full --track-origins=yes --show-leak-kinds=all -v
MKTEMP		= mktemp
BASENAME	= basename
DIRNAME		= dirname
NO_STDERR	= 2> /dev/null
NO_STDOUT	= > /dev/null
STDOUT_TO_ERR	= 1>&2
STDERR_TO_OUT	= 2>&1
DASH		= \#

############################################################################################
# Make command-line variables and recursion
# All make are different regarding propagation of variables to sub-makes.
# + gnu make (osx,3.81) does not propagate OPTI but propagates CC (because ?= or CC=$(shell which $CC) ?)
#   -> possibility to force propagation with 'MAKEFLAGS+= $(MAKEOVERRIDES)'
# + gnu make (4.2.1) propagates all command-line variables to sub-makes.
#   -> possibility to disable propagation with 'gmake MAKEFLAGS='
# + bsdmake and bmake allways propagate command-line variables
#   -> bsdmake: possibility to disable propagation with '.MAKEFLAGS=' in Makefile
#   -> bmake  : possibility to disable propagation with 'bmake .MAKEOVERRIDES='
MAKEFLAGS		?=
MAKEFLAGS 		+= $(MAKEOVERRIDES)
.MAKEFLAGS$(MAKEFLAGS)	=
.MAKEOVERRIDES$(MAKEFLAGS)=

############################################################################################
# About shell commands execution in this Makefile:
# - ALWAYS use braces with shell variables ! (${<var>})
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
CONFIGLOG	= config.log
CONFIGMAKE	= config.make
CONFIGINC	= config.h
ALLMAKEFILES	:= Makefile $(CONFIGMAKE)
# SRCINC containing source code is included if APP_INCLUDE_SOURCE is defined in VERSIONINC.
SRCINCDIR	= $(BUILDDIR)
SRCINC_STR	= $(SRCINCDIR)/_src_.c
SRCINC_Z	= $(SRCINCDIR)/_src_.z.c
cmd_NAME_FIXED	= echo "$(NAME)" | $(SED) -e "s|[$(DASH)\"%+*/:=;,.'[:space:]-]|_|g"

# Get Debug/Test mode in build.h
cmd_RELEASEMODE = $(SED) -n -e 's/^[[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*BUILD_APPRELEASE[[:space:]]*"\([^"]*\).*/\1/p' \
       		     $(BUILDINC) $(NO_STDERR) || echo RELEASE
tmp_RELEASEMODE	!= $(cmd_RELEASEMODE)
tmp_RELEASEMODE	?= $(shell $(cmd_RELEASEMODE))
RELEASE_MODE	:= $(tmp_RELEASEMODE)

# EXPERIMENTAL: Common commands to handle the bsd make .OBJDIR feature which
# puts all outputs in ./obj if existing.
# As this is for BSD make, we can use specific BSD variable replacement ($x:S/...)
#test whether ouputs are in other folder
cmd_TESTBSDOBJ	= $(TEST) "$(.OBJDIR)" != "$(.CURDIR)"
cmd_FINDBSDOBJ	= $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true
#keep quote-safe version of CURDIR and OBJDIR
tmp_CURDIR	:= $(.CURDIR:Q)
tmp_OBJDIR	:= $(.OBJDIR:Q)
#from OBJDIR, remove heading CURDIR to have relative path
RELOBJDIR	= $(tmp_OBJDIR:S/$(tmp_CURDIR)\///)
#save SUBLIBS
OLDSUBLIBS	:= $(SUBLIBS)
#add CURDIR to SUBLIBS (to have absolute path)
SUBLIBS         := $(SUBLIBS:S/^/$(tmp_CURDIR)\//)
SUBLIBS         := $(SUBLIBS:S/$(tmp_CURDIR)\/$//)
#from SUBLIBS, remove heading OBJDIR (if matching, CURDIR=OBJDIR)
SUBLIBS         := $(SUBLIBS:S/^$(tmp_OBJDIR)\///)
#restore updates on SUBLIBS if OBJDIR is not set
SUBLIBS$(.OBJDIR):= $(OLDSUBLIBS)

# TEST target is RELEASE with _TEST macro set
WARN_TEST	= $(WARN_RELEASE)
OPTI_TEST	= $(OPTI_RELEASE)
ARCH_TEST	= $(ARCH_RELEASE)
INCS_TEST	= $(INCS_RELEASE)
LIBS_TEST	= $(LIBS_RELEASE)
MACROS_TEST	?= $(MACROS_RELEASE) -D_TEST

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
		      ver="$$($${bin} -V 2>&1 | $(AWK) -F '.' '/[Bb][iI][sS][oO][nN].*[0-9]+(\.[0-9]+)+/ { \
		                                               $$0=substr($$0,match($$0,/[0-9]+(\.[0-9]+)+/)); \
		                                               print $$1*1000000 + $$2*1000 + $$3*1 }')"; \
		      $(TEST) -n "$${ver}" && $(TEST) $${ver} -ge 03000000 $(NO_STDERR) && found="$${bin}._have_bison3_" && break; \
		  done; $(TEST) -n "$${found}" && $(PRINTF) "$${found}" || $(WHICH) $(YACC) bison yacc $(NO_STDERR) | $(HEADN1) || true
tmp_YACC0	!= $(cmd_YACC)
tmp_YACC0	?= $(shell $(cmd_YACC))
tmp_YACC	:= $(tmp_YACC0)
BISON3		:= $(tmp_YACC)
tmp_YACC	:= $(tmp_YACC:._have_bison3_=)
BISON3$(tmp_YACC)._have_bison3_ := $(tmp_YACC)
BISON3		:= $(BISON3$(BISON3))
YACC		:= $(tmp_YACC)

cmd_BISON_VERSION	= { "$(BISON3)" -V || "$(YACC)" -V; } $(STDERR_TO_OUT) \
			  | $(SED) -ne 's/^[^.0-9]*\([0-9]\{1,3\}\.[.0-9]*\).*/\1/p' \
			  | $(AWK) -F '.' '{ print $$1*1000000 + $$2*1000 + $$3*1 }'
tmp_BISON_VERSION	!= $(cmd_BISON_VERSION)
tmp_BISON_VERSION	?= $(shell $(cmd_BISON_VERSION))
BISON_VERSION		:= $(tmp_BISON_VERSION)

# Search flex, lex, and find the location of corresponding FlexLexer.h needed by C++ Scanners.
# Depending on gcc include search paths, the wrong FlexLexer.h could be chosen if you have
# several flex on your system -> create link to correct FlexLexer.h.
# Particular case on MacOS where flex is a wrapper to xcode, meaning
# $(dirname flex)/../include/FlexLexer.h does not exist.
FLEXLEXER_INC	= FlexLexer.h
FLEXLEXER_LNK	= $(BUILDDIR)/$(FLEXLEXER_INC)
$(FLEXLEXER_LNK):
cmd_LEX		= lex=`$(WHICH) $(LEX) flex lex $(NO_STDERR) | $(HEADN1)`; \
		  $(TEST) -n "$${lex}" -a \( ! -e "$(FLEXLEXER_LNK)" -o -L "$(FLEXLEXER_LNK)" \) \
		  && flexinc="`dirname $${lex}`/../include/$(FLEXLEXER_INC)" \
		  && $(TEST) -e "$${flexinc}" \
		  || { $(TEST) "$(UNAME_SYS)" = "darwin" \
		       && otool -L "$${lex}" | $(GREP) -Eq 'libxcselect[^ ]*dylib' $(NO_STDERR) \
		       && xcodepath="`xcode-select -p $(NO_STDERR)`" \
		       && for flexinc in "$${xcodepath}/usr/include/$(FLEXLEXER_INC)" "$${xcodepath}/Toolchains/Xcodedefault.xctoolchain/usr/include/$(FLEXLEXER_INC)"; do \
		           $(TEST) -e "$${flexinc}" && break; done; } \
		  && ! $(TEST) "$(FLEXLEXER_LNK)" -ef "$${flexinc}" \
		       && $(PRINTF) -- "$(NAME): create link $(FLEXLEXER_LNK) -> $${flexinc}\n" $(STDOUT_TO_ERR) \
		  && ln -sf "$${flexinc}" "$(FLEXLEXER_LNK)" $(NO_STDERR) && $(TEST) -e $(BUILDINC) && $(TOUCH) $(BUILDINC); \
		  echo "$${lex}"
tmp_LEX		!= $(cmd_LEX)
tmp_LEX		?= $(shell $(cmd_LEX))
LEX		:= $(tmp_LEX)

# Search gcj compiler.
cmd_GCJ		= $(WHICH) ${GCJ} gcj gcj-6 gcj-5 gcj-mp gcj-mp-6 gcj-mp-5 gcj-mp-4.9 gcj-mp-4.8 gcj-mp-4.7 gcj-mp-4.6 $(NO_STDERR) | $(HEADN1)
tmp_GCJ		!= $(cmd_GCJ)
tmp_GCJ		?= $(shell $(cmd_GCJ))
GCJ		:= $(tmp_GCJ)

# Search gnat compiler.
cmd_GNAT	= $(WHICH) ${GNAT} gnat gnat-6 gnat-5 gnat-4 $(NO_STDERR) | $(HEADN1)
tmp_GNAT	!= $(cmd_GNAT)
tmp_GNAT	?= $(shell $(cmd_GNAT))
GNAT		:= $(tmp_GNAT)
GNATC		:= $(GNAT) compile
GNATBIND	:= $(GNAT) bind
GNATMAKE	:= $(GNAT) make

############################################################################################
# Scan for sources
############################################################################################
# Common find pattern to include files in SRCDIR/sysdeps ONLY if suffixed with system name,
# or the one suffixed with 'default' if not found.
find_AND_SYSDEP = -and \( \! -path '$(SRCDIR)/$(SYSDEPDIR)/*' \
		    -or -path '$(SRCDIR)/$(SYSDEPDIR)/*$(SYSDEP_SUF).*' \
		    -or \( -path '$(SRCDIR)/$(SYSDEPDIR)/*$(SYSDEP_SUF_DEF).*' \
		           -and \! \( -exec $(SHELL) -c "$(LS) \"\$$(echo \"{}\" \
		           | $(SED) -n -e 's|$(SYSDEP_SUF_DEF)\(\.[^.]*\)$$|$(SYSDEP_SUF)|p')\".* \
		                   $(NO_STDERR) | $(GREP) -E -v -q '\.[aod]$$' $(NO_STDERR)" \; \) \) \
		  \) -and \! -path '$(SRCDIR)/$(RELOBJDIR)/*'

# Search Meta sources (used to generate sources)
# For yacc/bison and lex/flex:
#   - the basename of meta sources must be always different (have one grammar calc.y and one
#     lexer calc.l is not supported: prefer parse-calc.y and scan-calc.l).
#   - c++ source is generated with .ll and .yy, c source with .l and .y, java with .yyj
#   - .l,.ll included if LEX is found, .y,.yy included if YACC is found, .yyj included
#     if BISON3 AND ((GCJ and BIN are defined) OR (JAR defined)).
#   - yacc generates by default headers for lexer, therefore lexer files depends on parser files.
cmd_YACCSRC	= $(cmd_FINDBSDOBJ); \
		  $(TEST) -n "$(YACC)" && $(FIND) $(SRCDIR) \( -name '*.y' -or -name '*.yy' \) \
		                            $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_LEXSRC	= $(cmd_FINDBSDOBJ); \
		  $(TEST) -n "$(LEX)" && $(FIND) $(SRCDIR) \( -name '*.l' -or -name '*.ll' \) \
		                           $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||' || true
cmd_YACCJAVA	= $(cmd_FINDBSDOBJ); \
		  $(TEST) \( \( -n "$(BIN)" -a -n "$(GCJ)" \) -o -n "$(JAR)" \) -a  -n "$(BISON3)" \
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
cmd_FIND_NOGEN	= $(cmd_FINDBSDOBJ); \
		  echo $(GENSRC) $(GENINC) $(GENJAVA) \
		       "$$($(FIND) $(SRCDIR) -mindepth 2 -name 'Makefile' \
		           | $(SED) -e 's|\([^[:space:]]*\)/[^[:space:]]*|\1/*|g')" \
		  | $(SED) -e 's|\([^[:space:]]*\)|-and \! -path "\1" -and \! -path "./\1"|g' || true
tmp_FIND_NOGEN	!= $(cmd_FIND_NOGEN)
tmp_FIND_NOGEN	?= $(shell $(cmd_FIND_NOGEN))
find_AND_NOGEN	:= $(tmp_FIND_NOGEN)

# Search non-generated sources and headers. Extensions must be in low-case.
# Include java only if a JAR is defined as output or if BIN and GCJ are defined.
cmd_JAVASRC	= $(cmd_FINDBSDOBJ); \
		  $(TEST) \( -n "$(BIN)" -a -n "$(GCJ)" \) -o -n "$(JAR)" \
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
# JABAOBJ the .o file containing Java code
JAVAOBJNAME	:= _Java_.o
JAVAOBJ$(BUILDDIR) := $(BUILDDIR)/$(JAVAOBJNAME)
JAVAOBJ.	:= $(JAVAOBJNAME)
JAVAOBJ		:= $(JAVAOBJ$(BUILDDIR))
TMPCLASSESDIR	= $(BUILDDIR)/.tmp_classes
# ADAOBJNAME the .o file containing ada stub
ADAOBJNAME	:= _ada-stub_.o
ADAOBJ$(BUILDDIR):= $(BUILDDIR)/$(ADAOBJNAME)
ADAOBJ.		:= $(ADAOBJNAME)
ADAOBJ		:= $(ADAOBJ$(BUILDDIR))

# Search non-generated ADA sources and headers. Extensions must be in low-case.
# Include ada only if GNAT is found. In FOREIGN_MAIN is ada, put it at head
# of list (needed by gnatbind).
# Ada ads files without adb body are considered as sources files, and must be compiled.
cmd_ADASRC	= $(cmd_FINDBSDOBJ); \
		  $(TEST) -x "$(GNAT)" \
		  && { $(TEST) -e '$(FOREIGN_MAIN)' -a '$(FOREIGN_MAIN:.adb=).adb' = '$(FOREIGN_MAIN)' && $(PRINTF) -- '$(FOREIGN_MAIN) ' || true; \
		  $(FIND) $(SRCDIR) \! -name '$(FOREIGN_MAIN)' -and \( -name '*.adb' \
		           -or \( -name '*.ads' -and -exec "$(SHELL)" -c 'f={}; f=$${f%.ads}.adb; ! $(TEST) -e "$${f}"' \; \) \) \
		           -and \! -path '$(ADAOBJ:.o=.adb)' -and \! -path './$(ADAOBJ:.o=.adb)' \
		           $(find_AND_NOGEN) $(find_AND_SYSDEP) \
			   -print $(NO_STDERR) | $(SED) -e 's|^\./||'; } || true
# JAVASRC variable, filled from the 'find' command (cmd_JAVA) defined above.
tmp_ADASRC	!= $(cmd_ADASRC)
tmp_ADASRC	?= $(shell $(cmd_ADASRC))
tmp_ADASRC	:= $(tmp_ADASRC)
ADASRC		:= $(tmp_ADASRC) $(GENADA)
tmp_ADAALI	:= $(ADASRC:.adb=.ali)
ADAALI		:= $(tmp_ADAALI:.ads=.ali)

# Add Java CNI headers to include search exclusion.
cmd_FIND_NOGEN2	= echo $(JCNIINC) | $(SED) -e 's|\([^[:space:]]*\)|-and \! -path "\1" -and \! -path "./\1"|g'
tmp_FIND_NOGEN2	!= $(cmd_FIND_NOGEN2)
tmp_FIND_NOGEN2	?= $(shell $(cmd_FIND_NOGEN2))
find_AND_NOGEN2	:= $(tmp_FIND_NOGEN2)
# Other non-generated sources and headers. Extension must be in low-case.
cmd_SRC		= $(cmd_FINDBSDOBJ); \
		  $(FIND) $(SRCDIR) \( -name '*.c' -or -name '*.cc' -or -name '*.cpp' -or -name '*.m' -or -name '*.mm' \) \
 		    $(find_AND_NOGEN) -and \! -path '$(SRCINC_Z)' -and \! -path './$(SRCINC_Z)' \
		                      -and \! -path '$(SRCINC_STR)' -and \! -path './$(SRCINC_STR)' \
		    $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||'
cmd_INCLUDES	= $(cmd_FINDBSDOBJ); \
		  $(FIND) $(INCDIRS) $(SRCDIR) \( -name '*.h' -or -name '*.hh' -or -name '*.hpp' \
		                                  -or -name '*.ads' \) \
		    $(find_AND_NOGEN) $(find_AND_NOGEN2) \
		    -and \! -path $(VERSIONINC) -and \! -path ./$(VERSIONINC) \
		    -and \! \( -path $(FLEXLEXER_LNK) -and -type l \) \
		    -and \! -path $(BUILDINC) -and \! -path ./$(BUILDINC) \
		    -and \! -path $(CONFIGINC) -and \! -path ./$(CONFIGINC) \
		    -and \! -path '$(ADAOBJ:.o=.ads)' -and \! -path './$(ADAOBJ:.o=.ads)' \
		    $(find_AND_SYSDEP) -print $(NO_STDERR) | $(SED) -e 's|^\./||'

# INCLUDE VARIABLE, filled from the 'find' command (cmd_INCLUDES) defined above.
tmp_INCLUDES	!= $(cmd_INCLUDES)
tmp_INCLUDES	?= $(shell $(cmd_INCLUDES))
tmp_INCLUDES	:= $(tmp_INCLUDES)
INCLUDES	:= $(VERSIONINC) $(BUILDINC) $(CONFIGINC) $(tmp_INCLUDES)

# SRC variable, filled from the 'find' command (cmd_SRC) defined above.
tmp_SRC		!= $(cmd_SRC)
tmp_SRC		?= $(shell $(cmd_SRC))
tmp_SRC		:= $(tmp_SRC) $(ADASRC)
SRC		:= $(tmp_SRC) $(GENSRC)

# OBJ variable computed from SRC, replacing SRCDIR by BUILDDIR and extension by .o
# Add _Java_.o if BIN, GCJ and JAVASRC are defined.
# Add _ada-stub_.o if GNAT is defined, and if there are ada sources.
tmp_OBJ1	:= $(SRC:.m=.o)
tmp_OBJ2	:= $(tmp_OBJ1:.mm=.o)
tmp_OBJ3	:= $(tmp_OBJ2:.cpp=.o)
tmp_OBJ4	:= $(tmp_OBJ3:.cc=.o)
tmp_OBJ5	:= $(tmp_OBJ4:.adb=.o)
tmp_OBJ6	:= $(tmp_OBJ5:.ads=.o)
OBJ_NOJAVA	:= $(tmp_OBJ6:.c=.o)
cmd_SRC_BUILD	:= echo " $(OBJ_NOJAVA)" | $(SED) -e 's| $(SRCDIR)/| $(BUILDDIR)/|g'; \
		   case " $(JAVASRC) " in *" "*".java "*) $(TEST) -n "$(GCJ)" -a -n "$(BIN)" && echo "$(JAVAOBJ)";; esac; \
		   case " $(SRC) " in *" "*".adb "*|*" "*".ads "*) $(TEST) -x "$(GNAT)" && echo "$(ADAOBJ)";; esac
tmp_SRC_BUILD	!= $(cmd_SRC_BUILD)
tmp_SRC_BUILD	?= $(shell $(cmd_SRC_BUILD))
OBJ		:= $(tmp_SRC_BUILD)

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

# CCLD: use $(GCJ) if _Java_.o, use $(CXX) if .cc,.cpp,.mm files, otherwise use $(CC).
cmd_CCLD	= case " $(OBJ) $(SRC) " in *" $(JAVAOBJ) "*) echo "$(GCJ)";; \
		                            *" "*".cpp "*|*" "*".cc "*|*" "*".mm "*) echo $(CXX);; *) echo $(CC) ;; esac
tmp_CCLD	!= $(cmd_CCLD)
tmp_CCLD	?= $(shell $(cmd_CCLD))
CCLD		:= $(tmp_CCLD)

CPP		= $(CC) -E
OBJC		= $(CC)
OBJCXX		= $(CXX)

cmd_JAVAC	= $(WHICH) $(JAVA_HOME)/*/javac javac $(NO_STDERR) | $(HEADN1)
tmp_JAVAC	!= $(cmd_JAVAC)
tmp_JAVAC	?= $(shell $(cmd_JAVAC))
JAVAC		:= $(tmp_JAVAC)

JAVA		= $(JAVAC:javac=java)
JARBIN		= $(JAVAC:javac=jar)
JAVAH		= $(JAVAC:javac=javah)
JAVA_HOME	= $(JAVAC:javac=..)

############################################################################################

sys_LIBS	= $(LIBS_$(SYSDEP_SUF))
sys_INCS	= $(INCS_$(SYSDEP_SUF))
sys_OPTI	= $(OPTI_$(SYSDEP_SUF))
sys_WARN	= $(WARN_$(SYSDEP_SUF))
sys_DEBUG	= $(DEBUG_$(SYSDEP_SUF))

############################################################################################
# Generic Build Flags
cmd_CPPFLAGS	= srcpref=; srcdirs=; $(cmd_TESTBSDOBJ) && srcpref="$(.CURDIR:Q)/" && srcdirs="$${srcpref} $${srcpref}$(SRCDIR)"; \
		  sep=; incpref=; incs=; for dir in . $(SRCDIR) $(BUILDDIR) $${srcdirs} : $(INCDIRS); do \
                      test -z "$${sep}" -a -n "$${incs}" && sep=" " || true; \
		      test "$${dir}" = ":" && incpref="$${srcpref}" && continue || true; \
		      case " $${incs} " in *" -I$${incpref}$${dir} "*) ;; *) incs="$${incs}$${sep}-I$${incpref}$${dir}";; esac; \
		  done; echo "$${incs}"
tmp_CPPFLAGS	!= $(cmd_CPPFLAGS)
tmp_CPPFLAGS	?= $(shell $(cmd_CPPFLAGS))
tmp_CPPFLAGS	:= $(tmp_CPPFLAGS)
CPPFLAGS	:= $(tmp_CPPFLAGS) $(INCS) $(MACROS) -DHAVE_VERSION_H
GCJCPPFLAGS	:= $(tmp_CPPFLAGS)
FLAGS_COMMON	= $(OPTI) $(WARN) $(ARCH)
CFLAGS		= -MMD $(FLAGS_C) $(FLAGS_COMMON)
CXXFLAGS	= -MMD $(FLAGS_CXX) $(FLAGS_COMMON)
OBJCFLAGS	= -MMD $(FLAGS_OBJC) $(FLAGS_COMMON)
OBJCXXFLAGS	= -MMD $(FLAGS_OBJCXX) $(FLAGS_COMMON)
CLASSPATH	= -classpath "$(BUILDDIR)$$(printf -- ':%s' $(SRCDIR) $(CLASSPATHS))" ## FIXME: used only once in javac or gcj -C, once in update-build.h
JFLAGS		= $(FLAGS_GCJ) $(FLAGS_COMMON) -I$(BUILDDIR) $(CLASSPATH)
JHFLAGS		= -I$(BUILDDIR)
ADAFLAGS	= -MMD $(FLAGS_ADA) $(FLAGS_COMMON)
LIBFORGCJ$(GCJ)	= $(CLASSPATHS) -lstdc++
FOREIGNMAINNOJAVAEXT	:= $(FOREIGN_MAIN:.java=)
FOREIGNMAINJAVAEXT	:= $(FOREIGNMAINNOJAVAEXT).java
MAINFORGCJ$(GCJ)$(FOREIGNMAINJAVAEXT)	:= --main=$(FOREIGNMAINNOJAVAEXT)
LDFLAGS		= $(ARCH) $(OPTI) $(LIBS) $(LIBFORGCJ$(CCLD)) $(MAINFORGCJ$(CCLD)$(FOREIGN_MAIN))
JAVAFLAGS	= $(FLAGS_JAVAC) $(CLASSPATH)
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
# inclusion by a shell command '!=' or '$(shell ...)'.
# Here, if .alldeps does not exit, we include version.h (containing only lines starting with
# dash(#), so that it can be parsed by make and do nothing), and the OBJs will depends on
# $(CONFIG_OBJDEPS_NOJAVA_version.h). In the same time, we create .alldeps.d containing
# inclusion of all .d files, created with default headers dependency (OBJ depends on all
# includes), that will be used on next 'make' and overrided by gcc -MMD.
# Additionnaly, we use this command to populate git submodules if needed.
#
cmd_DEBUGMAKEFILE=false
CONFIG_OBJDEPS_NOJAVA_$(VERSIONINC)	= $(ALLMAKEFILES) $(INCLUDES) $(GENINC)
CONFIG_OBJDEPS_NOJAVA_$(CONFIGMAKE)	= $(CONFIG_OBJDEPS_NOJAVA_$(VERSIONINC))
CONFIG_OBJDEPS_NOJAVA_$(INCLUDEDEPS)	=
DEPS			:= $(OBJ:.o=.d) $(SRCINC_STR:.c=.d) $(SRCINC_Z:.c=.d)
INCLUDEDEPS		:= .alldeps.d
CONFIGMAKE_REC_FILE	:= .configmake-recursion
cmd_SINCLUDEDEPS	= inc=1; ret=true; $(RM) -f '$(CONFIGMAKE_REC_FILE)' $(STDOUT_TO_ERR) || true; \
			  $(cmd_DEBUGMAKEFILE) && { printf -- '$(NAME): SINCLUDEDEPS pwd: '; pwd; } $(STDOUT_TO_ERR) || true; \
			  if $(TEST) -e '$(INCLUDEDEPS)' -a -e '$(CONFIGMAKE)'; then $(PRINTF) -- '$(INCLUDEDEPS)'; \
			  else $(TEST) -e '$(CONFIGMAKE)' && $(PRINTF) -- '$(CONFIGMAKE)' \
			       || { $(cmd_TESTBSDOBJ) && $(TEST) -e '$(.CURDIR)/$(CONFIGMAKE)' \
			            && $(RM) -f '$(.CURDIR)/$(CONFIGMAKE)' || true; $(PRINTF) -- '$(VERSIONINC)'; };\
			       $(TEST) -e "$(VERSIONINC)" || $(TOUCH) "$(VERSIONINC)"; \
			       inc=; $(PRINTF) -- "include $(CONFIGMAKE)\n" > '$(INCLUDEDEPS)'; fi; \
			  for f in $(DEPS:.d=); do \
			      if $(TEST) -n "$${f}" -a \( -z "$${inc}" -o ! -e "$${f}.d" \) ; then \
			          dir=`dirname "$${f}"`; $(TEST) -d "$${dir}" || $(MKDIR) -p "$${dir}" || echo ">> f:$${f} d:$${dir} mkdir" $(STDOUT_TO_ERR); \
			          $(TEST) "$${f}.o" = "$(JAVAOBJ)" && $(PRINTF) '\n' > "$${f}.d" \
			            || $(PRINTF) -- "$${f}.o: $(CONFIG_OBJDEPS_NOJAVA_$(VERSIONINC))\n" > "$${f}.d"; \
			          $(PRINTF) -- "include $${f}.d\n" >> $(INCLUDEDEPS); \
			      fi; \
			  done; \
			  $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; $(TEST) -x "$(GIT)" && for d in $(SUBDIRS); do \
			      if ! $(TEST) -e "$${d}/Makefile" && $(GIT) submodule status "$${d}" $(NO_STDERR) | $(GREP) -Eq "^-.*$${d}"; then \
			          $(GIT) submodule update --init "$${d}" $(STDOUT_TO_ERR) || ret=false; \
			      fi; \
			      done || true; $(cmd_DEBUGMAKEFILE) && { printf -- "SINCLUDEDEPS END ret:$${ret}\n"; } $(STDOUT_TO_ERR) || true; $${ret}

tmp_SINCLUDEDEPS != $(cmd_SINCLUDEDEPS)
tmp_SINCLUDEDEPS ?= $(shell $(cmd_SINCLUDEDEPS))
SINCLUDEDEPS := $(tmp_SINCLUDEDEPS)
include $(SINCLUDEDEPS)

# Following vars <var>_$(SINCLUDEDEPS) wil have value only if CONFIGMAKE has been included
cmd_CONFIGMAKE_INCLUDED		= $(TEST) '$(SINCLUDEDEPS)' != '$(VERSIONINC)'
cmd_CONFIGMAKE_RECURSE		= { $(TEST) -e '$(CONFIGMAKE_REC_FILE)' && ret=true || ret=false; \
				    $(cmd_DEBUGMAKEFILE) && { $(PRINTF) "$(NAME): target:$@ ?:$? rec-done:$${ret}\n"; } || true; $${ret}; }
CONFIG_OBJDEPS_$(VERSIONINC)	:=
CONFIG_OBJDEPS_$(INCLUDEDEPS) 	:= $(ALLMAKEFILES) $(VERSIONINC) $(CONFIGINC) $(BUILDINC)
CONFIG_OBJDEPS_$(CONFIGMAKE) 	:= $(ALLMAKEFILES) $(VERSIONINC) $(CONFIGINC) $(BUILDINC)
CONFIG_OBJDEPS			:= $(CONFIG_OBJDEPS_$(SINCLUDEDEPS))
CONFIG_OBJDEPS_NOJAVA		:= $(CONFIG_OBJDEPS_NOJAVA_$(SINCLUDEDEPS))

LIB$(CONFIG_OBJDEPS):=
BIN$(CONFIG_OBJDEPS):=
JAR$(CONFIG_OBJDEPS):=
MANIFEST$(CONFIG_OBJDEPS):=
JAVAOBJ$(CONFIG_OBJDEPS):=

############################################################################################
# SRCINC containing source code is included if APP_INCLUDE_SOURCE is defined in VERSIONINC.
# SRCINC_Z (compressed) is used if libz,zlib.h,gzip,od are present, otherwise SRCINC_STR is used.
# TODO: removing heading './' (| $(SED) -e 's|^\./||') causes issues with bsd make
cmd_HAVEVLIB	= case " $(INCLUDES) " in *"include/vlib/avltree.h "*) true ;; *) false ;; esac
cmd_HAVEZLIBH	= $(TEST) '$(CONFIG_ZLIB_H_FOUND)' = "1"
cmd_HAVEZLIB	= $(TEST) '$(CONFIG_ZLIB_FOUND)' = "1"
cmd_SRCINC	= $(cmd_FINDBSDOBJ); if $(TEST) "$(SINCLUDEDEPS)" != "$(VERSIONINC)"; then \
		    { ! $(TEST) -e $(VERSIONINC) \
		      || $(GREP) -Eq '^[[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*APP_INCLUDE_SOURCE([[:space:]]|$$)' \
		               $(VERSIONINC) $(NO_STDERR); } \
		    && { $(cmd_HAVEZLIBH) && $(cmd_HAVEZLIB) \
		              && $(TEST) -x "`$(WHICH) \"$(OD)\" | $(HEADN1) $(NO_STDERR)`" \
		                      -a -x "`$(WHICH) \"$(GZIP)\" | $(HEADN1) $(NO_STDERR)`" \
		              && echo $(SRCINC_Z) || echo $(SRCINC_STR); } || true; fi
tmp_SRCINC	!= $(cmd_SRCINC)
tmp_SRCINC	?= $(shell $(cmd_SRCINC))
SRCINC		:= $(tmp_SRCINC)

SRC		:= $(SRCINC) $(SRC)
OBJ		:= $(SRCINC:.c=.o) $(OBJ)
OBJ_NOJAVA	:= $(SRCINC:.c=.o) $(OBJ_NOJAVA)
############################################################################################

LICENSE		= LICENSE
README		= README.md
CLANGCOMPLETE	= .clang_complete
VALGRINDSUPP	= .valgrind.supp
SRCINC_CONTENT	= $(LICENSE) $(README) $(METASRC) $(tmp_SRC) $(tmp_JAVASRC) $(INCLUDES) $(ALLMAKEFILES)

############################################################################################
# For make recursion through sub-directories
TEST_SUBDIRS	?= $(SUBDIRS)
BUILDDIRS	= $(SUBDIRS:=-build)
INSTALLDIRS	= $(SUBDIRS:=-install)
DISTCLEANDIRS	= $(SUBDIRS:=-distclean)
CLEANDIRS	= $(SUBDIRS:=-clean)
CHECKDIRS	= $(SUBDIRS:=-check)
DEBUGDIRS	= $(TEST_SUBDIRS:=-debug)
DOCDIRS		= $(SUBDIRS:=-doc)
TESTDIRS	= $(TEST_SUBDIRS:=-test)
CONFIGUREDIRS	= $(SUBDIRS:=-configure)

# RECURSEMAKEARGS, see doc for SUBMODROOTDIR above. When SUBMODROOTDIR is not empty,
# if the submodule is fetched alone, it will use its own submodules, if it is fetched as a
# submodule, it will use the root submodule directory, redefined when recursing in SUBDIRS.
RECURSEMAKEARGS	= $(TEST) -n "$(SUBMODROOTDIR)" && recargs="SUBMODROOTDIR=\"`echo \"$${recdir}\" \
				| $(SED) -e 's/[^/][^/]*/../g'`/$(SUBMODROOTDIR)\"" || recargs=; \
		  echo "cd \"$${recdir}\" && \"$(MAKE)\" \"$${rectarget}\" $${recargs}"; \
		  $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true

# Default CHECK_RUN
CHECK_RUN	?= $(PRINTF) -- 'CHECK_RUN variable empty -> no test is done for this project\n'

############################################################################################
# .POSIX: for bsd-like dependency management
# .PHONY: .WAIT and .EXEC for compatibility, when not supported.
# .EXEC is needed on some bsdmake, so as
# phony targets don't taint to outdated the files which depend on them.
# .WAIT might not be mandatory
.POSIX:
.PHONY: .WAIT .EXEC

.PHONY: subdirs $(SUBDIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(TESTDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: subdirs $(DISTCLEANDIRS)
.PHONY: subdirs $(DEBUGDIRS)
.PHONY: subdirs $(TESTBUILDDIRS)
.PHONY: subdirs $(DOCDIRS)
.PHONY: subdirs $(CONFIGUREDIRS)
.PHONY: default_rule all build_all cleanme clean distclean dist check info rinfo \
	doc installme install debug gentags update-$(BUILDINC) create-$(BUILDINC) \
	.gitignore merge-makefile debug-makefile valgrind help test \
	subsubmodules configure
############################################################################################

default_rule: update-$(BUILDINC) $(BUILDDIRS) .WAIT $(BIN) $(LIB) $(JAR) gentags

$(SUBDIRS): $(BUILDDIRS)
$(SUBLIBS): $(BUILDDIRS)
	@true
$(BUILDDIRS): $(CONFIGMAKE) $(VERSIONINC) .EXEC
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-build=); rectarget=; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs}; fi

# --- clean : remove objects and generated files
clean: cleanme $(CLEANDIRS)
cleanme:
	$(RM) $(SRCINC_Z:.c=.*) $(SRCINC_STR:.c=.*) $(OBJ:.class=*.class) $(CLASSES:.class=*.class) \
	      $(GENSRC) $(GENINC) $(GENJAVA) $(DEPS) $(INCLUDEDEPS) $(CONFIGMAKE_REC_FILE) \
	      $(LEXSRC:=-cpp) $(YACCSRC:=-cpp) $(YACCJAVA:=-cpp) \
	      $(ADAOBJ:.o=.adb) $(ADAOBJ:.o=.ads) $(ADAOBJ:.o=.ali) $(ADAOBJ:.o=.ali) $(ADAALI)
	$(RM) -R $(TMPCLASSESDIR) || true
	@$(TEST) -L "$(FLEXLEXER_LNK)" && { cmd="$(RM) $(FLEXLEXER_LNK)"; echo "$${cmd}"; $${cmd} ; } || true
$(CLEANDIRS):
	@recdir=$(@:-clean=); rectarget=clean; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} clean

# --- distclean : remove objects, binaries and remove DEBUG flag in build.h
distclean: cleanme $(DISTCLEANDIRS)
	$(RM) $(BIN) $(LIB) $(JAR) $(BUILDINC) $(BUILDINCJAVA) $(CONFIGLOG) $(CONFIGMAKE) $(CONFIGINC) valgrind_*.log
	$(RM) -R $(BIN).dSYM || true
	$(RM) `$(FIND) . -name '.*.sw?' -or -name '*~' -or -name '\#*' $(NO_STDERR)`
	@$(cmd_TESTBSDOBJ) && { del=; for f in $(BIN) $(LIB) $(JAR) $(BUILDINC) $(BUILDINCJAVA) $(CONFIGMAKE) $(CONFIGINC); do \
	                                  $(TEST) -n "$${f}" && del="$${del} $(.CURDIR)/$${f}"; done; \
	                        for f in $(VERSIONINC) $(README) $(LICENSE); do del="$${del} $(.OBJDIR)/$${f}"; done; \
	                        echo "$(RM) $${del}"; $(RM) $${del} $(NO_STDERR); } || true
	@$(TEST) "$(BUILDDIR)" != "$(SRCDIR)" && $(RMDIR) `$(FIND) $(BUILDDIR) -type d | $(SORT) -r` $(NO_STDERR) || true
	@$(PRINTF) "$(NAME): distclean done, debug & test disabled.\n"
$(DISTCLEANDIRS):
	@recdir=$(@:-distclean=); rectarget=distclean; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} distclean

# --- debug : set DEBUG flag in build.h and rebuild
debug: $(CONFIGMAKE) update-$(BUILDINC) $(DEBUGDIRS)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	   if $(TEST) "$(RELEASE_MODE)" '!=' "DEBUG"; then \
	     { $(SED) -e 's/^\([[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*BUILD_APPRELEASE[[:space:]]\).*/\1 "DEBUG"/' \
	          $(BUILDINC) $(NO_STDERR); } > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC) || true; \
	     $(PRINTF) "$(NAME): debug enabled ('make distclean' to disable it).\n"; \
	   fi \
	   && $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; \
	   if $(TEST) -n "$(SUBMODROOTDIR)"; then "$(MAKE)" SUBMODROOTDIR="$(SUBMODROOTDIR)"; else "$(MAKE)"; fi; \
	 fi
$(DEBUGDIRS): $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-debug=); rectarget=debug; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} debug; fi

# --- test : set TEST release in build.h and rebuild
test: $(CONFIGMAKE) update-$(BUILDINC) $(TESTDIRS)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	   if $(TEST) "$(RELEASE_MODE)" = "RELEASE"; then \
	     { $(SED) -e 's/^\([[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*BUILD_APPRELEASE[[:space:]]\).*/\1 "TEST"/' \
	          $(BUILDINC) $(NO_STDERR); } > $(BUILDINC).tmp && $(MV) $(BUILDINC).tmp $(BUILDINC) \
	     && $(PRINTF) "$(NAME): test enabled ('make distclean' to disable it).\n"; \
	   fi \
	   && $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; \
	   if $(TEST) -n "$(SUBMODROOTDIR)"; then "$(MAKE)" SUBMODROOTDIR="$(SUBMODROOTDIR)"; else "$(MAKE)"; fi; \
	 fi
$(TESTDIRS): $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-test=); rectarget=test; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} test; fi

# --- doc : generate doc
doc: $(CONFIGMAKE) $(DOCDIRS)
$(DOCDIRS): $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-doc=); rectarget=doc; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} doc; fi

# --- install ---
installme: $(CONFIGMAKE) all
	@if ! $(cmd_CONFIGMAKE_RECURSE); then for f in $(INSTALL_FILES); do \
	     case "$${f}" in \
	         *.h|*.hh)    install="$(INSTALL)"; dest="$(PREFIX)/include" ;; \
	         *.a|*.so)    install="$(INSTALL)"; dest="$(PREFIX)/lib" ;; \
	         *)           if $(TEST) -x "$${f}"; then \
	                          install="$(INSTALLBIN)"; dest="$(PREFIX)/bin"; \
		              else \
			          install="$(INSTALL)"; dest="$(PREFIX)/share/$(NAME)"; \
	                      fi ;; \
	     esac; \
	     if $(TEST) -n "$${install}" -a -n "$${dest}"; then \
	         dir=`dirname "$${dest}"`; \
	         if ! $(TEST) -d "$${dir}"; then cmd="$(INSTALLDIR) $${dir}"; echo "$${cmd}"; $${cmd}; fi; \
		 cmd="$${install} $${f} $${dest}"; echo "$${cmd}"; \
		 $${cmd}; \
	     fi; \
	 done; fi
install: $(CONFIGMAKE) installme $(INSTALLDIRS)
$(INSTALLDIRS): $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-install=); rectarget=install; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} install; fi

# --- check: run tests ---
check: $(CONFIGMAKE) all $(CHECKDIRS)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then $(CHECK_RUN); fi
$(CHECKDIRS): $(CONFIGMAKE) all
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-check=); rectarget=check; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} check; fi

# --- build BIN ---
$(BIN): $(OBJ) $(SUBLIBS) $(JCNIINC)
	@if $(cmd_TESTBSDOBJ); then ln -sf "$(.OBJDIR)/`$(BASENAME) $@`" "$(.CURDIR)"; else $(TEST) -L $@ && $(RM) $@ || true; fi
	@cmd="$(CCLD) $(OBJ:.class=*.class) $(LDFLAGS) -o $@"; \
		case " $(SRC) " in *" "*".adb "*) $(CP) $(ADAOBJ:.o=.ali) $(ADAOBJ:.o=.tmp.ali); \
		                   cmd="$${cmd} "`$(GNAT) bind -shared -n -x -K $(ADAOBJ:.o=.tmp.ali) -o $(ADAOBJ:.o=.tmp.adb) \
		                      | $(SED) -n -e 's|^[[:space:]]*\(-[^lL]\)|\1|p' -e 's|^[[:space:]]*\(-l\)|\1|p' \
	                                          -e 's|^[[:space:]]*-L\([^.].*\)|-Wl,-rpath,\1 -L\1|p' | $(TR) '\n' ' '`; $(RM) -f $(ADAOBJ:.o=).tmp.*;; esac; \
	 $(PRINTF) -- "$${cmd}\n"; \
	 $${cmd}
	@$(PRINTF) "$@: build done.\n"

# --- build LIB ---
$(LIB): $(OBJ) $(SUBLIBS) $(JCNIINC)
	@if $(cmd_TESTBSDOBJ); then ln -sf "$(.OBJDIR)/`$(BASENAME) $@`" "$(.CURDIR)"; else $(TEST) -L $@ && $(RM) $@ || true; fi
	@# Workaround for issue on osx 10.11 where object names are changed when replaced,
	@# which disturbs dsymutil. This allows also to remove objects that are not part anymore of lib.
	@$(RM) $@
	$(AR) $(ARFLAGS) $@ $(OBJ:.class=*.class)
	$(RANLIB) $@
	@$(PRINTF) "$@: build done.\n"

# Build _Java_.o : $(JAVAOBJ)
$(CLASSES): $(JAVAOBJ)
	@true # Used to override implicit rule .java.class:
$(JAVAOBJ): $(JAVASRC)
	@# All classes generated/overriten at once. Generate them in tmp dir then check changed ones.
	@$(MKDIR) -p "$(TMPCLASSESDIR)"
	$(GCJ) $(JFLAGS) -d $(TMPCLASSESDIR) -C `echo '$> $^' | $(TR) ' ' '\n' | $(GREP) -E '\.java$$' | $(SORT) | $(UNIQ)` #FIXME
	@#$(GCJ) $(JFLAGS) -d $(BUILDDIR) -C $(JAVASRC)
	@for f in `$(FIND) "$(TMPCLASSESDIR)" -type f`; do \
	     dir=`dirname "$${f}" | $(SED) -e 's|$(TMPCLASSESDIR)||'`; \
	     file=`$(BASENAME) "$${f}"`; \
	     $(DIFF) -q "$(BUILDDIR)/$${dir}/$${file}" "$${f}" $(NO_STDERR) $(NO_STDOUT) \
	       || { $(MKDIR) -p "$(BUILDDIR)/$${dir}"; cp -a "$${f}" "$(BUILDDIR)/$${dir}"; }; \
	 done
	$(GCJ) $(JFLAGS) -d $(BUILDDIR) -c -o $@ $$(find "$(TMPCLASSESDIR)" -iname '*.class') #$(CLASSES:.class=*.class)
	@$(RM) -Rf "$(TMPCLASSESDIR)"

$(JCNIINC): $(ALLMAKEFILES) $(BUILDINC)

#$(JCNISRC:.cc=.o) : $(JCNIINC) # usefull without -MD
#$(JCNIOBJ): $(JCNIINC) # Useful without -MD

# This is a work in progress...
MANIFEST=$(BUILDDIR)/MANIFEST.MF

$(MANIFEST):
	printf -- "%s\n" "Manifest-Version: 1.0" \
		         "Main-Class: $(FOREIGN_MAIN:.java=)" \
		         "Class-Path: . $(CLASSPATHS)" > $(MANIFEST)  #. lib/jline-1.0.jar

$(JAR): $(JAVASRC) $(SUBLIBS) $(MANIFEST) $(ALLMAKEFILES)
	@$(MKDIR) -p "$(TMPCLASSESDIR)"
#	javac $(JAVASRC) -classpath $(SRCDIR) -classpath $(INCDIRS) -d $(BUILDDIR)
	$(JAVAC) $(JAVAFLAGS) $(JAVASRC) -d $(TMPCLASSESDIR)
#	jar cfm $@ $(MANIFEST) $(CLASSES:.class=*.class) $(INCDIRS)
	$(JARBIN) cfm $@ $(MANIFEST) $(INCDIRS) -C $(TMPCLASSESDIR) .
	@$(PRINTF) "$@: build done.\n"

##########################################################################################
#.SUFFIXES: # remove default make .SUFFIXES
.SUFFIXES:	.o .c .h .cpp .hpp .cc .hh .m .mm .adb .ads .ali _h.ads _hh.ads \
		.java .class .y-cpp .yy-cpp .yyj-cpp .l-cpp .ll-cpp .llj-cpp .y .l .yy .ll .yyj .llj

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
$(OBJ): $(CONFIG_OBJDEPS)
$(OBJ_NOJAVA): $(CONFIG_OBJDEPS_NOJAVA)
$(GENSRC): $(CONFIG_OBJDEPS)
$(GENJAVA): $(CONFIG_OBJDEPS)
$(CLASSES): $(CONFIG_OBJDEPS)

# Implicit rules: old-fashionned double suffix rules to be compatible with most make.
# -----------
# EXT: .mm
# -----------
.m.o:
	$(OBJC) $(OBJCFLAGS) $(FLAGS_OBJC_$<) $(CPPFLAGS) -c -o $@ $<
#$(BUILDDIR)/%.o: $(SRCDIR)/%.m
#	$(OBJC) $(OBJCFLAGS) $(FLAGS_OBJC_$<) $(CPPFLAGS) -c -o $@ $<
# -----------
# EXT: .mm
# -----------
.mm.o:
	$(OBJCXX) $(OBJCXXFLAGS) $(FLAGS_OBJCXX_$<) $(CPPFLAGS) -c -o $@ $<
#$(BUILDDIR)/%.o: $(SRCDIR)/%.mm
#	$(OBJCXX) $(OBJCXXFLAGS) $(FLAGS_OBJCXX_$<) $(CPPFLAGS) -c -o $@ $<
# -----------
# EXT: .c
# -----------
.c.o:
	$(CC) $(CFLAGS) $(FLAGS_C_$<) $(CPPFLAGS) -c -o $@ $<
#$(BUILDDIR)/%.o: $(SRCDIR)/%.c
#	$(CC) $(CFLAGS) $(FLAGS_C_$<) $(CPPFLAGS) -c -o $@ $<
# -----------
# EXT: .cpp
# -----------
.cpp.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
#$(BUILDDIR)/%.o: $(SRCDIR)/%.cc
#	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
# -----------
# EXT: .cc
# -----------
.cc.o:
	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
#$(BUILDDIR)/%.o: $(SRCDIR)/%.cc
#	$(CXX) $(CXXFLAGS) $(FLAGS_CXX_$<) $(CPPFLAGS) -c -o $@ $<
# -----------
# EXT: .adb
# -----------
# TODO, temporary workaround: on first build, 'gnatmake -M' (generation of ada dependency files)
#       might not work, then, if the .d file is empty, restore default dependencies (headers, ...).
# TODO, temporary workaround 2: implicit rule .ads.o must not be used if there is corresponding .adb file.
#                               That's why adb file is temporarily forced in .ads.o rule (maybe there is other way).
GNATC_CMD	= $(GNATC) $(ADAFLAGS) $(FLAGS_ADA_$${input}) $(CPPFLAGS) -c -o $@ "$${input}"
GNATC_DEP_CMD	= $(GNATMAKE) -c -gnats -M $(ADAFLAGS) $(FLAGS_ADA_$${input}) $(CPPFLAGS) -o $@ "$${input}" > $(@:.o=.d).tmp; \
	 	  $(TEST) -s "$(@:.o=.d).tmp" && $(MV) "$(@:.o=.d).tmp" "$(@:.o=.d)" || $(RM) -f "$(@:.o=.d).tmp"
.adb.o:
	@input="$<"; $(PRINTF) "$(GNATC_CMD)\n"; $(GNATC_CMD) && \
	$(GNATC_DEP_CMD)
.ads.o:
	@$(TEST) -e $(<:.ads=.adb) && input="$(<:.ads=.adb)" || input="$<"; $(PRINTF) "$(GNATC_CMD)\n"; \
	 $(GNATC_CMD) && $(GNATC_DEP_CMD)
#$(BUILDDIR)/%.o: $(SRCDIR)/%.adb
#	$(GNATC) $(ADAFLAGS) $(FLAGS_ADA_$<) $(CPPFLAGS) -c -o $@ $<
.o.ali:
	@true
.adb.h:
	@true
# -----------
# EXT: .ads  TODO, .h.ads files not part of INCLUDES or OBJ then .h.ads generation is never done.
#            TODO: gcc fdump-ada-spec changes '-' into '_' and puts all in lower case.
# -----------
# generates .ads files importing c symbols from a h header file.
.h_h.ads .hh_hh.ads:
	if $(TEST) -x "$(GNAT)"; then $(CC) -fdump-ada-spec -o $@ $< ; fi
# -----------
# _ada-stub_
# -----------
$(ADAOBJ:.o=.adb): $(ADAALI)
	@case '$(FOREIGN_MAIN)' in *.adb) arg=;; *) arg="-n";; esac; \
	 cmd="$(GNATBIND) -shared $${arg} $(FLAGS_ADA_$@) -o $@ $(ADAALI)"; \
	 $(PRINTF) -- "$${cmd}\n"; $${cmd}
# -----------
# EXT: .java
# -----------
.java.o:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) $< -o $@ $<
.java.class:
	$(GCJ) $(JFLAGS) $(FLAGS_GCJ_$<) -d $(BUILDDIR) -C $<
# -----------
# EXT: java cni
# -----------
.class.hh:
	@$(RM) $@
	$(GCJH) $(JHFLAGS) $(FLAGS_GCJH_$<) -o $@ $<
	@$(TOUCH) $@ || true
#$(BUILDDIR)/%.hh: $(SRCDIR)/%.class
# -----------
# EXTs: .l-cpp .ll-cpp .llj-cpp .y-cpp .yy-cpp .yyj-cpp
# -----------
BCOMPAT_SED_YYPREFIX=$(SED) -n -e \
	"s/^[[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*BCOMPAT_YYPREFIX[[:space:]][[:space:]]*\([A-Za-z_][A-Za-z0-9_]*\)/$${opt}\1/p" $<
# This runs C preprocessor for the '//%#<directives>' or '/*%#<directives>'
cmd_BISON_CPP   = echo "+ preprocessing $< for bison $(BISON_VERSION) (-> $(<:=-cpp)) ..."; \
		  $(SED) -e 's|^\([[:space:]]*\#\)|/*TEMP*///%%\1|' \
		         -e 's|^\([[:space:]]\)*/[/*]%\#|\1\#|' -e 's|^\([[:space:]]*\#.*\)\([/*]\{2\}\)$$|\1 //\2|' $< \
		  | $(CC) -x c -E -CC -P $(CPPFLAGS) -DBUILD_NOT_A_C_FILE -DBISON_VERSION=$(BISON_VERSION) - \
		  | $(SED) -e 's|^/\*TEMP\*///%%||'
.y.y-cpp:
	@$(cmd_BISON_CPP) > $@
.yy.yy-cpp:
	@$(cmd_BISON_CPP) > $@
.yyj.yyj-cpp:
	@$(cmd_BISON_CPP) > $@
.l.l-cpp:
	@$(cmd_BISON_CPP) > $@
.ll.ll-cpp:
	@$(cmd_BISON_CPP) > $@
.llj.llj-cpp:
	@$(cmd_BISON_CPP) > $@
#.y.c:
#	@true
#.yy.cc:
#	@true
#.l.c:
#	@true
#.ll.cc:
#	@true
#$(BUILDDIR)/%.y-cpp: $(SRCDIR)/%.y
#	$(cmd_BISON_CPP)
#$(BUILDDIR)/%.yy-cpp: $(SRCDIR)/%.yy
#	$(cmd_BISON_CPP)
#$(BUILDDIR)/%.yyj-cpp: $(SRCDIR)/%.yyj
#	$(cmd_BISON_CPP)
#$(BUILDDIR)/%.l-cpp: $(SRCDIR)/%.l
#	$(cmd_BISON_CPP)
#$(BUILDDIR)/%.ll-cpp: $(SRCDIR)/%.ll
#	$(cmd_BISON_CPP)
#$(BUILDDIR)/%.llj-cpp: $(SRCDIR)/%.llj
#	$(cmd_BISON_CPP)
# -----------
# EXT: .l
# -----------
LEX_CMD		= opt='-P'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
		  set -x; $(LEX) $(LFLAGS) $${args} $(FLAGS_LEX_$<) -o$@
#.l-cpp.c:
.l.c:
	@$(cmd_BISON_CPP) > $(<:=-cpp)
	@$(LEX_CMD) $(<:=-cpp)
#$(BUILDDIR)/%.c: $(SRCDIR)/%.l
#	@$(LEX_CMD)
# -----------
# EXT: .ll
# -----------
LEXCXX_CMD	= opt='-P'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
		  set -x; $(LEX) $(LCXXFLAGS) $${args} $(FLAGS_LEX_$<) -o$@
#.ll-cpp.cc:
.ll.cc:
	@$(cmd_BISON_CPP) > $(<:=-cpp)
	@$(LEXCXX_CMD) $(<:=-cpp)
#$(BUILDDIR)/%.cc: $(SRCDIR)/%.ll
#	@$(LEXCXX_CMD)
.llj.java:
	@$(cmd_BISON_CPP) > $(<:=-cpp)
	$(LEX) $(LJFLAGS) $(FLAGS_LEX_$<) -o$@ $(<:=-cpp)
# -----------
# EXT: .y
# -----------
YACC_CMD	= opt='-p'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
		  set -x; $(YACC) $(YFLAGS) $${args} $(FLAGS_YACC_$<) -o $@
YACC_CMD_MV	= case " $(YFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
		      if $(TEST) -e "$(@D)/y.tab.h"; then set -x; $(MV) $(@D)/y.tab.h $(@:.c=.h); set +x; fi ;; \
		  esac
#.y-cpp.c:
.y.c:
	$(cmd_BISON_CPP) > $(<:=-cpp)
	@$(YACC_CMD) $(<:=-cpp)
	@$(YACC_CMD_MV)
#$(BUILDDIR)/%.c: $(SRCDIR)/%.y-cpp
#	@$(YACC_CMD)
# -----------
# EXT: .yy
# -----------
YACCCXX_CMD	= opt='-p'; args=`$(BCOMPAT_SED_YYPREFIX)`; \
		  set -x; $(YACC) $(YCXXFLAGS) $${args} $(FLAGS_YACC_$<) -o $@
YACCCXX_CMD_MV	= case " $(YCXXFLAGS) $(FLAGS_YACC_$<) " in *" -d "*) \
		      if $(TEST) -e "$(@:.cc=.h)"; then set -x; $(MV) $(@:.cc=.h) $(@:.cc=.hh); \
		      elif $(TEST) -e "$(@D)/y.tab.h"; then set -x; $(MV) $(@D)/y.tab.h $(@:.cc=.hh); fi;; \
		  esac
#.yy-cpp.cc:
.yy.cc:
	@$(cmd_BISON_CPP) > $(<:=-cpp)
	@$(YACCCXX_CMD) $(<:=-cpp)
	@$(YACCCXX_CMD_MV)
#$(BUILDDIR)/%.cc: $(SRCDIR)/%.yy
#	@$(YACCCXX_CMD)
# -----------
# EXT: .yyj
# -----------
#.yyj-cpp.java:
.yyj.java:
	@$(cmd_BISON_CPP) > $(<:=-cpp)
	$(BISON3) $(YJFLAGS) $(FLAGS_YACC_$<) -o $@ $(<:=-cpp)
#$(BUILDDIR)/%.java: $(SRCDIR)/%.yyj
#	$(BISON3) $(YJFLAGS) $(FLAGS_YACC_$<) -o $@ $<
.y.h:
	@true
.yy.hh:
	@true
.l.h:
	@true
.ll.hh:
	@true
#.y-cpp.h:
#	@true
#.yy-cpp.hh:
#	@true
############################################################################################

#@#cd "$(DISTDIR)" && ($(ZIP) -q -r "$${distname}.zip" "$${distname}" || true)
DIST_NO_SCM	= --exclude='.git' --exclude='CVS/' --exclude='.hg/' --exclude='.svn/'
DIST_NO_SCM_FIND= -name '.git' -or -name 'CVS' -or -name '.hg' -or -name '.svn'
DIST_NO_SCM_FIND$(DIST_NO_SCM)	=
dist:
	@$(cmd_TESTBSDOBJ) && cd $(.CURDIR) || true; \
	 version=`$(GREP) -E '^[[:space:]]*\#[[:space:]]*define[[:space:]][[:space:]]*APP_VERSION[[:space:]][[:space:]]*"' \
	            $(VERSIONINC) | $(SED) -e 's/^.*"\([^"]*\)".*/\1/'` \
	 && distname="$(NAME)_$${version}_`$(DATE) '+%Y-%m-%d_%Hh%M'`" \
	 && topdir=`pwd` \
	 && $(MKDIR) -p "$(DISTDIR)/$${distname}" \
	 && $(PRINTF) "$(NAME): creating '$(DISTDIR)/$${distname}' (make dist DIST_NO_SCM= to include scm)...\n" \
	 && { $(TAR) c ${DIST_NO_SCM} --exclude='*.o' --exclude='*.d' \
	               --exclude='./obj/' --exclude='./$(NAME)' . | $(TAR) x -C "$(DISTDIR)/$${distname}" $(NO_STDERR) \
	      || { cp -Rf . "$(DISTDIR)/$${distname}" \
	           && $(RM) -R `$(FIND) "$(DISTDIR)/$${distname}" -type d -and \( $(DIST_NO_SCM_FIND) \) $(NO_STDERR)`; }; } \
	 && { for d in . $(SUBDIRS); do ver="$(DISTDIR)/$${distname}/$${d}/$(VERSIONINC)"; cd "$${d}" && "$(MAKE)" update-$(BUILDINC); cd "$${topdir}"; \
	      pat=`$(SED) -n -e "s|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*BUILD_\(GIT[^[:space:]]*\)[[:space:]]*\"\(.*\)|-e 's,DIST_\1 .*,DIST_\1 \"?-from:\2,'|p" \
	           "$${d}/$(BUILDINC)" | $(TR) '\n' ' '`; \
	      mv "$${ver}" "$${ver}.tmp" && eval "$(SED) $${pat} $${ver}.tmp" > "$${ver}" && $(RM) "$${ver}.tmp"; done; } \
	 && $(PRINTF) "$(NAME): building '$(DISTDIR)/$${distname}'...\n" \
	 && cd "$(DISTDIR)/$${distname}" && "$(MAKE)" distclean && "$(MAKE)" && "$(MAKE)" distclean && cd "$${topdir}" \
	 && cd "$(DISTDIR)" && { $(TAR) czf "$${distname}.tar.gz" "$${distname}" && targz=true || targz=false; \
	                         $(TAR) cJf "$${distname}.tar.xz" "$${distname}" || $${targz}; } && cd "$${topdir}" \
	 && $(RM) -R "$(DISTDIR)/$${distname}" \
	 && $(PRINTF) "$(NAME): archives created: $$(ls $(DISTDIR)/$${distname}.* | $(TR) '\n' ' ')\n"

$(SRCINC_STR): $(SRCINC_CONTENT)
	@# Generate $(SRCINC) containing all sources.
	@$(PRINTF) "$(NAME): generate $@\n"
	@$(MKDIR) -p "$(@D)"
	@$(cmd_TESTBSDOBJ) && input="$>" || input="$(SRCINC_CONTENT)"; \
	 $(PRINTF) "/* generated content */\n" > $@ ; \
		$(AWK) 'BEGIN { dbl_bkslash="\\"; gsub(/\\/, "\\\\\\", dbl_bkslash); o="awk on ubuntu 12.04"; \
	                        if (dbl_bkslash=="\\\\") dbl_bkslash="\\\\\\"; else dbl_bkslash="\\\\"; \
				print "#include <stdlib.h>\n#include <stdio.h>\n" \
		                      "#include \"$(VERSIONINC)\"\n#ifdef APP_INCLUDE_SOURCE\n" \
				      "static const char * const s_program_source[] = { (const char *) 0x0abcCafeUL,"; } \
		   function printblk() { \
	               gsub(/\\/, dbl_bkslash, blk); \
                       gsub(/"/, "\\\"", blk); \
	               gsub(/\n/, "\\n\"\n\"", blk); \
	               print "\"" blk "\\n\","; \
	           } { \
		       if (curfile != FILENAME) { \
		           fname=FILENAME; if ("$(.OBJDIR)" != "$(.CURDIR)" && index(fname, "$(.CURDIR)") == 1) { fname=substr(fname,length("$(.CURDIR)")+2); }; \
		           curfile="/* #@@# FILE #@@# $(NAME)/" fname " */"; blk = blk "\n"; blk=blk "\n" curfile; curfile=FILENAME; \
	               } if (length($$0 " " blk) > 500) { \
	                   printblk(); blk=$$0; \
                       } else \
		           blk=blk "\n" $$0; \
		   } END { \
		       printblk(); print "\"\\n\", NULL };\n" \
	           }' $${input} >> '$@'; \
	     print_getsrc_fun() { \
	         name_fixed=`$(cmd_NAME_FIXED)`; \
	         $(PRINTF) "%s\n" \
	            '# ifndef BUILD_VLIB' '#  define BUILD_VLIB 0' ' #endif' '# if BUILD_VLIB' '#  include "vlib/util.h"' '# endif' \
	            "int $${name_fixed}_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {" \
	            '# if defined(BUILD_VLIB) && BUILD_VLIB' \
	            '    return vdecode_buffer(out, buffer, buffer_size, ctx, (const char *)s_program_source, sizeof(s_program_source));' \
	            '# else' \
	            '    (void) buffer; (void) buffer_size; (void) ctx; const char *const* src;' \
	            '    if (out) for (src = s_program_source + 1; *src; src++) fprintf(out, "%s", *src);' 'return 0;' \
	            '# endif' \
	            '}' '#endif' >> '$@'; \
	     }; print_getsrc_fun; \
	     $(CC) -fsyntax-only $(CPPFLAGS) $(FLAGS_C) $(NO_STDERR) '$@' \
	         || { $(PRINTF) "%s\n" '#include <stdlib.h>' '#include <stdio.h>' '#include "$(VERSIONINC)"' '#ifdef APP_INCLUDE_SOURCE' \
	                             'static const char * const s_program_source[] = { (const char *) 0xabcCafeUL,' \
				     '    "cannot include source. check awk version or antivirus or bug\n", NULL' \
				     '};' > '$@'; print_getsrc_fun; }

VDECODEBUFFER_SOURCE= \
	"typedef struct { z_stream z; size_t off; } decodebuf_t; /* $(DASH)$(DASH)ZSRC_BEGIN */" \
	"ssize_t vdecode_buffer(FILE * out, char * outbuf, size_t outbufsz, void ** ctx, const char * inbuf, size_t inbufsz) {" \
	"    int             internalbuf = 0, ret = Z_DATA_ERROR;" \
	"    ssize_t         n = 0;" \
	"    decodebuf_t *   pctx = ctx ? *ctx : NULL;" \
	"    if (outbuf == NULL) {" \
	"       if (out == NULL || (outbuf = malloc((internalbuf = outbufsz = 4096))) == NULL) return -1; /* cannot use internal if no file given */" \
	"    } else if (ctx == NULL || outbufsz == 0) return -1; /* fail if giving a buf with size 0 or without ctx */" \
	"    if (pctx == NULL) { /* init inflate stream */" \
	"        if ((pctx = malloc(sizeof(decodebuf_t))) == NULL) { if (internalbuf) free(outbuf); return -1; }" \
	"        pctx->z.next_in = Z_NULL; pctx->z.zalloc = Z_NULL; pctx->z.zfree = Z_NULL; pctx->z.opaque = Z_NULL;" \
	"        pctx->z.avail_in = 0; pctx->off = 0;" \
	"        if ((ret = inflateInit2(&pctx->z, 31/*15(max_window)+16(gzip)*/)) != Z_OK) { if(pctx) free(pctx);if(internalbuf) free(outbuf); return -1; }" \
	"        if (ctx != NULL) *ctx = pctx;" \
	"    }" \
	"    do { /* decompress buffer */" \
	"        if (pctx->z.avail_in == 0 && pctx->off < inbufsz) {" \
	"            pctx->z.avail_in = pctx->off + outbufsz > inbufsz ? inbufsz - pctx->off : outbufsz;" \
	"            pctx->z.next_in = (Bytef*) inbuf + pctx->off; pctx->off += outbufsz;" \
	"        }" \
	"        pctx->z.avail_out = outbufsz; pctx->z.next_out = (Bytef*) outbuf;" \
	"        if ((ret = inflate(&pctx->z, Z_NO_FLUSH)) != Z_OK && ret != Z_STREAM_END) break ;" \
	"        n = outbufsz - pctx->z.avail_out;" \
	"        if (n > 0 && out && (fwrite(outbuf, 1, n, out) != (size_t)n || ferror(out)) && ((ret = Z_STREAM_ERROR) || 1)) break ;" \
	"    } while (ret != Z_STREAM_END && (internalbuf || (ret == Z_OK && n == 0)));" \
	"    if (internalbuf) { free(outbuf); n = 0; }" \
	"    if (n <= 0 || (ret != Z_OK && ret != Z_STREAM_END)) { /* last call */" \
	"        inflateEnd(&pctx->z); free(pctx);" \
	"        if (ctx) *ctx = NULL;" \
	"    }" \
	"    return ret == Z_OK || ret == Z_STREAM_END ? n : -1;" \
	"} /* $(DASH)$(DASH)ZSRC_END */"

$(SRCINC_Z): $(SRCINC_CONTENT)
	@# Generate $(SRCINC) containing all sources.
	@$(PRINTF) "$(NAME): generate $@\n"
	@$(MKDIR) -p "$(@D)"
	@$(cmd_TESTBSDOBJ) && input="$>" || input="$(SRCINC_CONTENT)"; \
	 $(PRINTF) "%s\n" "/* generated content */" \
	                  "#ifdef HAVE_VERSION_H" "# include \"$(VERSIONINC)\"" "#endif" \
	                  "#ifdef APP_INCLUDE_SOURCE" \
	                  "# include <stdlib.h>" "# include <stdio.h>" \
	                  "# ifndef BUILD_VLIB" "#  define BUILD_VLIB 0" "# endif" \
	                  "# if BUILD_VLIB" "#  include <vlib/util.h>" \
	                  "# else" "#  include <zlib.h>" "# endif" \
	                  "static const unsigned char s_program_source[] = {" \
	    > '$@' ; \
	 dumpsrc() { for f in $${input}; do \
	     $(cmd_TESTBSDOBJ) && fname=`echo "$${f}" | sed -e 's|^$(.CURDIR)/||' -e 's|^$(.OBJDIR)/||'` || fname=$${f}; \
	     $(PRINTF) "\n/* #@@# FILE #@@# $(NAME)/$${fname} */\n"; \
	     cat "$${f}"; \
	     done; echo; }; dumpsrc | $(GZIP) -c | $(OD) -An -tuC | $(SED) -e 's/[[:space:]][[:space:]]*0*\([0-9][0-9]*\)/\1,/g' >> '$@'; \
	 sha=`$(WHICH) shasum sha256 sha256sum cksum $(NO_STDERR) | $(HEADN1)`; \
	 case "$${sha}" in */shasum) sha="$${sha} -a256";; */cksum) sha="$${sha} -a SHA256";; esac; \
	 name_fixed=`$(cmd_NAME_FIXED)`; \
	 { $(PRINTF) '%s\n' "};" "static const char * s_program_hash = \"`dumpsrc | $${sha} | $(AWK) '{ print $$1; }'`\";"; \
	 $(cmd_HAVEVLIB) || $(PRINTF) '%s\n' $(VDECODEBUFFER_SOURCE); \
	 $(PRINTF) '%s\n' \
	     "int $${name_fixed}_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx) {" \
	     "    (void) s_program_hash;" \
	     "    return vdecode_buffer(out, buffer, buffer_size, ctx, (const char *) s_program_source, sizeof(s_program_source));" \
	     "} /* ##ZSRC_END */" \
	     "#endif /* ! APP_INCLUDE_SOURCE */"; } >> '$@'

$(LICENSE):
	@$(cmd_TESTBSDOBJ) && $(TEST) -e "$(.CURDIR)/$@" || echo "$(NAME): create $@"
	@$(PRINTF) "GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007 - http://gnu.org/licenses/gpl.html\n" > '$@'
	@if $(cmd_TESTBSDOBJ); then $(TEST) -e "$(.CURDIR)/$@" || mv $@ "$(.CURDIR)"; ln -sf "$(.CURDIR)/$@" .; fi

$(README):
	@$(cmd_TESTBSDOBJ) && $(TEST) -e "$(.CURDIR)/$@" || echo "$(NAME): create $@"
	@$(PRINTF) "%s\n" "## $(NAME)" "---------------" "" "* [Overview](#overview)" "* [License](#license)" "" \
	                  "## Overview" "TODO !" "" "## License" "GPLv3 or later. See LICENSE file." >> '$@'
	@if $(cmd_TESTBSDOBJ); then $(TEST) -e "$(.CURDIR)/$@" || mv '$@' "$(.CURDIR)"; ln -sf "$(.CURDIR)/$@" .; fi

$(VERSIONINC):
	@$(cmd_TESTBSDOBJ) && $(TEST) -e "$(.CURDIR)/$@" || echo "$(NAME): create $@"
	@$(PRINTF) "%s\n" "#ifndef APP_VERSION_H" "# define APP_VERSION_H" \
		          "# define APP_COMMENT__ \"PUT ONLY LINES STARTING WITH '#' IN THIS FILE.\"" \
			  "# undef APP_COMMENT__" "# define APP_VERSION \"0.1\"" \
			  "# define APP_INCLUDE_SOURCE" "# define APP_BUILD_NUMBER 1" "# define DIST_GITREV \"unknown\"" \
			  "# define DIST_GITREVFULL \"unknown\"" "# define DIST_GITREMOTE \"unknown\"" \
			  "# include \"build.h\"" "#endif" >> '$@'
	@if $(cmd_TESTBSDOBJ); then $(TEST) -e "$(.CURDIR)/$@" || mv '$@' "$(.CURDIR)"; ln -sf "$(.CURDIR)/$@" .; fi

# As defined above, everything depends on $(BUILDINC), and we want they wait for update-$(BUILDINC)
# create-$(BUILDINC) and update-$(BUILDINC) have .EXEC so that some bsd-make don' taint to outdated
# the files which depends on them.
$(BUILDINC): update-$(BUILDINC)
	@true
$(BUILDINCJAVA): update-$(BUILDINC)
	@true
#fullgitrev=`$(GIT) describe --match "v[0-9]*" --always --tags --dirty --abbrev=0 $(NO_STDERR)`
update-$(BUILDINC): $(CONFIGMAKE) $(VERSIONINC) .EXEC
	@if ! $(cmd_CONFIGMAKE_INCLUDED); then true; else \
	 if $(cmd_TESTBSDOBJ); then ln -sf "$(.OBJDIR)/$(BUILDINC)" "$(.CURDIR)"; ln -sf "$(.OBJDIR)/$(BUILDINCJAVA)" "$(.CURDIR)"; \
	 else $(TEST) -L $(BUILDINC) && $(RM) $(BUILDINC); $(TEST) -L $(BUILDINCJAVA) && $(RM) $(BUILDINCJAVA) || true; fi; \
	 name_fixed=`$(cmd_NAME_FIXED)`; \
	 if ! $(TEST) -e $(BUILDINC); then \
	     echo "$(NAME): create $(BUILDINC)"; \
	     $(cmd_TESTBSDOBJ) && ! $(TEST) -e "$(VERSIONINC)" && ln -sf "$(.CURDIR)/$(VERSIONINC)" .; \
	     build=`$(SED) -n -e 's/^[[:space:]]*#[[:space:]]*define[[:space:]]APP_BUILD_NUMBER[[:space:]][[:space:]]*\([0-9][0-9]*\).*/\1/p' $(VERSIONINC)`; \
	     $(PRINTF) "%s\n" "#define BUILD_APPNAME \"\"" "#define BUILD_NUMBER $${build}" "#define BUILD_PREFIX \"\"" \
	       "#define BUILD_GITREV \"\"" "#define BUILD_GITREVFULL \"\"" "#define BUILD_GITREMOTE \"\"" \
	       "#define BUILD_APPRELEASE \"\"" "#define BUILD_SYSNAME \"\"" "#define BUILD_SYS_unknown" \
	       '#define BUILD_FOREIGN_MAIN 0' '#define BUILD_JAVA_MAIN 0' '#define BUILD_ADA_MAIN 0' \
	       "#define BUILD_MAKE \"\"" "#define BUILD_CC_CMD \"\"" "#define BUILD_CXX_CMD \"\"" "#define BUILD_OBJC_CMD \"\"" \
	       "#define BUILD_GCJ_CMD \"\"" "#define BUILD_JAVAC_CMD \"\"" '#define BUILD_GNATC_CMD ""' "#define BUILD_CCLD_CMD \"\"" \
	       "#define BUILD_JAVAOBJ 0" '#define BUILD_GNAT 0' "#define BUILD_JAR 0" "#define BUILD_BIN 0" "#define BUILD_LIB 0" \
	       "#define BUILD_YACC 0" "#define BUILD_LEX 0" "#define BUILD_BISON3 0" "#define BUILD_BISON_VERSION 0" \
	       "#define BUILD_SRCPATH \"\"" "#define BUILD_VLIB 0" "#define BUILD_CONFIG_CHECK \"\"" "#include \"$(CONFIGINC)\"" \
	       "#ifdef __cplusplus" "# define BUILD_EXTERN(L) extern L" "#else" "# define BUILD_EXTERN(L)" "#endif" \
               "#ifndef BUILD_NOT_A_C_FILE" "#include <stdio.h>" \
	       "BUILD_EXTERN(\"C\") int $${name_fixed}_get_source(FILE * out, char * buffer, unsigned int buffer_size, void ** ctx);" \
	       "#endif" >> $(BUILDINC); \
	 fi; \
	 if gitstatus=`$(GIT) status --untracked-files=no --ignore-submodules=untracked --short --porcelain $(NO_STDERR)`; then \
	     i=0; for rev in `$(GIT) show --quiet --ignore-submodules=untracked --format="%h %H" HEAD $(NO_STDERR)`; do \
	         case $${i} in 0) gitrev="$${rev}";; 1) fullgitrev="$${rev}" ;; esac; \
	         i=$$((i+1)); \
	     done; if $(TEST) -n "$${gitstatus}"; then gitrev="$${gitrev}-dirty"; fullgitrev="$${fullgitrev}-dirty"; fi; \
	     gitremote="\"`$(GIT) remote get-url origin $(NO_STDERR)`\""; \
	     gitrev="\"$${gitrev}\""; fullgitrev="\"$${fullgitrev}\""; \
	 else gitrev="DIST_GITREV"; fullgitrev="DIST_GITREVFULL"; gitremote="DIST_GITREMOTE"; fi; \
 	 case " $(OBJ) " in *" $(JAVAOBJ) "*) javaobj=1;; *) javaobj=0;; esac; \
	 $(TEST) -n "$(JAR)" && jar=1 || jar=0; \
	 $(TEST) -n "$(BIN)" && bin=1 || bin=0; \
	 $(TEST) -n "$(LIB)" && lib=1 || lib=0; \
	 $(TEST) -n "$(YACC)" && yacc=1 || yacc=0; \
	 $(TEST) -n "$(LEX)" && lex=1 || lex=0; \
	 $(TEST) -n "$(BISON3)" && bison3=1 || bison3=0; \
	 $(TEST) -n "$(BISON_VERSION)" && bisonversion="$(BISON_VERSION)" || bisonversion=0; \
	 $(TEST) -n "$(SRCINC)" && appsource=true || appsource=false; \
	 foreign_main=0; java_main=0; ada_main=0; $(TEST) -n "$(FOREIGN_MAIN)" \
	     && case " $(SRC) $(JAVASRC) " in *" $(FOREIGN_MAIN) "*) foreign_main=1; \
	         case "$(FOREIGN_MAIN)" in *.adb) ada_main=1;; *.java) java_main=1;; esac;; esac; \
	 gnat=0; $(TEST) -x "$(GNAT)" && case " $(SRC) " in *" "*".adb "*) gnat=1;; esac || true; \
	 $(cmd_HAVEVLIB) && vlib=1 || vlib=0; \
	 if $(SED) -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREV[[:space:]]\).*|\1$${gitrev}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREVFULL[[:space:]]\).*|\1$${fullgitrev}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GITREMOTE[[:space:]]\).*|\1$${gitremote}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_PREFIX[[:space:]]\).*|\1\"$(PREFIX)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SRCPATH[[:space:]]\).*|\1\"$${PWD}\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_APPNAME[[:space:]]\).*|\1\"$(NAME)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_APPRELEASE[[:space:]]\).*|\1\"$(RELEASE_MODE)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SYSNAME[[:space:]]\).*|\1\"$(SYSDEP_SUF)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_SYS_\).*|\1$(SYSDEP_SUF)|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_MAKE[[:space:]]\).*|\1\"$(MAKE)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CC_CMD[[:space:]]\).*|\1\"$(CC) $(CFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CXX_CMD[[:space:]]\).*|\1\"$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_OBJC_CMD[[:space:]]\).*|\1\"$(OBJC) $(OBJCFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GCJ_CMD[[:space:]]\).*|\1\"$(GCJ) $(JFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAVAC_CMD[[:space:]]\).*|\1\"$(JAVAC) $(JAVAFLAGS)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GNATC_CMD[[:space:]]\).*|\1\"$(GNATC) $(ADAFLAGS) $(CPPFLAGS) -c\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CCLD_CMD[[:space:]]\).*|\1\"$(CCLD) $(LDFLAGS)\"|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAVAOBJ[[:space:]]\).*|\1$${javaobj}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_GNAT[[:space:]]\).*|\1$${gnat}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAR[[:space:]]\).*|\1$${jar}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_BIN[[:space:]]\).*|\1$${bin}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_LIB[[:space:]]\).*|\1$${lib}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_FOREIGN_MAIN[[:space:]]\).*|\1$${foreign_main}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_JAVA_MAIN[[:space:]]\).*|\1$${java_main}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_ADA_MAIN[[:space:]]\).*|\1$${ada_main}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_YACC[[:space:]]\).*|\1$${yacc}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_LEX[[:space:]]\).*|\1$${lex}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_BISON3[[:space:]]\).*|\1$${bison3}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_BISON_VERSION[[:space:]]\).*|\1$${bisonversion}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_VLIB[[:space:]]\).*|\1$${vlib}|" \
	        -e "s|^\([[:space:]]*#define[[:space:]][[:space:]]*BUILD_CONFIG_CHECK[[:space:]]\).*|\1\"$(CONFIG_CHECK)\"|" \
	        -e "s|^\(.*[[:space:]]\)[[:space:]]*.*\(_get_source[[:space:]]*(.*\)|\1$${name_fixed}\2|" \
	        $(BUILDINC) > $(BUILDINC).tmp \
	 ; then \
	    if $(DIFF) -q $(BUILDINC) $(BUILDINC).tmp $(NO_STDOUT); then $(RM) $(BUILDINC).tmp; \
	    else $(MV) $(BUILDINC).tmp $(BUILDINC) && echo "$(NAME): $(BUILDINC) updated" \
	    && if $(TEST) "$${javaobj}" = "1" || $(TEST) "$${jar}" = "1"; then \
	        debug=false;test=false;echo " $(MACROS) " | $(GREP) -q -- ' -D_TEST ' && test=true; echo " $(MACROS) " | $(GREP) -q -- ' -D_DEBUG ' && debug=true; \
	        { $(PRINTF) "public final class Build {\n" && \
	        $(SED) -n -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\(BUILD_GIT[^[:space:]]*\)[[:space:]][[:space:]]*\(.*\).*|    public static final String  \1 = \2;|p' \
	                  -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\([^[:space:]]*\)[[:space:]][[:space:]]*\(".*"\).*|    public static final String  \1 = \2;|p' \
	                  -e 's|^[[:space:]]*#[[:space:]]*define[[:space:]][[:space:]]*\([^[:space:](]*\)[[:space:]][[:space:]]*\([^[:space:]]*\).*|    public static final int     \1 = \2;|p' \
	                   $(VERSIONINC) $(BUILDINC) $(CONFIGINC) \
	        && $(PRINTF) "%s\n" "    public static final String  BUILD_SYS = \"$(UNAME_SYS)\";" \
	                            "    public static final boolean BUILD_DEBUG = $${debug};" \
	                            "    public static final boolean BUILD_TEST = $${test};" \
	                            "    public static final String  BUILD_DATE = \"`date '+%Y-%m-%d %H:%M:%S %Z'`\";" \
	                            "    public static final boolean APP_INCLUDE_SOURCE = $${appsource};" "}"; \
	        } > $(BUILDINCJAVA); \
	       fi; \
	    fi; \
	 fi; fi

# configure, CONFIGMAKE, ... config.h, config.make, config.log generation
configure: $(CONFIGUREDIRS)
	@$(RM) -f $(CONFIGMAKE) $(INCLUDEDEPS)
	@$(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; "$(MAKE)" $(CONFIGMAKE)
$(CONFIGUREDIRS):
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 recdir=$(@:-configure=); rectarget=configure; $(RECURSEMAKEARGS); cd "$${recdir}" && "$(MAKE)" $${recargs} configure; fi
# prefixes where to search libraries
CONFIG_CHECK_PREFIXES	= '' '/opt/local' '/usr/local'
# Variables with content of programs checking features.
# + quotes must be in variable content.
# + '#' should be escaped.
# + printf sequences like '%' or '\\' should be escaped as printf will get those strings.
CONFTEST_CURSES_TI_ALL	= int main() {\n\t if (isatty(STDOUT_FILENO) && setupterm(0, STDOUT_FILENO, 0) == OK) {\n\
			  \t\t  fprintf(stderr, "cols %%d\\n", tigetnum("cols"));\n\
			  \t\t  fprintf(stderr, "setaf %%d\\n", (int)tigetstr("setaf"));\n\
			  \t\t  fprintf(stderr, "setbf %%d\\n", (int)tigetstr("setf"));\n\
			  \t\t  fprintf(stderr, "colors %%d\\n", tigetnum("colors"));\n\
			  \t\t  char *s; if ((s=tigetstr("setaf")) != NULL && s != (char*)-1)\n\
			  \t\t\t  fprintf(stderr, "setaf 1 %%s\\n", tparm(s, 1));\n\
			  \t\t  del_curterm(cur_term);\n\
			  \t} return 0;\n}\n

CONFTEST_CURSES_TI	= '\#include <unistd.h>\n\#include <stdio.h>\n\#include <termios.h>\n\#include <curses.h>\n\#include <term.h>\n\
			  $(CONFTEST_CURSES_TI_ALL)'

CONFTEST_CURSES_TI_NOINC= '\#include <unistd.h>\n\#include <stdio.h>\n\#include <termios.h>\n\
			  int setupterm(void*,int,int*);\nextern void * cur_term;\nint del_curterm(void*);\nchar * tparm(const char*, ...);\n\
			  char * tigetstr(char*);\nint tigetnum(char*);\n\#define OK 0\n $(CONFTEST_CURSES_TI_ALL)'

CONFTEST_NCURSES_SCR_ALL= \#include <unistd.h>\n\#include <stdio.h>\n\#include <termios.h>\n\
			  int main() {\n\t if (isatty(STDOUT_FILENO)) {\n\
			  \t\t filter(); if (initscr() != NULL) {\n\
			  \t\t\t if (has_colors()) {\n\t\t\t start_color(); endwin(); }\n\
			  \t} } return 0;\n}\n

CONFTEST_NCURSES_SCR	= '\#include <unistd.h>\n\#include <stdio.h>\n\#include <termios.h>\n\#include <curses.h>\n\#include <term.h>\n\
			  $(CONFTEST_NCURSES_SCR_ALL)'

CONFTEST_NCURSES_SCR_NOINC = '\#include <unistd.h>\n\#include <stdio.h>\n\#include <termios.h>\n\
			     $(CONFTEST_NCURSES_SCR_ALL)'

CONFTEST_ZLIB		= '\#include <stdio.h>\n\#include <string.h>\n\#include <zlib.h>\n\
			  unsigned char inbuf[] = { 31,139,8,0,239,31,168,90,0,3,51,228,2,0,83,252,81,103,2,0,0,0 };\n \
			  int main() { unsigned char out[2] = {0,0}; z_stream z; memset(&z, 0, sizeof(z_stream));\n\
			  \t           z.next_in = inbuf; z.next_out=out; z.avail_out = 2; z.avail_in = sizeof(inbuf); \n\
			  \t  if(inflateInit2(&z, 31) == 0 && inflate(&z, Z_NO_FLUSH) == 1 && *out == '"'1'"') return 0;\n\
			  \t  printf("%%u", *out);return 1; }\n'

CONFTEST_ZLIB_NOINC	= '\#include <stdio.h>\n\#include <string.h>\n\
			  typedef struct { unsigned char *next_in; unsigned avail_in; unsigned long total_in;\n\
			  \t               unsigned char * next_out; unsigned avail_out; unsigned long total_out;\n\
			  \t               char *msg; void *state; int(*zalloc)(void*,unsigned,unsigned);\n\
			  \t               int(*zfree)(void*,void*); void * opaque; int data_type; unsigned long adler;\n\
			  \t		   unsigned long reserved; } z_stream;\n\
			  const char * zlibVersion(); int inflateInit2_(z_stream*, int, const char *, int); int inflate(z_stream*, int);\n\
			  unsigned char inbuf[] = { 31,139,8,0,239,31,168,90,0,3,51,228,2,0,83,252,81,103,2,0,0,0 };\n \
			  int main() { unsigned char out[2] = {0,0}; z_stream z; memset(&z, 0, sizeof(z_stream));\n\
			  \t           z.next_in = inbuf; z.next_out=out; z.avail_out = 2; z.avail_in = sizeof(inbuf); \n\
			  \t  if(inflateInit2_(&z, 31, zlibVersion(), sizeof(z_stream)) == 0 && inflate(&z, 0) == 1 && *out == '"'1'"') return 0;\n\
			  \t  printf("%%u", *out);return 1; }\n'

CONFTEST_LIBCRYPTO	= 'int SHA256_Init(void *);\n\
			  int main() { long n[128]; SHA256_Init(&n); return 0; }\n'

CONFTEST_APPLECRYPTO	= '\#define COMMON_DIGEST_FOR_OPENSSL\n\
			  \#include <CommonCrypto/CommonDigest.h>\nint main() { SHA256_CTX n; SHA256_Init(&n); return 0; }\n'

CONFTEST_OPENSSL	= '\#include <openssl/sha.h>\n\
			  int main() { SHA256_CTX n; SHA256_Init(&n); return 0; }\n'

CONFTEST_LIBINTL	= '\#include <stdio.h>\n\#include <libintl.h>\nint main(void)\n \
			  { fprintf(stderr, gettext("Hello Intl\\n")); return 0; }\n'

CONFTEST_SIGQUEUE	= '\#include <signal.h>\nint main(void) { union sigval si; sigqueue(0,0,si); return 0; }\n'

CONFTEST_SIGRTMIN	= '\#include <signal.h>\n\#include <stdio.h>\nint main(void) { printf("%%d", SIGRTMIN); return 0; }\n'

CONFTEST_LIBCRYPT	= 'int main(void) { return 0; }\n'

CONFTEST_CRYPT_H	= '\#include <crypt.h>\nint main(void) { return 0; }\n'

CONFTEST_CRYPT_GNU	= '\#include <stdio.h>\n\#include <string.h>\n\#include <unistd.h>\n\#include "'"$${PWD}"'/$(CONFIGINC)"\n\
			   \#if defined(CONFIG_CRYPT_H) && CONFIG_CRYPT_H\n\#include <crypt.h>\n\#endif\n\
			   int main(void) { int ret=1; int f=0; int i; char *cr, *s=strdup("$$1$$abcdefgh$$");\nfor(i=1; i <= 9; i++) \
			   {s[1]='"'0'"'+i; cr = crypt("toto", s); if (cr && !strncmp(s, cr, strlen(s))) { \nret=0; f |= 1 << (i-1); } } \
			   printf("0x%%02x", f); return ret; }\n'

$(CONFIGINC) $(CONFIGMAKE): Makefile
	 @if $(cmd_TESTBSDOBJ); then ln -sf "$(.OBJDIR)/$(CONFIGMAKE)" "$(.CURDIR)" && ln -sf "$(.OBJDIR)/$(CONFIGINC)" "$(.CURDIR)"; \
		                else $(TEST) -L "$(CONFIGMAKE)" && $(RM) "$(CONFIGMAKE)"; $(TEST) -L "$(CONFIGINC)" && $(RM) "$(CONFIGINC)" || true; fi; \
	  case " $? " in *" Makefile "*|*" $(.CURDIR)/Makefile "*) doconfigure=1;; *) doconfigure=1;; esac; \
	  $(cmd_DEBUGMAKEFILE) && $(PRINTF) -- "$(NAME): CONFIGMAKE ?:<$?> DO_CONFIGURE:$${doconfigure}\n" || true; \
	  if $(TEST) "$${doconfigure}" = "1"; then \
	  $(TEST) -s '$(VERSIONINC)' || $(RM) -f '$(VERSIONINC)'; \
	  $(PRINTF) "$(NAME): generate $(CONFIGMAKE), $(CONFIGINC)\n"; \
	  log() { $(PRINTF) "$$@"; $(PRINTF) "$$@" >> "$${configlog}"; }; \
	  checktest() { \
	      _t="$$1"; _t2="$${_t%:*}"; test -n "$${_t2}" && _t="$${_t2}"; \
	      case " $(CONFIG_CHECK) " in *" $${_t} "*|*" +$${_t} "*|*" all "*) ;; *) return 1;; esac; \
	  }; \
	  cctest() { \
	    plabel=$$1; shift; lcode=$$@; binout=; binerr=; \
	    checktest "$${plabel}" || return 1; \
	    logheader="$(NAME): checking $${plabel}"; \
	    tmpname=`$(MKTEMP) "$${mytmpdir}/cctest_XXXXXX" || echo "$${mytmpdir}/cctest_TMP"`; \
	    $(TEST) -n "$${cflags}" && logheader="$${logheader} ($${cflags})"; \
	    $(TEST) -n "$${libs}" && logheader="$${logheader} ($${libs})"; \
	    logheader="$${logheader}... "; \
	    $(PRINTF) -- "$${lcode}" > "$${tmpname}.c"; \
	    ccout=`$(CC) -pipe $${cflags} -o "$${tmpname}" "$${tmpname}.c" $${libs} 2>&1` \
		&& binout=`"$${tmpname}" 2>$${mytmpfile}` && binerr=`$(CAT) "$${mytmpfile}"` && ret=0 || ret=1; \
	    $(TEST) -n "$${binerr}" && binerr="$${binerr}\n" || true; \
	    $(RM) -f "$${tmpname}" $(NO_STDERR); \
	    $(PRINTF) -- "\n------------------------------------------------------\n" >> "$${configlog}"; \
	    $(TEST) "$${ret}" = "0" && log "$${logheader}yes $${binout}\n" || log "$${logheader}no\n"; \
	    $(TEST) -n "$${ccout}" && $(PRINTF) -- "\n*********** $(CC) $${cflags} $${libs}\n$${ccout}\n" >> "$${configlog}"; \
	    $(PRINTF) -- "\n>>>>>>>>>> $${tmpname}.c <<<<<<<<\n" >> "$${configlog}"; \
	    $(CAT) "$${tmpname}.c" >> "$${configlog}"; \
	    $(TEST) -n "$${binout}$${binerr}" \
	        && $(PRINTF) -- "\n>>>>>>>>> $${tmpname} [result:$${ret}]\n$${binerr}$${binout}\n" >> "$${configlog}"; \
	    $(RM) -f "$${tmpname}.c" $(NO_STDERR); $(RM) -Rf "$${tmpname}.dSYM" $(NO_STDERR); \
	    libs=; cflags=; \
	    configcheck=`$(PRINTF) -- "$${configcheck}" | $(SED) -e "s;[+-]$${plabel} ;;g"`; \
	    $(TEST) "$${ret}" = "0" && confres="+" || confres="-"; configcheck="$${configcheck}$${confres}$${plabel} "; \
	    $(TEST) "$${ret}" = "0" || case " $(CONFIG_CHECK) " in \
	        *" +$${plabel} "*) $(PRINTF) -- "$(NAME): error: $${plabel} is mandatory\n"; exit 1;; \
	        *) false;; esac; \
	}; \
	conftest() { \
	    incconf=$$1; shift; incval=$$1; shift; libconf=$$1; shift; libval=$$1; shift; confname=$$1; shift; \
	    checktest "$${confname}" || return 1; \
	    cctest "$${confname}" "$$@"; ret=$$?; \
	    if $(TEST) "$${ret}" = "0"; then support=1; confres="+$${confname}"; \
	                              else support=0; confres="-$${confname}"; incval=; libval=; fi; \
	    for conf in "$${incconf}" "$${libconf}"; do \
	        if $(TEST) -n "$${conf}"; then \
	            $(GREP) -Ev '^[[:space:]]*$(DASH)[[:space:]]*define[[:space:]][[:space:]]*'"$${conf}"'[[:space:]]' \
	                $(CONFIGINC) > "$${mytmpfile}"; $(MV) "$${mytmpfile}" "$(CONFIGINC)"; \
	            $(GREP) -Ev '^[[:space:]]*'"$${conf}"'(_FOUND)?[[:space:]]*=' $(CONFIGMAKE) > "$${mytmpfile}"; \
	            $(MV) "$${mytmpfile}" "$(CONFIGMAKE)"; \
	        fi; done; \
	    $(TEST) -n "$${incconf}" && { $(PRINTF) "$(DASH)define $${incconf} $${support}\n" >> $(CONFIGINC); \
	                                  $(PRINTF) '%s\n' "$${incconf}_FOUND=$${support}" "$${incconf}=$${incval}" >> $(CONFIGMAKE); }; \
	    $(TEST) -n "$${libconf}" && { $(PRINTF) "$(DASH)define $${libconf} $${support}\n" >> $(CONFIGINC); \
	                                  $(PRINTF) '%s\n' "$${libconf}_FOUND=$${support}" "$${libconf}=$${libval}" >> $(CONFIGMAKE); }; \
	    return $${ret}; \
	}; \
	getldpaths() { \
	    { for p in `$(CC) -pipe -Wl,--verbose -l__LIB_NOT_FOUND__ 2>&1 \
	              | $(SED) -n -e "s|.*[[:space:]]\([^[:space:]]*\)/lib__LIB_NOT_FOUND__.*|\1|p" \
		      | $(SED) -e 's|//*|/|g' | $(SORT) | $(UNIQ)`; do \
	        $(TEST) -d "$${p}" && (cd "$${p}" 2>/dev/null && pwd); done; \
	    for p in `$(CC) -pipe -Wl,-v -l__LIB_NOT_FOUND__ 2>&1 \
	              | $(SED) -n -e "s|^[[:space:]]*\([/_.a-zA-Z0-9-]*\).*|\1|p" \
	              | $(SED) -e 's|//*|/|g' | $(SORT) | $(UNIQ)`; do \
	        $(TEST) -d "$${p}" && (cd "$${p}" 2>/dev/null && pwd); done; } | $(SORT) | $(UNIQ); \
	}; \
	getlibpath() { \
	    llib=$$1; lver=$$2; $(TEST) -n "$${lver}" && lpat="$${lver}" || lpat="*"; \
	    for p in $${ldpaths}; do \
	        for l in `/bin/ls -1 "$${p}/lib$${llib}"*.* $(NO_STDERR) | $(SORT) -r | $(UNIQ)`; do \
	           case "$${l}" in *"$${llib}".$${lpat}."dylib"|*"$${llib}."$${lpat}".so") \
		                     l=`$(BASENAME) "$${l}"`; l=$${l$(DASH)lib}; l=$${l%.dylib}; echo "-l$${l}";; \
		                 *"$${llib}.so."$${lpat}) echo "-l:`$(BASENAME) \"$${l}\"`";; *) continue; esac; \
		$(TEST) -z "$${lver}" && return 0; done; done; \
	}; \
	mytmpdir=.; mytmpfile=`$(MKTEMP) "$${mytmpdir}/conftest_XXXXXX" || echo "$${mytmpdir}/conftest_TMP"`; \
	configcheck=; configlog=$(CONFIGLOG); $(PRINTF) "" > $${configlog}; ldpaths=`getldpaths $(NO_STDERR)`; \
	$(PRINTF) '$(DASH) generated file\n' > $(CONFIGMAKE); $(PRINTF) '/* generated file */\n' > $(CONFIGINC); \
	true "**** ZLIB CHECK *****"; \
	flag=; lib="-lz"; cflags="$${flag}" libs="$${lib}" conftest CONFIG_ZLIB_H "$${flag}" CONFIG_ZLIB "$${lib}" \
	    "zlib" $(CONFTEST_ZLIB) \
	|| { cflags="" libs="$${lib}" conftest "" "" CONFIG_ZLIB "$${lib}" \
	    "zlib:noheaders" $(CONFTEST_ZLIB_NOINC) \
	; } || true; \
	true "**** NCURSES CHECK *****"; \
	flag=; for lib in "-lncurses" "-lncurses -ltinfo" '`getlibpath ncurses` `getlibpath tinfo`' \
			  "-lcurses" "-ltinfo" '`getlibpath ncurses`' '`getlibpath tinfo`'; do \
	    case "$${lib}" in '`'*|'$$('*) lib=$$(eval echo "$${lib}");; esac; \
	    cflags="$${flag}" libs="$${lib}" conftest CONFIG_CURSES_H "$${flag}" CONFIG_CURSES "$${lib}" \
	        "ncurses" $(CONFTEST_CURSES_TI) \
	    && break \
	    || { cflags="" libs="$${lib}" conftest "" "" CONFIG_CURSES "$${lib}" \
	         "ncurses:noheaders" $(CONFTEST_CURSES_TI_NOINC) \
	    && break; } || true; \
	done; \
	cflags="$${flag}" libs="$${lib}" conftest CONFIG_CURSES_SCR "$${flag}" "" "" \
	    "ncurses:initscr" $(CONFTEST_NCURSES_SCR) \
	|| cflags="$${flag}" libs="$${lib}" conftest CONFIG_CURSES_SCR "$${flag}" "" "" \
	    "ncurses:initscr_noheaders" $(CONFTEST_NCURSES_SCR_NOINC) \
	|| true; \
	true "**** LIBCRYPTO CHECK *****"; \
	for lib in "-lcrypto" '`getlibpath crypto`'; do \
	    case "$${lib}" in '`'*|'$$('*) lib=$$(eval echo "$${lib}");; esac; \
	    flag=''; cflags="$${flag}" libs="$${lib}" conftest "" "$${flag}" CONFIG_LIBCRYPTO "$${lib}" \
	        "libcrypto" $(CONFTEST_LIBCRYPTO) && flagcrypto="$${flag}" && libcrypto="$${lib}" && break \
		|| { flagcrypto=; libcrypto=; true; }; \
	done; \
	flag=''; lib=''; cflags="$${flag}" libs="$${lib}" conftest "CONFIG_APPLECRYPTO" "$${flag}" "" "$${lib}" \
	    "applecrypto" $(CONFTEST_APPLECRYPTO) || true; \
        for prefix in $(CONFIG_CHECK_PREFIXES); do \
	    if $(TEST) -n "$${prefix}"; then cflag="-I$${prefix}/include"; lib="-L$${prefix}/lib -lcrypto"; \
	    else cflag=; lib="-lcrypto"; fi; \
	    cflags="$${cflag}" libs="$${lib}" conftest CONFIG_OPENSSL_H "$${cflag}" CONFIG_LIBOPENSSL "$${lib}" \
	        "openssl" $(CONFTEST_OPENSSL) \
	    && break || true; \
	done; \
	mlib_break=; for prefix in $(CONFIG_CHECK_PREFIXES); do \
	    for mlib in '' '-lintl'; do \
	        $(TEST) -z "$${mlib}" -a -n "$${prefix}" && continue ; \
	        cflag=; lib="$${mlib}"; \
	        if $(TEST) -n "$${prefix}"; then cflag="-I$${prefix}/include"; \
	                                         $(TEST) -n "$${mlib}" && lib="-L$${prefix}/lib $${mlib}"; fi; \
	        cflags="$${cflag}" libs="$${lib}" conftest CONFIG_LIBINTL_H "$${cflag}" CONFIG_LIBINTL "$${lib}" \
	        "libintl" $(CONFTEST_LIBINTL) \
	        && mlib_break=yes && break || true; \
	    done; $(TEST) -n "$${mlib_break}" && break; \
	done; \
	flag=''; lib=''; cflags="$${flag}" libs="$${lib}" conftest CONFIG_SIGQUEUE "$${flag}" "" "$${lib}" \
	    "sigqueue" $(CONFTEST_SIGQUEUE) || true; \
	cflags='' libs='' cctest "sigrtmin" $(CONFTEST_SIGRTMIN) \
	    && $(PRINTF) "#define CONFIG_SIGRTMIN $${binout}\n" >> $(CONFIGINC) || true; \
	flag=''; lib="-lcrypt"; cflags="$${flag}" libs="$${lib}" conftest "" "$${flag}" CONFIG_LIBCRYPT "$${lib}" \
	    "libcrypt" $(CONFTEST_LIBCRYPT) && libcrypt="$${lib}" && flagcrypt="$${flag}" \
	    || { libcrypt=; flagcrypt=; true; }; \
	flag=; lib=; cflags="$${flag}" libs="$${lib}" conftest CONFIG_CRYPT_H "$${flag}" "" "$${lib}" \
	    "crypt.h" $(CONFTEST_CRYPT_H) \
	    || true; \
	flag=; lib=; cflags="$${flag} $${flagcrypt}" libs="$${lib} $${libcrypt}" \
	    cctest "crypt_gnu" $(CONFTEST_CRYPT_GNU) \
	    && $(PRINTF) "#define CONFIG_CRYPT_GNU $${binout}\n" >> $(CONFIGINC) || true; \
	cflags='' libs='' cctest "crypt_des_ext" "#include <stdio.h>\n#include <string.h>\n#include <unistd.h>\n#include \"$${PWD}/$(CONFIGINC)\"\n\
	        #if defined(CONFIG_CRYPT_H) && CONFIG_CRYPT_H\n#include <crypt.h>\n#endif\nint main(void) { char *s=\"_1200Salt\";\n\
	        return strncmp(s, crypt(\"toto\", s), strlen(s)); }\n" \
	    && $(PRINTF) '#define CONFIG_CRYPT_DES_EXT 1\n' >> $(CONFIGINC) || true; \
	$(PRINTF) -- "#define CONFIG_FEATURES \"$${configcheck}\"\n" >> $(CONFIGINC); \
	$(RM) -f "$${mytmpfile}"; \
	$(PRINTF) -- "$(NAME): making recursion ($(.TARGETS) $(MAKECMDGOALS))...\n"; \
	$(cmd_TESTBSDOBJ) && cd $(.CURDIR) || true; "$(MAKE)" $(.TARGETS) $(MAKECMDGOALS) && ret=true || ret=false; \
	$(cmd_TESTBSDOBJ) && cd $(.OBJDIR) || true; $(TOUCH) '$(CONFIGMAKE_REC_FILE)'; \
	$${ret}; fi

.gitignore: $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 $(cmd_TESTBSDOBJ) && cd $(.CURDIR) && build=`echo $(.OBJDIR) | $(SED) -e 's|^$(.CURDIR)||'`/ || build=; \
	 { cat .gitignore $(NO_STDERR); \
	   for f in $(LIB) $(JAR) $(GENSRC) $(GENJAVA) $(GENINC) $(SRCINC_Z) $(SRCINC_STR) $(CONFIGMAKE_REC_FILE) \
	            $(BUILDINC) $(BUILDINCJAVA) $(CLANGCOMPLETE) $(CONFIGLOG) $(CONFIGMAKE) $(CONFIGINC) obj/ \
	            `$(TEST) -n "$(BIN)" && echo "$(BIN)" "$(BIN).dSYM" "$(BIN).core" "core" "core.[0-9]*[0-9]" || true` \
	            `echo "$(FLEXLEXER_LNK)" | $(SED) -e 's|^\./||' || true`; do \
	       $(TEST) -n "$${f}" && $(PRINTF) "/$${f}\n" | $(SED) -e 's|^/\./|/|' || true; done; \
	       for f in $${build} '*.o' '*.d' '*.class' '*~' '.*.sw?' '/valgrind_*.log' '*.ali' '$(ADAOBJ:.o=).ad[bs]'; do $(PRINTF) "$${f}\n"; done; \
	 } | $(SORT) | $(UNIQ) > .gitignore; fi

gentags: $(CLANGCOMPLETE)
# CLANGCOMPLETE rule:
# * FIXME bug: regeneration don't preserve order with custom rules
$(CLANGCOMPLETE): $(ALLMAKEFILES) $(BUILDINC) $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 echo "$(NAME): update $@"; \
	 curdir=; moresed="s/^$(DASH)/$(DASH)/"; if $(cmd_TESTBSDOBJ); then \
	     base=`$(BASENAME) "$@"`; $(TEST) -L "$(.OBJDIR)/$${base}" || ln -sf "$(.CURDIR)/$${base}" "$(.OBJDIR)"; \
	     curdir="$(.CURDIR)/"; moresed="s|-I$(.CURDIR)\([^[:space:]]*\)|-I$(.CURDIR)/\1 -I$(.OBJDIR)/\1|g"; \
	 fi; \
	 $(AWK) 'BEGIN { ign=0 } /^[[:space:]]*$(DASH)[[:space:]]*auto-generated[[:space:]]/{ ign = 2 } \
	                         //{ if (ign>0) ign--; else print $$0 }' "$@" $(NOSTDERR) > "$@.tmp" || true; \
	 for d in . $(SUBDIRS); do \
	     short_d="$${d}"; \
	     d="$${curdir}$${d}"; cppflags="$(CPPFLAGS)"; \
	     cppflags="`{ test "$${d}" = "$${curdir}." \
	                  && $(PRINTF) -- "$${cppflags}" | $(SED) -e "$${moresed}" \
	                  || $(GREP) -E -v '^[[:space:]]*$(DASH)' "$${d}/$(CLANGCOMPLETE)" \
			     | $(SED) -e "s%-I\([^/][^[:space:]]*\)%-I$${d}/\1%g"; \
	                } | $(TR) '\n' ' '`"; \
	 $(PRINTF) -- '%s\n' "$${cppflags}" >> "$@.tmp"; \
	 done && $(CAT) "$@.tmp" > "$@" && $(RM) "$@.tmp" || touch "$@"; fi

# to spread 'generic' makefile part to sub-directories
merge-makefile:
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	  $(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; for makefile in `$(FIND) $(SUBDIRS) -name Makefile | $(SORT) | $(UNIQ)`; do \
	     $(GREP) -E -i -B10000 '^[[:space:]]*#[[:space:]]*generic[[:space:]]part' "$${makefile}" > "$${makefile}.tmp" \
	     && $(GREP) -E -i -A10000 '^[[:space:]]*#[[:space:]]*generic[[:space:]]part' Makefile | tail -n +2 >> "$${makefile}.tmp" \
	     && mv "$${makefile}.tmp" "$${makefile}" && echo "merged $${makefile}" || echo "! cannot merge $${makefile}" && $(RM) -f "$${makefile}.tmp"; \
	     file=make-fallback; target="`dirname $${makefile}`/$${file}"; if $(TEST) -e "$${file}" -a -e "$${target}"; then \
	         $(GREP) -E -i -B10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$${target}" > "$${target}.tmp" \
	         && $(GREP) -E -i -A10000 '^[[:space:]]*#[[:space:]]*This program is free software;' "$${file}" | tail -n +2 >> "$${target}.tmp" \
	         && mv "$${target}.tmp" "$${target}" && echo "merged $${target}" && chmod +x "$${target}" || echo "! cannot merge $${target}" && $(RM) -f "$${target}.tmp"; \
	     fi; \
	   done; fi

# To update submodules of submodules if SUBMODROOTDIR is used
#  When a submodule (S) in main project (M) is updated,
#  if this submodule is used in another submodule (A),
#  the index of S in A must be set to same index of S in M.
# This is not perfect as only unintialized submodules are updated (but with submodrootdir, you should not clone with recursive)
# + get recursive submodule list <status>;<sha1>;<folder>
# + Loop on each submodule (recursively)
#   - for each submodule, if it is not initialized, find a more recent repository and update index
subsubmodules:
	@if ! $(cmd_CONFIGMAKE_RECURSE) && $(TEST) -n "$(SUBMODROOTDIR)"; then \
	     mods=`$(GIT) submodule status --recursive | $(SED) -e 's/^[[:space:]]/=;/' -e 's/^\([U+-]\)/\1;/' -e 's/[[:space:]]/;/g'`; \
	     for mod in $${mods}; do \
	         stat=; sha=; dir=; IFSbak=$${IFS}; IFS=';'; for tok in $${mod}; do \
	             { $(TEST) -z "$${stat}" && stat=$${tok}; } \
	             || { $(TEST) -z "$${sha}" && sha=$${tok}; } \
	             || { $(TEST) -z "$${dir}" && dir=$${tok}; }; \
	         done; IFS=$${IFSbak}; \
	         if $(TEST) "$${stat}" = "-"; then \
	             for mod2 in $${mods}; do \
	                 if $(TEST) "$${mod2}" != "$${mod}"; then \
	                     stat2=; sha2=; dir2=; IFSbak2=$${IFS}; IFS=';'; for tok2 in $${mod2}; do \
	                         { $(TEST) -z "$${stat2}" && stat2=$${tok2}; } \
	                         || { $(TEST) -z "$${sha2}" && sha2=$${tok2}; } \
	                         || { $(TEST) -z "$${dir2}" && dir2=$${tok2}; }; \
	                    done; IFS=$${IFSbak2}; \
	                    if $(TEST) "$${sha}" != "$${sha2}" && $(GIT) -C "$${dir2}" show --summary --pretty=oneline "$${sha}" $(NO_STDERR) $(NO_STDOUT); then \
	                        $(PRINTF) "+ setting index of <$${dir}> to index of <$${dir2}> ($${sha2})\n"; \
	                        lsfiles=`$(GIT) -C "$${dir}/.." ls-files -s --full-name $$($(BASENAME) "$${dir}") | $(GREP) $${sha} | $(AWK) '{ print $$1 " " $$4}'` && \
	                        { index=; for tok in $${lsfiles}; do $(TEST) -z "$${index}" && index=$${tok} || moddir=$${tok}; done; } \
	                        && $(TEST) -n "$${index}" -a -n "$${moddir}" && $(GIT) -C "$${dir}/.." update-index --cacheinfo "$${index}" "$${sha2}" "$${moddir}" \
	                        || $(PRINTF) "! cannot set index <$${index}> of <$${dir}>.\n"; break; \
	                    fi; \
	                 fi; \
	             done; \
	         fi; \
	     done; \
	 fi;

#to generate makefile displaying shell command beeing run
debug-makefile:
	@$(cmd_TESTBSDOBJ) && cd "$(.CURDIR)" || true; \
	 sed -e 's/^\(cmd_[[:space:]0-9a-zA-Z_]*\)=/\1= ls $(NAME)\/\1 || time /' Makefile > Makefile.debug \
	 && "$(MAKE)" -f Makefile.debug

#$(VALGRINDSUPP):
#	@$(cmd_TESTBSDOBJ) && $(TEST) -e "$(.CURDIR)/$@" || echo "$(NAME): create $@"
#	@$(TOUCH) '$@'
#	@if $(cmd_TESTBSDOBJ); then $(TEST) -e "$(.CURDIR)/$@" || mv '$@' "$(.CURDIR)"; ln -sf "$(.CURDIR)/$@" .; fi
# Run Valgrind filter output
valgrind: all $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	 $(RM) -R $(BIN).dSYM; \
	 logfile=`$(MKTEMP) ./valgrind_XXXXXX` && $(MV) "$${logfile}" "$${logfile}.log"; logfile="$${logfile}.log"; \
	 $(TEST) -e "$(VALGRINDSUPP)" && vgsupp="--suppressions=$(VALGRINDSUPP)" || vgsupp=; \
	 $(VALGRIND) $(VALGRIND_ARGS) $${vgsupp} --log-file="$${logfile}" $(VALGRIND_RUN) || true; \
	 if $(TEST) -z "$(VALGRIND_MEM_IGNORE_PATTERN)"; then cat "$${logfile}"; else \
 	     $(AWK) '/([0-9]+[[:space:]]+bytes|[cC]onditional jump|uninitialised value)[[:space:]]+/ { if (block == 0) {block=1; blockignore=0;} } \
	         //{ \
	             if (block) { \
			 if (/$(VALGRIND_MEM_IGNORE_PATTERN)/) {blockignore=1;} else {blockstr=blockstr "\n" $$0}; \
	             } else { print $$0; } \
	         } \
		 /^[[:space:]]*=+[0-9]+=+[[:space:]]*$$/ { \
	             if (block) { \
	                 if (!blockignore) print blockstr; \
			 blockstr=""; \
	                 block=0; \
	             } \
	         } \
	         ' $${logfile} > $${logfile%.log}_filtered.log && cat $${logfile%.log}_filtered.log; \
	 fi && echo "* valgrind output in $${logfile} and $${logfile%.log}_filtered.log (will be deleted by 'make distclean')"; \
	 fi

help: $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then $(PRINTF) "%s\n" \
	  "make <target>" \
	  "  target: all, file (main.o, bison.c), ...:" \
	  "  CC           [$(CC)]" \
	  "  CXX          [$(CXX)]" \
	  "  OBJC         [$(OBJC)]" \
	  "  GCJ          [$(GCJ)]" \
	  "  GNAT         [$(GNAT)]" \
	  "  CCLD         [$(CCLD)]" \
	  "  MACROS       [$(MACROS)]" \
	  "  OPTI         [$(OPTI)]" \
	  "  WARN         [$(WARN)]" \
	  "  ARCH         [$(ARCH)]" \
	  "  INCS         [$(INCS)]" \
	  "  INCDIRS      [$(INCDIRS)]" \
	  "  LIBS         [$(LIBS)]" \
	  "  FLAGS_C      [$(FLAGS_C)]" \
	  "  FLAGS_CXX    [$(FLAGS_CXX)]" \
	  "  FLAGS_OBJC   [$(FLAGS_OBJC)]" \
	  "  FLAGS_GCJ    [$(FLAGS_GCJ)]" \
	  "  FLAGS_ADA    [$(FLAGS_ADA)]" \
	  "  ..." \
	  "  to disable propagation to sub-makes: '<make> <variable>=<value> MAKEFLAGS='" \
	  "  for a specific subdir: '<make> <subdir-path>-{build,test,debug,check,clean,distclean,...}'" \
	  "" \
	  "make clean / distclean / configure" \
	  "  clean intermediary files / clean generated files / reconfigure" \
	  "" \
	  "make info" \
	  "  display makefile variables" \
	  "" \
	  "make debug / make test" \
	  "  enable debug/tests compile flags and rebuild" \
	  "" \
	  "make valgrind" \
	  "  run valgrind with:" \
	  "   VALGRIND                    [$(VALGRIND)]" \
	  "   VALGRIND_ARGS               [$(VALGRIND_ARGS)]" \
	  "   VALGRIND_RUN                [$(VALGRIND_RUN)]" \
	  "   VALGRIND_MEM_IGNORE_PATTERN [$(VALGRIND_MEM_IGNORE_PATTERN)]" \
	  "   VALGRINDSUPP                [$(VALGRINDSUPP)]" \
	  "" \
	  "make merge-makefile" \
	  "  merge the common part of Makefile with SUBDIRS:" \
	  "   SUBDIRS [$(SUBDIRS)]" \
	  "" \
	  "make install" \
	  "  PREFIX         [$(PREFIX)]" \
	  "  INSTALL_FILES  [$(INSTALL_FILES)]" \
	  "" \
	  "make check"; \
	  $(PRINTF) -- '  CHECK_RUN      [$(CHECK_RUN:S/'/'"'"'/g)$(subst ','"'"',$(CHECK_RUN))]\n'; \
	  $(PRINTF) '%s\n' \
	  "" \
	  "make .gitignore" \
	  "" \
	  "make subsubmodules" \
	  "  update index of sub-submodules which are not populated when SUBMODROOTDIR is used" \
	  "" \
	  "make dist [DIST_NO_SCM=]" \
	  "  DISTDIR          [ $(DISTDIR) ]" \
	  ""; fi

info: $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then $(PRINTF) "%s\n" \
	  "NAME             : $(NAME)" \
	  "UNAME_SYS        : $(UNAME_SYS)  [`uname -a`]" \
	  "MAKE             : $(MAKE)  [`\"$(MAKE)\" --version $(NO_STDERR) | $(HEADN1) || "$(MAKE)" -V $(NO_STDERR) | $(HEADN1)`]" \
	  "SHELL            : $(SHELL)" \
	  "FIND             : $(FIND)  [`$(FIND) --version $(NO_STDERR) | $(HEADN1) || $(FIND) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "AWK              : $(AWK)  [`$(AWK) --version $(NO_STDERR) | $(HEADN1) || $(AWK) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "GREP             : $(GREP)  [`$(GREP) --version $(NO_STDERR) | $(HEADN1) || $(GREP) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "SED              : $(SED)  [`$(SED) --version $(NO_STDERR) | $(HEADN1) || $(SED) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "TAR              : $(TAR)  [`$(TAR) --version $(NO_STDERR) | $(HEADN1) || $(TAR) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "DATE             : $(DATE)  [`$(DATE) --version $(NO_STDERR) | $(HEADN1) || $(DATE) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "PKGCONFIG        : $(PKGCONFIG)" \
	  "CC               : $(CC)  [`$(CC) --version $(NO_STDERR) | $(HEADN1)`]" \
	  "CXX              : $(CXX)  [`$(CXX) --version $(NO_STDERR) | $(HEADN1)`]" \
	  "OBJC             : $(OBJC)" \
	  "GCJ              : $(GCJ)  [`$(GCJ) --version $(NO_STDERR) | $(HEADN1) || true`]" \
	  "GCJH             : $(GCJH)" \
	  "GNAT             : $(GNAT)  [`$(GNAT) --version $(NO_STDERR) | $(HEADN1) || true`]" \
	  "GNATC            : $(GNATC)  [`$(GNATC) --version $(NO_STDERR) | $(HEADN1) || true`]" \
	  "GNATBIND         : $(GNATBIND)  [`$(GNATBIND) --version $(NO_STDERR) | $(HEADN1) || true`]" \
	  "GNATMAKE         : $(GNATMAKE)  [`$(GNATMAKE) --version $(NO_STDERR) | $(HEADN1) || true`]" \
	  "JAVA             : $(JAVA)  [`$(JAVA) -version $(STDERR_TO_OUT) | $(HEADN1) || true`]" \
	  "CPP              : $(CPP)" \
	  "CCLD             : $(CCLD)" \
	  "YACC             : $(YACC)  [`$(YACC) --version $(NO_STDERR) | $(HEADN1) || $(YACC) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "BISON3           : $(BISON3)  [`$(BISON3) --version $(NO_STDERR) | $(HEADN1) || $(BISON3) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "LEX              : $(LEX)  [`$(LEX) --version $(NO_STDERR) | $(HEADN1) || $(LEX) -V $(NO_STDERR) | $(HEADN1)`]" \
	  "CFLAGS           : $(CFLAGS)" \
	  "CXXFLAGS         : $(CXXFLAGS)" \
	  "OBJCFLAGS        : $(OBJCFLAGS)" \
	  "JFLAGS           : $(JFLAGS)" \
	  "ADAFLAGS         : $(ADAFLAGS)" \
	  "CPPFLAGS         : $(CPPFLAGS)" \
	  "LDFLAGS          : $(LDFLAGS)" \
	  "YFLAGS           : $(YFLAGS)" \
	  "YCXXFLAGS        : $(YCXXFLAGS)" \
	  "YJFLAGS          : $(YJFLAGS)" \
	  "LFLAGS           : $(LFLAGS)" \
	  "LCXXFLAGS        : $(LCXXFLAGS)" \
	  "LJFLAGS          : $(LJFLAGS)" \
	  "SRCDIR           : $(SRCDIR)" \
	  "DISTDIR          : $(DISTDIR)" \
	  "BUILDDIR         : $(BUILDDIR)" \
	  "PREFIX           : $(PREFIX)" \
	  "CONFIG_CHECK     : $(CONFIG_CHECK)" \
	  "SUBMODROOTDIR    : $(SUBMODROOTDIR)" \
	  "SUBDIRS          : $(SUBDIRS)" \
	  "SUBLIBS          : $(SUBLIBS)" \
	  "LIBS             : $(LIBS)" \
	  "BIN              : $(BIN)" \
	  "LIB              : $(LIB)" \
	  "METASRC          : $(METASRC)" \
	  "GENINC           : $(GENINC)" \
	  "GENSRC           : $(GENSRC)" \
	  "GENJAVA          : $(GENJAVA)" \
	  "INCLUDES         : $(INCLUDES)" \
	  "SRC              : $(SRC)" \
	  "JAVASRC          : $(JAVASRC)" \
	  "OBJ              : $(OBJ)" \
	  "CLASSES          : $(CLASSES)"; \
	  fi
rinfo: info $(CONFIGMAKE)
	@if ! $(cmd_CONFIGMAKE_RECURSE); then \
	   old="$${PWD}"; for d in $(SUBDIRS); do recdir="$${d}"; rectarget=rinfo; $(RECURSEMAKEARGS); \
	     cd "$${recdir}" && "$(MAKE)" $${recargs} rinfo; cd "$${old}"; done; fi


