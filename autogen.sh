#!/bin/sh

set -x
aclocal
autoheader
libtoolize --force --copy --automake
automake --add-missing --foreign --copy
autoconf
