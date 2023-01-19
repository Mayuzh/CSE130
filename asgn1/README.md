# Assignment 1: HTTP Server
## Keywords:
- Strong modularity; Client/Server system
- HTTP server
- System calls

Files
- 
#### httpserver.c
Perform GET, PUT, and APPEND commands. 
- GET: Receive the contents of an existing file.
- PUT: Replace/Update the contents of an existing/new-created file.
- APPEND: Add the contents at the end of an existing file.  

```
    Status-Code	Status-Phrase 		    Usage
	200 		OK 			        	When a method is Successful
	201 		Created 				When a URI’s file is created
	400 		Bad Request 			When a request is ill-formatted
	403 		Forbidden 		    	When the server cannot access the URI’s file
	404 		Not Found 		    	When the URI’s file does not
	500 		Internal Server Error	When an unexpected issue prevents processing
	501 		Not Implemented 		When a request includes an unimplemented Method
	*For Message-Body, add "\n" by the end of Status-Phrase
```
#### Makefile
- type "make", "make all", or "make httpserver"  to build httpserver
- type "make clean" to remove all files that are complier generated

Design
-
#### Usage

```c
./httpserver <port>
```
#### Data Structure
```c
typedef struct Request {
    int socket;

    int method; // Method
    char path[100]; // URI
    char version[100]; // Version
    char hostvalue[100];

    unsigned long int cnt_len; // Content-Length

    bool has_mtd; // should only be true once(only one Request-Line)
    bool has_len; // Should only be true for PUT and APPEND

    char msg_bdy[VALUE_SIZE]; // Message-Body
    int read_len; // GET: Total number of bytes read in from file

} Request;
```
#### Functions

Helper functions to parse URL message into useful information. 
```c
// Extract each line from buffer by "\r\n"
int extract_line(char *buffer, Request *req);
```
```c
// Extract each word from each line by " "
int parse_line(char *line, Request *req);
```
```c
// Check if the request is a bad request
int check_format(Request *req); 
```

Helper functions to implement GET, PUT, and APPEND operations.
```c
void process_get(Request *req);
void process_put_append(Request *req);
```
Main managing function: interface with helper functions(system), and process_request()(clients); interpret requests and send responses.
```c
void process_request(int connfd, char *buffer);
```
Write: use system call sprint() and write() to send contents to clients.
```c
// send_response() is calling through the whole program any time 
void send_response(Request *req, int status);
```
Read: use system call socket(), bind(), listen(), and accept() to build connection with clients.
```c
int create_listen_socket(uint16_t port);
void handle_connection(int connfd);
```