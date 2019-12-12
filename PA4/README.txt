#############################
# Network Systems PA4 - DFS #
#############################

Description
-----------
These programs implement a TCP-based DFS (distributed file server) and DFC
(distributed file client). The server is capable of handling multiple conccurent
connections for multiple concurrent users. The client will issue commands and
the server will respond appropriately. Before issuing a command, the client must
be authenticated. If the username and password do not exist in the servers
configuration, the server will not serve the client.  There are 3 commands
implemented:
    * put [filename] - Splits the requested file into 4 (roughly) equal parts
                       and sends 2 parts to each of the 4 servers. This means
                       that if 1 server is terminated, all files will still be
                       available, and if 2 are terminated, a file has a 50%
                       chance of being available.

    * get [filename] - Requests the 4 parts from the servers to be sent back for
                       reassembling the file. If a part is unavailable, the user
                       will be informed that the file is incomplete.

    * ls -             Lists the files contained on the servers, and whether
                       they are complete or not.

Building the programs
---------------------
There is an included makefile to make building the client and server easier.
Simply run the command "make" in the directory. This will generate binaries
named "dfs" and "dfc".

Running the client
------------------
The client expects 1 command line argument: a configuration file storing the IP
and port of each of the 4 servers, a username, and a password. Each user will
have a separate configuration files. 2 are provided in the repository:
"dfc-Alex.conf" and "dfc-Jon.conf".

To start the client, run the command
"./dfc <config file name>"

Running the server
------------------
The server expects 2 command line arguments: a directory and a port.

The directory is where the server will store the file pieces. The
directory should be one of "/DFS1", "/DFS2", "/DFS3", or "/DFS4", or the program
will not run.

The port must match the appropriate line in the dfc configuration file.

To start the servers, run the following command for each server
"./dfs </DFS{1-4}> <port #>"

Cleaning up
-----------
The makefile also provides a clean command to remove the compiled binaries. To
use this, run the command
"make clean"
