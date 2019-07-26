AM_CPPFLAGS = -I$(top_builddir)/interfaces/cc
LDADD = $(top_builddir)/libaspell.la

if USE_OSSFUZZ_FLAG
FUZZ_FLAG = $(LIB_FUZZING_ENGINE)
else
if USE_OSSFUZZ_STATIC
LDADD += $(LIB_FUZZING_ENGINE)
FUZZ_FLAG =
else
LDADD += libstandaloneengine.a
FUZZ_FLAG =
endif
endif

noinst_PROGRAMS =
noinst_LIBRARIES =

if USE_OSSFUZZERS
noinst_PROGRAMS += \
	aspell_fuzzer

noinst_LIBRARIES += \
	libstandaloneengine.a
endif

aspell_fuzzer_SOURCES = aspell_fuzzer.cc testinput.h
aspell_fuzzer_CXXFLAGS = $(AM_CXXFLAGS) $(FUZZ_FLAG)
aspell_fuzzer_LDFLAGS = $(AM_LDFLAGS) -static

libstandaloneengine_a_SOURCES = standaloneengine.cc
libstandaloneengine_a_CXXFLAGS = $(AM_CXXFLAGS)
