## Geany LLM Interaction Plugin

This is a Geany plugin to provide interaction with a Large Language Model
(LLM), initially designed to connect to a local llama.cpp HTTP server.

Right now it connects to the completion endpoint and returns a single answer only.
Hopefully it will be able to do more advanced functions later, like
- Code completions in a document (TODO)
- Chat with documents attached (TODO)

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

Run a local model using llama-server, e.g. a small local model qwen-coder-2.5

```
llama-server --fim-qwen-1.5b-default
```

- Once installed, enable the plugin in Geany's Plugin Manager (Tools -> Plugin Manager).
Look for the "LLama Assistant" plugin.

- Set up the host (e.g. http://localhost:8080 or wherever your server runs)

- Specify the model qwen-coder-2.5

Further details on configuring the LLM server will be available in the
plugin's configuration dialog once implemented (temperature, token limit, etc.).
