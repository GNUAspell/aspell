CXXFLAGS = -O -g
CFLAGS   = -O -g
export ASPELL = ${CURDIR}/inst/bin/aspell
export PREZIP = ${CURDIR}/inst/bin/prezip-bin
MAKE := ${MAKE} "CXXFLAGS=${CXXFLAGS}" "CFLAGS=${CFLAGS}"

all:: prep sanity filter-test suggest
	cat test-res

prep:: inst/bin/aspell inst/lib/aspell-0.60/en.multi tmp
	rm -f test-res

tmp:
	mkdir tmp

sanity:: prep
	./sanity
	echo "all ok (sanity)" >> test-res

filter-test:: prep
	./filter-test "${ASPELL}" < markdown.dat
	echo "all ok (markdown filter-test)" >> test-res

inst/bin/aspell: build/Makefile phony
	$(MAKE) -C build install

inst/lib/aspell-0.60/en.multi: inst/bin/aspell aspell6-en-2018.04.16-0.tar.bz2
	tar xf aspell6-en-2018.04.16-0.tar.bz2
	cp en_phonet.dat aspell6-en-2018.04.16-0
	cd aspell6-en-2018.04.16-0 && ./configure
	$(MAKE) -C aspell6-en-2018.04.16-0 install
	cp en.prepl inst/lib/aspell-0.60
	echo 'add en_US.multi' > inst/lib/aspell-0.60/en_US-w_repl.multi
	echo 'add en.prepl' >> inst/lib/aspell-0.60/en_US-w_repl.multi

build/Makefile:
	mkdir -p build
	cd build && ../../configure --enable-maintainer-mode --disable-shared --disable-pspell-compatibility --prefix="${CURDIR}/inst"

aspell6-en-2018.04.16-0.tar.bz2:
	curl -O ftp://ftp.gnu.org/gnu/aspell/dict/en/aspell6-en-2018.04.16-0.tar.bz2

clean::
	rm -rf inst build tmp aspell6-en-2018.04.16-0

suggest.mk: suggest/mkmk
	suggest/mkmk > suggest.mk

include suggest.mk

.PHONY: phony
phony:
