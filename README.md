## Geany LLM Interaction Plugin

This is a Geany plugin to provide interaction with a Large Language Model
(LLM), initially designed to connect to a local llama.cpp HTTP server.

### Features
- Streaming response from the LLM
- Can stop streaming 
- Can set model, proxy, llm host
- Attach selected documents to context


Right now it connects to the completion endpoint and returns a single answer only.
Hopefully it will be able to do more advanced functions later, like
- Code completions in a document (TODO)
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

## API Key Security

For security, you can provide your API key via the `OPENAI_API_KEY` environment variable instead of saving it in the plugin's configuration file. This avoids storing your key in plaintext on disk.

To use an environment variable, run Geany like this:

```
export OPENAI_API_KEY=sk-...yourkey...
geany
```

If the environment variable is set, the plugin will use it and ignore any API key saved in the config file. You can still enter and save an API key in the settings dialog if you prefer convenience over security.

## Packaging and Installing with RPM (openSUSE Leap)

To build and install the plugin as an RPM package on openSUSE Leap:

1. Create a source tarball from the project root (if not already present):
   
   ```sh
   make dist
   # or manually:
   tar czf geany-llama-plugin-0.1.tar.gz geany-llama-plugin-0.1/
   ```

2. Build the source RPM (SRPM):
   
   ```sh
   rpmbuild -bs geany-llama-plugin.spec --define "_sourcedir $(pwd)" --define "_srcrpmdir $(pwd)"
   ```

3. Build the binary RPM:
   
   ```sh
   rpmbuild -bb geany-llama-plugin.spec --define "_sourcedir $(pwd)" --define "_builddir $(pwd)" --define "_srcrpmdir $(pwd)" --define "_rpmdir $(pwd)"
   ```

   The resulting RPM will be in the `x86_64/` directory.

4. Install the RPM package:
   
   ```sh
   sudo zypper install ./x86_64/geany-llama-plugin-0.1-0.x86_64.rpm
   ```

> **Note:** The generated RPM package is unsigned. Some package managers (such as zypper or YaST) may report the package as untrusted or broken due to the lack of a signature. You can safely ignore this warning for local testing, or sign the package with your own GPG key for distribution.

This process ensures all dependencies are tracked and the plugin is installed in the correct location for Geany.
