
## vlib
--------------

* [Overview](#overview)
* [System Requirments](#systemrequirments)
* [Compilation](#compilation)
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

When you link **vlib** with a program, you need pthread (-lpthread).

## Contact
[vsallaberry@gmail.com]  
<https://github.com/vsallaberry/vlib>

## License
GPLv3 or later. See LICENSE file.

CopyRight: Copyright (C) 2017-2018 Vincent Sallaberry

