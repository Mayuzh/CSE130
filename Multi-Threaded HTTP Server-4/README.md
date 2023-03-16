# Assignments 2–4: A Multi-Threaded HTTP Server
## Keywords:
- Strong modularity; Client/Server system
- HTTP server
- System calls
- muti-threaded
- atomicity
- concurrency 
- synchronization

Usage
-
```c
./httpserver [-t threads] [-l logfile] <port>
```
Files
- 
#### httpserver.c
Perform GET, PUT, and APPEND commands. 
- GET: Receive the contents of an existing file.
- PUT: Replace/Update the contents of an existing/new-created file.
- APPEND: Add the contents at the end of an existing file.  
```c
// Status code and errer message
//For Message-Body, add "\n" by the end of Status-Phrase
const char *Phrase(int code) {
    switch (code) {
    case 200: return "OK"; // When a method is Successful
    case 201: return "Created"; // When a URI’s file is created
    case 404: return "Not Found"; // When the URI’s file does not exist
    case 500: return "Internal Server Error"; // When an unexpected issue prevents processing
    }
    return NULL;
}
```
##### Data Structure
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

##### Functions
Helper function to create parallel multi-threads.
```c
void *worker_thread(void *arg);
```

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
#### queue.h/queue.c
Contains the function definition and implementation of queue ADT.
Uses semaphores to achieve atomicity.

##### Resources and Examples
- creating semaphore:
http://www.eventorient.com/2017/07/semaphores-in-mac-os-x-seminit-is.html
- tail queue:
https://man7.org/linux/man-pages/man3/tailq.3.html
https://ofstack.com/C++/9343/c-language-tail-queue-tailq-is-shared-using-examples.html

##### Functions
```c
void createQueue(void);
// add an element to the end of the queue
void enqueue(int connfd);
// remove an element from the head of the queue
int dequeue(void);
```
#### Makefile
- type "make", "make all", or "make httpserver"  to build httpserver
- type "make clean" to remove all files that are complier generated
