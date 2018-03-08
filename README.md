
## vlib
--------------

* [Overview](#overview)
* [System Requirments](#systemrequirments)
* [Compilation](#compilation)
* [Integration](#integration)
* [Contact](#contact)
* [License](#license)

## Overview
**vlib** is a simple C library for simple programs including log, command-line, hash, list.

## System requirements
- A somewhat capable compiler (gcc/clang), make (GNU,BSD), sh (sh/bash/ksh)
  and coreutils (awk,grep,sed,date,touch,head,printf,which,find,test,...)

This is not an exhaustive list but the list of systems on which it has been built:
- Linux: slitaz 4 2.6.37, ubuntu 12.04 3.11.0, debian9.
- OSX 10.11.6
- OpenBSD 5.5
- FreeBSD 11.1

## Compilation
Make sure you clone the repository with '--recursive' option.  
    $ git clone --recursive https://github.com/vsallaberry/vlib

Just type:  
    $ make # (or 'make -j3' for SMP)

If the Makefile cannot be parsed by 'make', try:  
    $ ./make-fallback

Most of utilities used in Makefile are defined in variables and can be changed
with something like 'make SED=gsed TAR=gnutar' (or ./make-fallback SED=...)

To See how make understood the Makefile, you can type:  
    $ make info # ( or ./make-fallback info)

When making without version.h created (not the case for this repo), some old
bsd make can stop. Just type again '$ make' and it will be fine.

When you link **vlib** with a program, you need pthread (-lpthread), 
and on linux, rt, dl (-lrt -ldl).

## Integration
This part describes the way to integrate and use **vlib** in another project.
Simplest way is to add **vlib** as a git submodule of your project:  
$ git submodule add -b master https://github.com/vsallaberry/vlib ext/vlib  

where ext/vlib is the path where vlib will be fetched. It is a good idea to group
submodules in a common folder, here, 'ext'.

Then you will reference **vlib** in the main Makefile (can be a copy of **vlib** Makefile),
so as make can be run on **vlib** (SUBDIRS), BIN dependencies and linking are right (SUBLIBS),
and include dirs for 'gcc -I' are reconized(INCDIRS):  
    LIBVLIBDIR      = ext/vlib
    SUBDIRS         = $(LIBVLIBDIR)
    SUBLIBS         = $(LIBVLIBDIR)/libvlib.a
    INCDIRS         = $(LIBVLIBDIR)/include

WORK-IN-PROGRESS...

## Contact
[vsallaberry@gmail.com]  
<https://github.com/vsallaberry/vlib>

## License
GPLv3 or later. See LICENSE file.
Copyright: Copyright (C) 2017-2018 Vincent Sallaberry
**vlib** was first created and first published in 2017 by Vincent Sallaberry.

