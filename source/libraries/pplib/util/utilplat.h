
#ifndef UTIL_PLAT_H
#define UTIL_PLAT_H

#if defined(_WIN32) || defined(WIN32)
#  ifdef _MSC_VER
#    if defined(_M_64) || defined(_WIN64)
#      define MSVC64
#    else
#      define MSVC32
#    endif
#  else
#    if defined(__MINGW64__)
#      define MINGW64
#    else
#      if defined(__MINGW32__)
#        define MINGW32
#      endif
#    endif
#  endif
#endif

// #ifdef __GNUC__
// //#  define FALLTHRU [[fallthrough]] // c++17
// //#  define FALLTHRU [[gnu:fallthrough]] // c++14
// #  define FALLTHRU __attribute__((fallthrough)); // C and C++03
// #else
// #  define FALLTHRU
// #endif

/* Ensure __has_c_attribute is safely defined so older preprocessors don't crash */
# ifndef __has_c_attribute
    # define __has_c_attribute(x) 0
# endif
/* Standard C23 attribute */
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    # define FALLTHRU [[fallthrough]];
/* C23 attribute check for compilers supporting __has_c_attribute */
# elif __has_c_attribute(fallthrough)
    # define FALLTHRU [[fallthrough]];
/* GNU / Clang extension fallback */
# elif defined(__GNUC__) && (__GNUC__ >= 7)
    # define FALLTHRU __attribute__((fallthrough));
/* MSVC fallback */
# elif defined(_MSC_VER) && (_MSC_VER >= 1910)
    # define FALLTHRU __fallthrough;
/* No-op fallback for older standard C compilers */
# else
    # define FALLTHRU ((void) 0);
# endif

#endif