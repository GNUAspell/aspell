#!/bin/sh

set -e
set -x

SRCDIR="`pwd`"
OBJDIR="`mktemp -d`"
cd "$OBJDIR"
"$SRCDIR/configure" --disable-shared --disable-pspell-compatibility \
                    --prefix="$OBJDIR/inst/" CFLAGS='-g -O -Wall' CXXFLAGS='-g -O -Wall'

make -j2
make install

export PATH=$OBJDIR/inst/bin:$PATH

DICT=aspell6-en-2016.11.20-0
curl ftp://ftp.gnu.org/gnu/aspell/dict/en/$DICT.tar.bz2 | tar xfj -
cd $DICT
./configure
make install

if echo 'color' | aspell -d en_US -a | fgrep '*'; then
    echo "pass"
else
    echo "fail"
    exit 1
fi

if echo 'colour' | aspell -d en_US -a | fgrep '& colour' | fgrep 'color'; then
    echo "pass"
else
    echo "fail"
    exit 1
fi

aspell -d en_US dump master | aspell -d en_US list > incorrect
if [ -e incorrect -a ! -s incorrect ]; then
    echo "pass"
else
    echo "fail:"
    cat incorrect
    exit 1
fi

cd "$SRCDIR"
rm -r "$OBJDIR"
