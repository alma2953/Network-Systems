#############################
# Network Systems PA1 - UDP #
#############################

Description
-----------
These programs implement a simple UDP-based server capable of transferring files
between a client and server. These programs may be run on the same machine or on
different machines. The client will issue commands and the server will respond
appropriately. There are 5 commands implemented:
    * put [filename] - Transfers the requested file from client to server
    * get [filename] - Transfers the requested file from server to client
    * delete [filename] - Deletes the requested file from the server
    * ls - Lists the files contained in the server
    * exit - Terminates both the client and server

Building the programs
---------------------
There is a makefile present in each directory (/udp/client/makefile and
/udp/server/makefile) to make building the programs easier.

To compile the client or server, navigate to their respective directory:
"cd client" or "cd server"
and run the make command:
"make"

This will generate the "client" and "server" binaries that may now be run.

Running the client
------------------
Client expects two command line arguments: a port and a hostname. The port can
be any number between 5000 and 65535. Make sure to use the same port for the
server. If running the client and server on the same machine, the hostname is
"localhost", otherwise, run the command "hostname -i" on the machine the server
will be run on, and use that IP address as the hostname.

To start the client, run the command
"./client <hostname> <port>"

Running the server
------------------
Server expects 1 command line argument: a port. This should be the same port #
the client is using.

To start the server, run the command
"./server <port>"

You may now use the commands as described above.

Cleaning up
-----------
The makefile also provides a clean command to remove the compiled binary. To use
this, run the command
"make clean"
in the server or client directory.  
