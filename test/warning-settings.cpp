#include <cstdio>

#ifdef __GNUC__
static const int GCC = __GNUC__, GCC_MINOR = __GNUC_MINOR__;
#else
static const int GCC = 0, GCC_MINOR = 0;
#endif

#ifdef __clang__
#define has_gcc_warning(str) __has_warning(str)
#else
#define __has_warning(str) false
#define has_gcc_warning(str) true
#endif

#define disable_clang_warning(str) if (__has_warning("-W" str)) printf("-Wno-%s ", str)
#define disable_gcc_warning(str) if (has_gcc_warning("-W" str)) printf("-Wno-%s ", str)
#define disable_gcc_error(str) if (has_gcc_warning("-W" str)) printf("-Wno-error=%s ", str)

int main() {
  if (!GCC) return 0;
  fprintf(stderr, "gcc version = %d.%d\n", GCC, GCC_MINOR);
  printf("EXTRA_CXXFLAGS = -Wall -Wno-sign-compare -Wno-unused -Werror ");
  printf("-Wno-error=unused-result "); // FIXME: Remove this once the cause of the warning is fixed
  if ((GCC == 4 && GCC_MINOR >= 7) || GCC >= 5)
    disable_gcc_error("maybe-uninitialized");
  if (GCC >= 6)
    disable_gcc_warning("misleading-indentation");
  if (GCC == 7) // FIXME: I _think_ this is a false positive but need
                // to double check
    printf("-Walloc-size-larger-than=-1 ");
  disable_clang_warning("return-type-c-linkage");
  disable_clang_warning("tautological-compare");
  printf("\n");
}
