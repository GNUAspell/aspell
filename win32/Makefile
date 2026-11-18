# aspell-0.50.2/win32/Makefile / 02-06-10	vim600:fdm=marker
# Author : Thorsten Maerz <torte@netztorte.de>	vim:ts=32
#
# Standalone makefile for building GNU Aspell-0.50 using MinGW / GCC3.2
#
################################################################################
#
# DEBUGVERSION=1: uncomment to include debugging symbols
#DEBUGVERSION=1

# CURSESDIR=<path>: (optional) location of pdcurses headers and libs
#CURSESDIR=../../pdcurses

# MSVCLIB=LIB.EXE: (optional) build import .lib for MS VC++ (needs ms lib.exe)
#MSVCLIB=LIB.EXE

# TARGET=<path-and-prefix>: (optional) prefix for cross compilation
#TARGET=i586-mingw32msvc-

# USE_GCC_2X=1: use gcc2.x (not working)
#USE_GCC_2X=1

# WIN32_RELOCATABLE=1: uncomment to have user files in <prefix>
WIN32_RELOCATABLE=1

################################################################################

prefix=c:/aspell
pkglibdir=$(prefix)/dict
pkgdatadir=$(prefix)/data
sysconfdir=$(prefix)

top_srcdir=..
installdir=$(prefix)

################################################################################
# {{{1	compiler settings

ifdef DEBUGVERSION
	DEBUGFLAG=-g
else
	OPTIMIZATION=-O3
endif

ifdef CURSESDIR
	CURSES_INCLUDE=-I$(CURSESDIR)
	CURSES_LIB=$(CURSESDIR)/panel.a $(CURSESDIR)/pdcurses.a
	CURSES_DEFINES=-DHAVE_LIBCURSES -DCURSES_NON_POSIX
endif

ifdef USE_GCC_2X
	# gcc-2.x: -fnative-struct
	BITFIELD=-fnative-struct
	GCC2LIBS=-lmingwex -liberty
else
	# gcc-3.x: -mms-bitfields
	BITFIELD=-mms-bitfields
endif

ifdef WIN32_RELOCATABLE
	WIN32RELOC=-DENABLE_WIN32_RELOCATABLE
endif

# cygwin: built native w32 code
#NOCYGWIN=-mno-cygwin

################################################################################

LIBVERSION=15
ASPELLVERSION=0-60-8-1

CCOMPILER=gcc
CPPCOMPILER=g++
CC=$(TARGET)$(CCOMPILER)
CXX=$(TARGET)$(CPPCOMPILER)
AR=$(TARGET)ar
ARFLAGS=rcv
DEFINES=$(CURSES_DEFINES) $(WIN32RELOC)
EXTRALIBS=
RESOURCE=
RESCOMP=$(TARGET)windres
STRIP=$(TARGET)strip
NATIVE_CC=$(NATIVE)$(CCOMPILER)
NATIVE_CXX=$(NATIVE)$(CPPCOMPILER)

VPATH=	$(top_srcdir)/common:\
	$(top_srcdir)/modules/filter:\
	$(top_srcdir)/modules/speller/default:\
	$(top_srcdir)/modules/tokenizer:\
	$(top_srcdir)/lib:\
	$(top_srcdir)/gen:\
	$(top_srcdir)/win32/gen:\
	$(top_srcdir)/prog
INCLUDES = \
	-I. \
	-I${top_srcdir}/common \
	-I$(top_srcdir)/interfaces/cc \
	-I$(top_srcdir)/modules/filter \
	-I$(top_srcdir)/modules/speller/default \
	-I$(top_srcdir)/modules/tokenizer \
	-I$(top_srcdir)/gen \
	-I$(top_srcdir)/win32 \
	-I$(top_srcdir)/win32/gen \
	$(CURSES_INCLUDE)

