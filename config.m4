dnl $Id$
dnl config.m4 for extension pplib

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

PHP_ARG_ENABLE(pplib, whether to enable pplib support,
dnl Make sure that the comment is aligned:
[  --enable-pplib            Enable pplib support])

if test "$PHP_PPLIB" != "no"; then
  dnl Write more examples of tests here...

  PHP_NEW_EXTENSION(pplib, pplib.c, $ext_shared)
fi
