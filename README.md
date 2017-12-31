
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
- Linux: slitaz 4 2.6.37, ubuntu 12.04 3.11.0
- OSX 10.11.6
- OpenBSD 5.5
- FreeBSD 11.1

## Compilation
Make sure you clone the repository with '--recursive' option.
    $ git clone --recursive https://github.com/vsallaberry/vlib

Just type:
    $ make

If the Makefile cannot be parsed by 'make', try:
    $ ./make-fallback

Most of utilities used in Makefile are defined in variables and can be changed
with something like 'make SED=gsed TAR=gnutar' (or ./make-fallback SED=...)

To See how make understood the Makefile, you can type:
    $ make info # ( or ./make-fallback info)

## Contact
[vsallaberry@gmail.com]
<https://github.com/vsallaberry/vlib>

## License
GPLv3 or later. See LICENSE file.

CopyRight: Copyright (C) 2017 Vincent Sallaberry