FLAGS=$(DEBUGFLAG) $(BITFIELD) $(NOCYGWIN) $(OPTIMIZATION)
CFLAGS=$(FLAGS) $(DEFINES) $(INCLUDES)
CXXFLAGS=$(CFLAGS)
SUPPORTLIBS=
LIBS=-lstdc++ $(GCC2LIBS)

################################################################################
# {{{1	targets
#
ifndef DEBUGVERSION 
all_strip:	all strip
endif

all: \
	aspell_common_a \
	aspell_filter_standard_a \
	aspell_speller_default_a \
	aspell_tokenizer_a \
	aspell_dll \
	pspell_dll \
	aspell_exe \
	word_list_compress_exe \
	msvc_lib
clean: \
	aspell_common_clean \
	aspell_filter_standard_clean \
	aspell_speller_default_clean \
	aspell_tokenizer_clean \
	aspell_dll_clean \
	pspell_dll_clean \
	aspell_exe_clean \
	word_list_compress_clean \
	msvc_lib_clean \
	regfile_clean
	
################################################################################
# {{{1	aspell-common
#
ASPELL_COMMON_A_NAME=aspell-common-$(ASPELLVERSION).a

aspell_common_OBJECTS = \
	cache.o \
	string.o \
	getdata.o \
	itemize.o \
	file_util.o \
	string_map.o \
	string_list.o \
	config.o \
	version.o \
	posib_err.o \
	errors.o \
	error.o \
	fstream.o \
	iostream.o \
	info.o \
	can_have_error.o \
	convert.o \
	tokenizer.o \
	speller.o \
	document_checker.o \
	filter.o \
	objstack.o \
	strtonum.o \
	gettext_init.o \
	file_data_util.o


cache.o:	cache.cpp
string.o:	string.cpp
getdata.o:	getdata.cpp
itemize.o:	itemize.cpp
file_util.o:	file_util.cpp
string_map.o:	string_map.cpp
string_list.o:	string_list.cpp
config.o:	config.cpp
version.o:	version.cpp
posib_err.o:	posib_err.cpp
errors.o:	errors.cpp
error.o:	error.cpp
fstream.o:	fstream.cpp
iostream.o:	iostream.cpp
info.o:	info.cpp
can_have_error.o:	can_have_error.cpp
convert.o:	convert.cpp
tokenizer.o:	tokenizer.cpp
speller.o:	speller.cpp
document_checker.o:	document_checker.cpp
filter.o:	filter.cpp
objstack.o:	objstack.cpp
strtonum.o:	strtonum.cpp
gettext_init.o:	gettext_init.cpp
file_data_util.o:	file_data_util.cpp


gen/dirs.h:
	perl ${top_srcdir}/gen/mk-dirs_h.pl ${prefix} ${pkgdatadir} ${pkglibdir}  ${sysconfdir} >  ${top_srcdir}/gen/dirs.h

${top_srcdir}/common/config.cpp: gen/dirs.h

aspell_common_a: $(ASPELL_COMMON_A_NAME) 
$(ASPELL_COMMON_A_NAME): gen/dirs.h $(aspell_common_OBJECTS)
	$(AR) $(ARFLAGS) \
	$@ \
	$(aspell_common_OBJECTS)
aspell_common_clean:
	-rm $(aspell_common_OBJECTS) \
	$(ASPELL_COMMON_A_NAME) \
	${top_srcdir}/gen/dirs.h

#######################################################################
# {{{1	aspell-filter-standard
#
ASPELL_FILTER_STANDARD_A_NAME=aspell-filter-standard.a
aspell_filter_standard_EXTRALIBS = \
	$(ASPELL_COMMON_A_NAME)

aspell_filter_standard_OBJECTS = \
	url.o \
	email.o \
	tex.o \
	sgml.o \
	context.o \
	nroff.o \
	texinfo.o \

url.o:	url.cpp
email.o:	email.cpp
tex.o:	tex.cpp
sgml.o:	sgml.cpp
context.o:	context.cpp
nroff.o:	nroff.cpp
texinfo.o:	texinfo.cpp

