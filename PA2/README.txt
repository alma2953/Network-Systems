#############################
# Network Systems PA2 - TCP #
#############################

Description
-----------
This program implements a simple TCP-based web server capable of receiving
multiple concurrent HTTP requests from multiple clients. The only supported HTTP
request is GET, and it supports the following file extensions:
  * html, txt, png, gif, jpg, css, and js

Building the server
-------------------
There is an included makefile to make building the server easier. Simply run the
command "make" in the same directory as the makefile a binary called "server"
will be created.

Running the server
------------------
The server expects one command line argument, a port number. This is a user
assigned port via which the server will send and receive HTTP requests and
responses.

To start  the server, run the command
"./server <port>"

Testing the server
------------------
To test the server, you can use any garden-variety web browser. Simply navigate
to the URL: "localhost:<port>", where <port> is the port from which you are
running the server. This will open the website in the www/ folder.

Cleaning up
-----------
The makefile also provides a clean command to remove the generated binary. To
use this, run the command "make clean" in the same directory as the makefile.
