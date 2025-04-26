## Geany LLM Interaction Plugin

This is a Geany plugin to provide interaction with a Large Language Model
(LLM), initially designed to connect to a local llama.cpp HTTP server.

### Building and Installation:

This project uses Autotools. You will need autoconf, automake, libtool,
pkg-config, a C compiler, and the development files for Geany, GTK+ 3,
libcurl, and gettext installed on your system.

1. Run the autogen script:
   ./autogen.sh

2. Configure the project:
   ./configure

3. Build the plugin:
   make

4. Install the plugin:
   make install
   (You might need root privileges for system-wide installation)

### Usage:

Once installed, enable the plugin in Geany's Plugin Manager (Tools -> Plugin Manager).
Look for the "LLM Interaction" plugin.

Further details on configuring the LLM server will be available in the
plugin's configuration dialog once implemented.