aspell_filter_standard_a: $(ASPELL_FILTER_STANDARD_A_NAME)
$(ASPELL_FILTER_STANDARD_A_NAME): $(aspell_filter_standard_OBJECTS)
	$(AR) $(ARFLAGS) \
	$@ \
	$+
aspell_filter_standard_clean:
	-rm $(aspell_filter_standard_OBJECTS) \
	 $(ASPELL_FILTER_STANDARD_A_NAME)

#######################################################################
# {{{1	aspell-speller-default
#
ASPELL_SPELLER_DEFAULT_A_NAME=aspell-speller-default.a
aspell_speller_default_EXTRALIBS = \
	$(ASPELL_COMMON_A_NAME)

aspell_speller_default_OBJECTS = \
	readonly_ws.o \
	suggest.o \
	data.o \
	multi_ws.o \
	phonetic.o \
	writable.o \
	speller_impl.o \
	phonet.o \
	typo_editdist.o \
	editdist.o \
	primes.o \
	language.o \
	leditdist.o \
	affix.o \

readonly_ws.o:	readonly_ws.cpp
suggest.o:	suggest.cpp
data.o:	data.cpp
multi_ws.o:	multi_ws.cpp
phonetic.o:	phonetic.cpp
writable.o:	writable.cpp
speller_impl.o:	speller_impl.cpp
phonet.o:	phonet.cpp
typo_editdist.o:	typo_editdist.cpp
editdist.o:	editdist.cpp
primes.o:	primes.cpp
language.o:	language.cpp
leditdist.o:	leditdist.cpp
affix.o:	affix.cpp

aspell_speller_default_a: $(ASPELL_SPELLER_DEFAULT_A_NAME)
$(ASPELL_SPELLER_DEFAULT_A_NAME): $(aspell_speller_default_OBJECTS)
	$(AR) $(ARFLAGS) \
	$@ \
	$+
aspell_speller_default_clean:
	-rm $(aspell_speller_default_OBJECTS) \
	$(ASPELL_SPELLER_DEFAULT_A_NAME)

################################################################################
# {{{1	aspell-tokenizer
#
ASPELL_TOKENIZER_A_NAME=aspell-tokenizer.a
aspell_tokenizer_EXTRALIBS = \
	$(ASPELL_COMMON_A_NAME)

aspell_tokenizer_OBJECTS = \
	basic.o \

basic.o:	basic.cpp

aspell_tokenizer_a: $(ASPELL_TOKENIZER_A_NAME)
$(ASPELL_TOKENIZER_A_NAME): $(aspell_tokenizer_OBJECTS)
	$(AR) $(ARFLAGS) \
	$@ \
	$+
aspell_tokenizer_clean:
	-rm $(aspell_tokenizer_OBJECTS) \
	$(ASPELL_TOKENIZER_A_NAME)

################################################################################
# {{{1	aspell-dll
#
ASPELL_DLL_NAME=aspell-$(LIBVERSION).dll
ASPELL_DLL_LIB_NAME=libaspell-$(LIBVERSION)-dll.lib
ASPELL_DLL_DEF_NAME=aspell-$(LIBVERSION).def

aspell_dll_EXTRALIBS = \
	$(ASPELL_FILTER_STANDARD_A_NAME) \
	$(ASPELL_SPELLER_DEFAULT_A_NAME) \
	$(ASPELL_TOKENIZER_A_NAME) \
	$(ASPELL_COMMON_A_NAME)
aspell_dll_LDFLAGS = \
	-Wl,--export-all-symbols \
	-Wl,--compat-implib \
	-Wl,--out-implib=$(ASPELL_DLL_LIB_NAME) \
	-Wl,--output-def=$(ASPELL_DLL_DEF_NAME)

