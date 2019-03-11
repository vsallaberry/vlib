
## vlib
--------------

* [Overview](#overview)
* [System Requirements](#system-requirements)
* [Compilation](#compilation)
* [Integration](#integration)
* [Contact](#contact)
* [License](#license)

## Overview
**vlib** is a simple C library for simple programs including log, command-line, hash, list, avltree.

## System requirements
- A somewhat capable compiler (gcc/clang), 
- make (GNU,BSD)
- sh (sh/bash/ksh)
- coreutils (awk,grep,sed,date,touch,head,printf,which,find,test,...)

This is not an exhaustive list but the list of systems on which it has been built:
- Linux: slitaz 4 2.6.37, ubuntu 12.04 3.11.0, debian9.
- OSX 10.11.6
- OpenBSD 5.5
- FreeBSD 11.1

## Compilation

### Cloning **vlib** repository
If you are using SUBMODROOTDIR Makefile's feature (RECOMMANDED, see [submodules](#using-git-submodules)):  
    $ git clone https://github.com/vsallaberry/vlib.git  
    $ git submodule update --init  

Otherwise:  
    $ git clone --recursive https://vsallaberry/vlib.git  

### Building
Just type:  
    $ make # (or 'make -j3' for SMP)  

If the Makefile cannot be parsed by 'make', try:  
    $ ./make-fallback  

### General information
An overview of Makefile rules can be displayed with:  
    $ make help  

Most of utilities used in Makefile are defined in variables and can be changed
with something like 'make SED=gsed TAR=gnutar' (or ./make-fallback SED=...)  

To See how make understood the Makefile, you can type:  
    $ make info # ( or ./make-fallback info)  

When making without version.h created (not the case for this repo), some old
bsd make can stop. Just type again '$ make' and it will be fine.  

When you link **vlib** with a program, you need pthread (-lpthread), zlib 
(CONFIG\_ZLIB, -lz), ncurses (CONFIG\_CURSES, -lncurses), and on linux: rt (-lrt).

### Using git submodules
When your project uses git submodules, it is a good idea to group
submodules in a common folder, here, 'ext'. Indeed, instead of creating a complex tree
in case the project (A) uses module B (which uses module X) and module C (which uses module X),
X will not be duplicated as all submodules will be in ext folder.  

You need to set the variable SUBMODROOTDIR in your program's Makefile to indicate 'make'
where to find submodules (will be propagated to SUBDIRS).  

As SUBDIRS in Makefile are called with SUBMODROOTDIR propagation, currently you cannot use 
'make -C <subdir>' (or make -f <subdir>/Makefile) but instead you can use 'make <subdir>',
 'make {check,debug,test,install,...}-<subdir>', as <subdir>, check-<subdir>, ... are
defined as targets.  

When SUBMODROOTDIR is used, submodules of submodules will not be populated as they are
included in root project. The command `make subsubmodules` will update index of non-populated 
sub-submodules to the index used in the root project.

You can let SUBMODROOTDIR empty if you do not want to group submodules together.

## Integration
This part describes the way to integrate and use **vlib** in another project.

### SubModule creation
Simplest way is to add **vlib** as a git submodule of your project, in for example ext folder:   
    $ git submodule add -b master https://github.com/vsallaberry/vlib.git ext/vlib  

Populate the submodule with:  
    $ git submodule update --init  

If you are using ssh and github, and need to push, you can do:  
    $ git remote set-url --push origin git@github.com:vsallaberry/vlib.git  

### Referencing **vlib** in the program's Makefile
Then you will reference **vlib** in the program's Makefile (can be a copy of **vlib** Makefile),
so as make can be run on **vlib** (SUBDIRS), ensuring right BIN dependencies and linking (SUBLIBS),
and using appropriate include dirs for gcc '-I<IncludeDir>' (INCDIRS):  
    # If you want to Group all submodules (including submodules of submodules) in one folder:
    SUBMODROOTDIR   = ext
    LIBVLIBDIR      = $(SUBMODROOTDIR)/vlib
    # If you want to keep the submodules tree as it is (with possible submodule duplicates):  
    #SUBMODROOTDIR   = 
    #LIBVLIBDIR      = ext/vlib
    SUBDIRS         = $(LIBVLIBDIR)
    SUBLIBS         = $(LIBVLIBDIR)/libvlib.a
    INCDIRS         = $(LIBVLIBDIR)/include
    CONFIG_CHECK    = zlib ncurses

WORK-IN-PROGRESS...

## Contact
[vsallaberry@gmail.com]  
<https://github.com/vsallaberry/vlib>

## License
GPLv3 or later. See LICENSE file.
Copyright: Copyright (C) 2017-2019 Vincent Sallaberry
**vlib** was first created and first published in 2017 by Vincent Sallaberry.

