/* common/settings.h.in.  Generated from configure.ac by autoheader.  */

/* Defined if no special Workarounds are needed for Curses headers */
#undef CURSES_INCLUDE_STANDARD

/* Defined if special Wordaround I is need for Curses headers */
#undef CURSES_INCLUDE_WORKAROUND_1

/* Defined if curses like POSIX Functions should be used */
#undef CURSES_ONLY

/* Defined if win32 relocation should be used */
#define ENABLE_WIN32_RELOCATABLE 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Defined if msdos getch is supported */
#undef HAVE_GETCH

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Defined if the curses library is available */
#undef HAVE_LIBCURSES

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Defined if mmap and friends is supported */
#undef HAVE_MMAP

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Defined if Posix Termios is Supported */
#undef POSIX_TERMIOS

/* Defined if STL rel_ops polute the global namespace */
#undef REL_OPS_POLLUTION

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Defined if file ino is supported */
#undef USE_FILE_INO

/* Defined if file locking and truncating is supported */
#undef USE_FILE_LOCKS

/* Defined if Posix locales are supported */
#undef USE_LOCALE

/* Version number of package */
#define VERSION "0.50.4.1"

#define PACKAGE_VERSION "aspell-6.0"

#define C_EXPORT extern "C"

#if defined(ASPELL_NO_EXPORTS)
#  define ASPELL_API
#elif defined(_WINDLL) || defined(_USRDLL) || defined(__DLL__)
#  define ASPELL_API  __declspec(dllexport)
#else
#  define ASPELL_API  __declspec(dllimport)
#endif /*_DLL */

