#!/bin/sh
# Run this to generate all the initial makefiles, etc.

libtoolize --force --copy
intltoolize --force --copy
gtkdocize --copy
aclocal
autoconf
autoheader
automake --add-missing --copy --foreign

YELP=`which yelp-build 2>/dev/null`
if test -z "$YELP"; then
  echo "*** Note: yelp-tools not found; help documentation will not be built ***"
fi

if [ -z "$NOCONFIGURE" ]; then
    ./configure $@
fi