aspell_dll_OBJECTS = \
	can_have_error-c.o \
	info-c.o \
	string_list-c.o \
	config-c.o \
	speller-c.o \
	string_map-c.o \
	error-c.o \
	mutable_container-c.o \
	string_pair_enumeration-c.o \
	find_speller.o \
	new_checker.o \
	new_config.o \
	string_enumeration-c.o \
	word_list-c.o \
	filter-c.o \
	document_checker-c.o \
	new_filter.o \
	new_fmode.o \

filter-c.o:	filter-c.cpp
word_list-c.o:	word_list-c.cpp
info-c.o:	info-c.cpp
mutable_container-c.o:	mutable_container-c.cpp
error-c.o:	error-c.cpp
document_checker-c.o:	document_checker-c.cpp
string_map-c.o:	string_map-c.cpp
new_config.o:	new_config.cpp
config-c.o:	config-c.cpp
string_enumeration-c.o:	string_enumeration-c.cpp
can_have_error-c.o:	can_have_error-c.cpp
new_filter.o:	new_filter.cpp
new_fmode.o:	new_fmode.cpp
string_list-c.o:	string_list-c.cpp
find_speller.o:	find_speller.cpp
speller-c.o:	speller-c.cpp
string_pair_enumeration-c.o:	string_pair_enumeration-c.cpp
new_checker.o:	new_checker.cpp

static_optfiles = ${top_srcdir}/modules/filter/url-filter.info

### Add the .info file your filter comes with
optfiles = \
  ${top_srcdir}/modules/filter/email-filter.info\
  ${top_srcdir}/modules/filter/tex-filter.info\
  ${top_srcdir}/modules/filter/sgml-filter.info\
  ${top_srcdir}/modules/filter/html-filter.info\
  ${top_srcdir}/modules/filter/context-filter.info\
  ${top_srcdir}/modules/filter/nroff-filter.info\
  ${top_srcdir}/modules/filter/texinfo-filter.info

static_optfiles += ${optfiles}

# settings.h added as a dependency so it will get recreated if
#   the COMPILE_IN_FILTERS option changes
win32/gen/static_filters.src.cpp: ${static_optfiles} ${top_srcdir}/gen/mk-static-filter.pl
	perl ${top_srcdir}/gen/mk-static-filter.pl ${static_optfiles}

${top_srcdir}/lib/new_filter.cpp: win32/gen/static_filters.src.cpp


aspell_dll: $(ASPELL_DLL_NAME)
$(ASPELL_DLL_NAME): $(aspell_dll_OBJECTS)
	$(CXX) $(FLAGS) \
	-shared \
	$(aspell_dll_LDFLAGS) \
	$+ \
	$(aspell_dll_EXTRALIBS) \
	$(LIBS) -o $@
aspell_dll_clean:
	-rm $(aspell_dll_OBJECTS) \
	$(ASPELL_DLL_NAME) \
	$(ASPELL_DLL_LIB_NAME) \
	$(ASPELL_DLL_DEF_NAME) \
	${top_srcdir}/gen/static_filters.src.cpp

################################################################################
# {{{1	pspell-dll
#
PSPELL_DLL_NAME=pspell-$(LIBVERSION).dll
PSPELL_DLL_LIB_NAME=libpspell-$(LIBVERSION)-dll.lib
PSPELL_DLL_DEF_NAME=pspell-$(LIBVERSION).def

pspell_dll_LDFLAGS = \
	-Wl,--compat-implib \
	-Wl,--out-implib=$(PSPELL_DLL_LIB_NAME) \
	-Wl,--output-def=$(PSPELL_DLL_DEF_NAME)
pspell_dll_OBJECTS = \
	dummy.o \

dummy.o:	dummy.cpp

pspell_dll: $(PSPELL_DLL_NAME)
$(PSPELL_DLL_NAME): $(pspell_dll_OBJECTS)
	$(CXX) $(FLAGS) \
	-shared \
	$(pspell_dll_LDFLAGS) \
	$+ \
	$(LIBS) -o $@
pspell_dll_clean:
	-rm $(pspell_dll_OBJECTS) \
	$(PSPELL_DLL_NAME) \
	$(PSPELL_DLL_LIB_NAME) \
	$(PSPELL_DLL_DEF_NAME)

