CXXFLAGS = -O -g
CFLAGS   = -O -g
export ASPELL = ${CURDIR}/inst/bin/aspell
export PREZIP = ${CURDIR}/inst/bin/prezip-bin
MAKE := ${MAKE} "CXXFLAGS=${CXXFLAGS}" "CFLAGS=${CFLAGS}"

all:: prep sanity

prep:: inst/bin/aspell inst/lib/aspell-0.60/en.multi tmp

tmp:
	mkdir tmp

sanity:: prep
	./sanity

inst/bin/aspell: build/Makefile phony
	$(MAKE) -C build install

inst/lib/aspell-0.60/en.multi: inst/bin/aspell aspell6-en-2018.04.16-0.tar.bz2
	tar xf aspell6-en-2018.04.16-0.tar.bz2
	cd aspell6-en-2018.04.16-0 && ./configure
	$(MAKE) -C aspell6-en-2018.04.16-0 install

build/Makefile:
	mkdir -p build
	cd build && ../../configure --enable-maintainer-mode --disable-shared --disable-pspell-compatibility --prefix="${CURDIR}/inst"

aspell6-en-2018.04.16-0.tar.bz2:
	curl -O ftp://ftp.gnu.org/gnu/aspell/dict/en/aspell6-en-2018.04.16-0.tar.bz2

clean::
	rm -rf inst build tmp aspell6-en-2018.04.16-0

.PHONY: phony
phony:
