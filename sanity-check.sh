#!/bin/sh

set -e

make -C test clean
make -j2 -C test sanity

