#!/bin/bash -e
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME=softgre

(test -f $srcdir/configure.ac ) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

rm -rf autom4te.cache 
rm -f autoscan.log config.log config.status configure

set -fx
autoheader
aclocal
autoconf
autoreconf -fvi
automake --add-missing
./configure
set +fx