################################################################################
# {{{1	aspell-exe
#
ASPELL_EXE_NAME=aspell.exe
aspell_exe_EXTRALIBS = \
	$(ASPELL_COMMON_A_NAME) \
	$(ASPELL_DLL_LIB_NAME) \
	$(PSPELL_DLL_LIB_NAME) \

aspell_exe_OBJECTS = \
	aspell.o \
	check_funs.o \
	checker_string.o \

aspell.o:	aspell.cpp
check_funs.o:	check_funs.cpp
checker_string.o:	checker_string.cpp

aspell_exe: $(ASPELL_EXE_NAME)
$(ASPELL_EXE_NAME): $(aspell_exe_OBJECTS)
	$(CXX) $(FLAGS) \
	$+ \
	$(aspell_exe_EXTRALIBS) \
	$(CURSES_LIB) \
	$(LIBS) -o $@
aspell_exe_clean:
	-rm $(aspell_exe_OBJECTS) $(ASPELL_EXE_NAME)

################################################################################
# {{{1	word-list-compress
#
WORD_LIST_COMPRESS_EXE_NAME=word-list-compress.exe

word_list_compress_OBJECTS = \
	compress.o \

compress.o:	compress.c

word_list_compress_exe: $(WORD_LIST_COMPRESS_EXE_NAME)
$(WORD_LIST_COMPRESS_EXE_NAME): $(word_list_compress_OBJECTS)
	$(CXX) $(FLAGS) \
	$+ \
	$(LIBS) -o $@
word_list_compress_clean:
	-rm $(word_list_compress_OBJECTS) $(WORD_LIST_COMPRESS_EXE_NAME)

################################################################################
# {{{1	strip
#
strip:
	$(STRIP) -g \
	$(ASPELL_DLL_NAME) \
	$(PSPELL_DLL_NAME) \
	$(ASPELL_EXE_NAME) \
	$(WORD_LIST_COMPRESS_EXE_NAME)

################################################################################
# {{{1	msvc-lib
#
msvc_lib:
ifdef MSVCLIB
	-$(MSVCLIB) -machine:ix86 -def:$(ASPELL_DLL_DEF_NAME)
	-$(MSVCLIB) -machine:ix86 -def:$(PSPELL_DLL_DEF_NAME)
else
	@echo "msvc .lib skipped"
endif
msvc_lib_clean:
	-rm aspell-$(LIBVERSION).lib pspell-$(LIBVERSION).lib aspell-common-$(ASPELLVERSION).lib
	-rm aspell-$(LIBVERSION).exp pspell-$(LIBVERSION).exp aspell-common-$(ASPELLVERSION).exp

################################################################################
# {{{1	install, regfile
#
REGFILE_NAME=aspell.reg

$(REGFILE_NAME):
	echo REGEDIT4		>  aspell.reg
	echo 		>> aspell.reg
	echo [HKEY_CURRENT_USER\\Environment]	>> aspell.reg
	echo \"ASPELL_CONF\"=\"prefix $(prefix)\"	>> aspell.reg

regfile: $(REGFILE_NAME)
regfile_clean:
	-rm $(REGFILE_NAME)
install: regfile
	-mkdir $(installdir)
	-mkdir $(pkglibdir)
	-mkdir $(pkgdatadir)
	-cp  $(WORD_LIST_COMPRESS_EXE_NAME) \
	    $(ASPELL_EXE_NAME) \
	    $(PSPELL_DLL_NAME) \
	    $(ASPELL_DLL_NAME) \
	    $(installdir)
	-cp $(top_srcdir)/data/*.cmap $(pkgdatadir)
	-cp $(top_srcdir)/data/*.cset $(pkgdatadir)
	-cp $(top_srcdir)/data/*.kbd $(pkgdatadir)
	-cp $(REGFILE_NAME) $(installdir)
	


