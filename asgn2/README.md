# Assignments 2–4: A Multi-Threaded HTTP Server
## Keywords:
- Strong modularity; Client/Server system
- HTTP server
- System calls
- atomicity
- concurrency 
- synchronization

Files
- 
#### httpserver.c
Perform GET, PUT, and APPEND commands. 
- GET: Receive the contents of an existing file.
- PUT: Replace/Update the contents of an existing/new-created file.
- APPEND: Add the contents at the end of an existing file.  

```
    Status-Code	Status-Phrase			Usage
	200 		OK						When a method is Successful
	201 		new-created				When a URI’s file is created
	404 		Not Found 				When the URI’s file does not exist
	500 		Internal Server Error 	When an unexpected issue prevents processing

	*For Message-Body, add "\n" by the end of Status-Phrase
```
#### Makefile
- type "make", "make all", or "make httpserver"  to build httpserver
- type "make clean" to remove all files that are complier generated

Design
-
#### Usage

```c
./httpserver [-t threads] [-l logfile] <port>
```
#### Data Structure
```c
typedef struct Request {
    int socket;

    int  method; // Method
    char path[100]; // URI
    char version[100]; // Version

    unsigned long int cnt_len; // Content-Length
    int req_id; // Request-Id

    char msg_bdy[VALUE_SIZE]; // Message-Body
    int  read_len; // GET: Total number of bytes read in from file

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
// LOG is called to write response to logfile indicated by user
void send_response(Request *req, int status);
```
Read: use system call socket(), bind(), listen(), and accept() to build connection with clients.
```c
int create_listen_socket(uint16_t port);
void handle_connection(int connfd);
```