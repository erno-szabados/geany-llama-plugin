# The name of your plugin shared library. Geany plugins are typically named with a prefix like geany_
# followed by the plugin name and the shared library extension.
plugin_LTLIBRARIES = libgeany_llm.la

# Source files for your plugin
libgeany_llm_la_SOURCES = \
    plugin.c \
    plugin.h \
    settings.c \
    settings.h \
    llm.c \
    llm.h \
    types.h

# Compiler flags (CFLAGS) and linker flags (LDFLAGS) for your plugin
# These are automatically set by PKG_CHECK_MODULES in configure.ac
libgeany_llm_la_CFLAGS = \
    $(GEANY_CFLAGS) \
    $(GTK3_CFLAGS) \
    $(LIBCURL_CFLAGS) \
    $(AM_C_CFLAGS) \
    $(WARN_CFLAGS) \
    $(GETTEXT_CFLAGS)\
    $(JSONGLIB_CFLAGS)

libgeany_llm_la_LIBADD = \
    $(GEANY_LIBS) \
    $(GTK3_LIBS) \
    $(LIBCURL_LIBS) \
    $(GETTEXT_LIBS) \
    $(JSONGLIB_LIBS)

# Installation directory for Geany plugins
# Automake expects the installation directory for _LTLIBRARIES to be defined
# by a variable named after the target's base name plus 'dir'.
# In this case, the base name is 'plugin', so the variable is 'plugindir'.
# We set it relative to $(libdir), which is determined by ./configure.
plugindir = $(libdir)/geany

# Internationalization (gettext)
# Define the plugin's text domain and where to install the message catalog
GETTEXT_PACKAGE = geany-llm-plugin
LOCALE_ALIAS_DIR = $(localedir)
AM_CPPFLAGS = -DLOCALEDIR=\"$(LOCALE_ALIAS_DIR)\"

# Headers to be installed (optional)
# geany_llm_pluginincludedir = $(includedir)/geany-plugins/geany_llm
# geany_llm_plugininclude_HEADERS = plugin.h
