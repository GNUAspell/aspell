#!/bin/bash -eu

# This script is called by the oss-fuzz main project when compiling the fuzz
# targets. This script is regression tested by travisoss.sh.

# Save off the current folder as the build root.
export BUILD_ROOT=$PWD
export DICT_DIR=$OUT/dict

if [[ ! -d ${DICT_DIR} ]]
then
    mkdir -p ${DICT_DIR}
fi

echo "CC: $CC"
echo "CXX: $CXX"
echo "LIB_FUZZING_ENGINE: $LIB_FUZZING_ENGINE"
echo "CFLAGS: $CFLAGS"
echo "CXXFLAGS: $CXXFLAGS"
echo "OUT: $OUT"

export CFLAGS="$CFLAGS -g"
export CXXFLAGS="$CXXFLAGS -g"

export MAKEFLAGS+="-j$(nproc)"

# Install dependencies
apt-get -y install \
    automake \
    autopoint \
    bzip2 \
    gettext \
    libtool \
    texinfo \
    wget

# Compile the fuzzer.

export LIBS=-lpthread

./autogen
./configure \
    --enable-static \
    --enable-ossfuzzers \
    --disable-pspell-compatibility \
    --prefix=$OUT/deps \
    --enable-pkgdatadir=$DICT_DIR \
    --enable-pkglibdir=$DICT_DIR

make V=1
make install

# Copy the fuzzer and corpus to the output directory.
cp -v ossfuzz/aspell_fuzzer $OUT/
zip $OUT/aspell_fuzzer_seed_corpus.zip ossfuzz/aspell_fuzzer_corpus/*

# Install some dictionaries!
export PATH=$OUT/deps/bin:$PATH

install_language() {
    LANG=$1
    DICT=$2
    pushd /tmp
    wget -O- https://ftp.gnu.org/gnu/aspell/dict/${LANG}/$DICT.tar.bz2 | tar xfj -
    pushd $DICT
    ./configure
    make install
    popd
    popd
}

install_language en aspell6-en-2016.11.20-0
install_language pt_BR aspell6-pt_BR-20131030-12-0
