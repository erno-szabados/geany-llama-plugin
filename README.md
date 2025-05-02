## Geany LLM Interaction Plugin

This is a Geany plugin to provide interaction with a Large Language Model
(LLM), initially designed to connect to a local llama.cpp HTTP server.

### Features
- Streaming response from the LLM
- Can stop streaming 
- Can set model, proxy, llm host

### Warning!

- Do not unload/reload the module or it will crash geany. json-glib could
not deal with dynamic reloading as is, and i don't plan to implement 
gobject module loading (well, i tried, but i'd rather won't). 


Right now it connects to the completion endpoint and returns a single answer only.
Hopefully it will be able to do more advanced functions later, like
- Code completions in a document (TODO)
- Chat with documents attached - The current document is attached to the context.
- Set API key, temperature, sampling params, ...

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

or even better:

```
lama-server -mu https://huggingface.co/bartowski/ibm-granite_granite-3.3-2b-instruct-GGUF/resolve/main/ibm-granite_granite-3.3-2b-instruct-Q8_0.gguf -ngl 99  -c 32768 --mlock
```

- Once installed, enable the plugin in Geany's Plugin Manager (Tools -> Plugin Manager).
Look for the "LLama Assistant" plugin.

- Set up the host (e.g. http://localhost:8080 or wherever your server runs)

- Specify the model qwen-coder-2.5

Further details on configuring the LLM server will be available in the
plugin's configuration dialog once implemented (temperature, token limit, etc.).
