AC_INIT([geany-llm-plugin], [0.1], [erno.szabados@windowslive.com])


AC_CONFIG_SRCDIR([src/plugin.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4]) # Recommended directory for custom autoconf macros

AM_INIT_AUTOMAKE([foreign])

# Check for programs
AM_PROG_AR # Ensure archiver available
LT_INIT # Initialize libtool
AC_PROG_CC
AC_PROG_INSTALL

# Check for standard libraries (optional, often covered by pkg-config)
#AC_CHECK_LIB([m], [cos]) # Example: Check for math library function

# --- Check for Dependencies using pkg-config ---

# Geany API
PKG_CHECK_MODULES([GEANY], [geany >= 1.37]) # Check for Geany development files. Adjust version as needed.
GEANY_CFLAGS="$GEANY_CFLAGS"
GEANY_LIBS="$GEANY_LIBS"

# GTK3
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 3.0]) # Check for GTK3 development files. Adjust version as needed.
GTK3_CFLAGS="$GTK3_CFLAGS"
GTK3_LIBS="$GTK3_LIBS"

# JSON-GLIB
PKG_CHECK_MODULES([JSONGLIB], [json-glib-1.0 >= 1.0],
    [],
    [AC_MSG_ERROR([json-glib is required but not installed])])
JSONGLIB_CFLAGS="$JSONGLIB_CFLAGS"
JSONGLIB_LIBS="$JSONGLIB_LIBS"

# libcurl
PKG_CHECK_MODULES([LIBCURL], [libcurl >= 7.0]) # Check for libcurl development files. Adjust version as needed.
LIBCURL_CFLAGS="$LIBCURL_CFLAGS"
LIBCURL_LIBS="$LIBCURL_LIBS"

# gettext (for internationalization)
AM_GNU_GETTEXT_VERSION([0.21]) # Specify the gettext version
AM_GNU_GETTEXT([external]) # Use external gettext library

dnl Set the libdir 
libdir=$($PKG_CONFIG --variable=libdir geany)
AC_SUBST(libdir)

dnl Set the data root directory
datarootdir=$($PKG_CONFIG --variable=datarootdir geany)
AC_SUBST(datarootdir)

dnl Set the locale data directory 
localedir=$($PKG_CONFIG --variable=localedir geany)
AC_SUBST(localedir)

dnl Set the include directory
includedir=$($PKG_CONFIG --variable=includedir geany)
AC_SUBST(includedir)

AC_CONFIG_FILES([Makefile src/Makefile po/Makevars po/Makefile.in])
AC_OUTPUT
