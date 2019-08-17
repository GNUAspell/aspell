CXXFLAGS = -O -g
CFLAGS   = -O -g -Wall
export ASPELL = ${CURDIR}/inst/bin/aspell
export PREZIP = ${CURDIR}/inst/bin/prezip-bin
MAKE := ${MAKE} "CXXFLAGS=${CXXFLAGS}" "CFLAGS=${CFLAGS}"
EXTRA_CONFIG_FLAGS =

all:: prep sanity filter-test suggest wide cxx_warnings
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
	cp en.dat en.prepl en_repl.dat en_phonet.dat inst/lib/aspell-0.60
	echo 'add en_US.multi' > inst/lib/aspell-0.60/en_US-w_repl.multi
	echo 'add en.prepl' >> inst/lib/aspell-0.60/en_US-w_repl.multi

build/Makefile:
	mkdir -p build
	cd build && ../../configure --enable-maintainer-mode --disable-shared --disable-pspell-compatibility --prefix="${CURDIR}/inst" $(EXTRA_CONFIG_FLAGS)

aspell6-en-2018.04.16-0.tar.bz2:
	curl -O ftp://ftp.gnu.org/gnu/aspell/dict/en/aspell6-en-2018.04.16-0.tar.bz2

clean::
	rm -rf inst build tmp aspell6-en-2018.04.16-0

suggest.mk: suggest/mkmk
	suggest/mkmk > suggest.mk

include suggest.mk

wide:: wide_test_invalid wide_test_invalid-but_ok wide_test_valid

wide_test_invalid: wide_test_invalid.c prep
	$(CC) $(CFLAGS) -Iinst/include -c $< -o tmp/$@.o
	$(CXX) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
	./$@ 2> tmp/$@.log || true
	fgrep -q 'Null-terminated wide-character strings unsupported when used this way.' tmp/$@.log

wide_test_valid: wide_test_valid.c prep
	$(CC) $(CFLAGS) -Iinst/include -c $< -o tmp/$@.o
	$(CXX) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
	./$@

wide_test_invalid-but_ok: wide_test_invalid.c prep
	$(CC) $(CFLAGS) -DASPELL_ENCODE_SETTING_SECURE -Iinst/include -c $< -o tmp/$@.o
	$(CXX) tmp/$@.o inst/lib/libaspell.a -ldl -o $@
	./$@

cxx_warnings:  cxx_warnings_test.cpp prep
	$(CXX) $(CXXFLAGS) -Wall -Wconversion -Werror -Iinst/include -c $<

## this target is only true if configured with --enable-sloppy-null-term-strings
wide_test_invalid-but_allowed: wide_test_invalid.c prep
	$(CC) $(CFLAGS) -Iinst/include -c $< inst/lib/libaspell.a -ldl -o tmp/obj.o
	$(CXX) tmp/obj.o inst/lib/libaspell.a -ldl -o $@
	./$@

.PHONY: phony
phony:
