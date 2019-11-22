###################################
# Network Systems PA3 - Web Proxy #
###################################

Description
-----------
This program implements an HTTP forward web proxy. The proxy acts as a middleman
between your web browser and the internet. It is capable of locally caching
files retrieved by HTTP requests so they do not need to be redundantly fetched
if they are requested again. The user can optionally supply a timeout value so
files that were cached later than this timeout value will not be served. It also
has a DNS cache, so once a hostname is resolved, it does not need to be resolved
again until the proxy is terminated. It also supports blacklisting by hostname
or IP in the "blacklist" file. The program uses mutexes to prevent overwriting
of caches shared between threads (program memory DNS cache or local file cache).
Note that this proxy only supports HTTP, not HTTPS so it will not work for the
majority of modern websites.

Configuring your browser
------------------------
I tested this proxy using firefox. To setup firefox to use the proxy, go to
Preferences > Network Settings > Settings
Select "Manual proxy configuration", and enter "localhost" in the "HTTP proxy"
field. Choose a port number and input the same port number as a command line
argument when running the proxy. Check the "Use this proxy server for all
protocols", and now the browser is configured to use the proxy.

Building the proxy
------------------
There is an included makefile to make building the proxy easier. Simply run the
command "make" in the directory. This will generate a binary named "webproxy"

Running the proxy
-----------------
The server expects 1 command line argument, a port number, as well as 1 optional
command line argument, a timeout value for the local file cache. Files cached
later than the timeout value will not be served by the proxy.

To run the proxy with no timeout, run the command
./webproxy <port>

To run the proxy with a timeout, run the command
./webproxy <port> <timeout>

Cleaning up
-----------
The makefile also provides a clean command to remove the generated binary. To
use this, run the command "make clean" in the same directory as the makefile.
