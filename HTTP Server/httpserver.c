#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <ctype.h>

//
//// Converts a string to an 16 bits unsigned integer.
//// Returns 0 if the string is malformed or out of the range.
//
uint16_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

//
//// Creates a socket for listening for connections.
//// Closes the program and prints an error message on error.
//
int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    // Creating socket file descriptor
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    // Initializing the address with 0
    memset(&addr, 0, sizeof addr);
    // Storing IP address and port number in addr
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    // Binding the server to the localhost
    // Forcefully attaching socket to the port number
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 500) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

#define BUF_SIZE   4096
#define VALUE_SIZE 2048
#define VERSION    "HTTP/1.1"

typedef enum key {
    GET,
    PUT,
    APPEND,
    HOST,
    USER_AGENT,
    ACCEPT,
    CONTENT_LENGTH,
    CONTENT_TYPE,
    EXPECT,
    TOTAL,
} key;

char *string[TOTAL] = {
    [GET] = "GET",
    [PUT] = "PUT",
    [APPEND] = "APPEND",
    [HOST] = "Host:",
    [USER_AGENT] = "User-Agent:",
    [ACCEPT] = "Accept:",
    [CONTENT_LENGTH] = "Content-Length:",
    [CONTENT_TYPE] = "Content-Type:",
    [EXPECT] = "Expect:",
};

typedef struct Request {
    int socket;

    int method; // Method
    char path[100]; // URI
    char version[100]; // Version

    char hostvalue[100];

    unsigned long int cnt_len; // Content-Length

    bool has_mtd;
    bool has_len;

    char msg_bdy[VALUE_SIZE]; // Message-Body

    int read_len;

} Request;

// Status-Phrase
const char *Phrase(int code) {
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    }
    return NULL;
}

//
// TODO: remove '/'
//
int check_format(Request *req) {
    // Check for version
    if (strncmp(req->version, VERSION, 8) != 0) {
        return 0;
    }

    // Check for valid path format
    if (strncmp(req->path, "//", 1) != 0) {
        return 0;
    }
    // remove '/'
    memmove(req->path, req->path + 1, strlen(req->path));
    // Check for valid length
    if (strlen(req->path) > 19) {
        return 0;
    }
    // Check for [a-zA-Z0-9], '_', '.'
    for (size_t i = 1; i < strlen(req->path); i++) {
        if ((req->path[i] != '_') && (req->path[i] != '.') && (isalnum(req->path[i]) == 0)) {
            return 0;
        }
    }
    return 1;
}

//
// TODO: send response for APPEND, PUT
//
void send_response(Request *req, int status) {
    char response[VALUE_SIZE];

    if (req->method == GET && status == 200) { // Content-Length: length of content from file
        sprintf(response, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n", status, Phrase(status),
            req->read_len);
        write(req->socket, response, strlen(response));
        return;
    }

    else {
        int cnt_len = strlen(Phrase(status)) + 1; // Content-Length: length of Message-Body
        sprintf(response, "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n%s\n", status,
            Phrase(status), cnt_len, Phrase(status));
        write(req->socket, response, strlen(response));
        return;
    }
    return;
}

// TODO: PUT and APPEND must include a message-body
//       and a Content-Length header
// TODO: GET must not include a message-body

void process_get(Request *req) {
    int fd = 0, sz = 0, total_read = 0;
    //printf("process get, path is %s\n", req->path);
    if ((fd = open(req->path, O_RDONLY, 0)) < 0) {
        if (errno == 2) {
            send_response(req, 404);
        } else {
            send_response(req, 403);
        }
        return;
    }
    //send_response(req, 200);
    //char chr[200];
    int buf_sz = 300;
    char *chr = (char *) malloc(buf_sz * sizeof(char));
    while ((sz = read(fd, chr + total_read, 200)) > 0) {
        total_read += sz;
        //write(req->socket, chr, sz);
        if (total_read + 200 > buf_sz) {
            buf_sz += 200;
        }
        chr = (char *) realloc(chr, buf_sz * sizeof(char));
    }
    req->read_len = total_read;
    send_response(req, 200);
    write(req->socket, chr, total_read);

    free(chr);
    close(fd);
}

// PUT Method:
// To update/replace the content of the file identified by the URI.
//
// 1.If file does not exist, create the file;
// set the content to message-body; return status-code 201.
// 2.If file does exist, replace contents with message-body;
// return statu-code 200.
//
// *If message-nody is not equal to content-length,
// return 200 and then return 400
//
// 403: Forbidden 404: Not Found
// errno 2 no such file or directory

void process_put_append(Request *req) {
    int fd = 0;
    int status = 0;
    if (access(req->path, F_OK) == 0)
        status = 200; // truncate code OK

    else
        status = 201; // create code CREATED

    //printf("file name is %s\n", req->path);
    if (req->method == PUT) {
        fd = open(req->path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            send_response(req, 500);
            return;
        }
    }

    if (req->method == APPEND) {
        fd = open(req->path, O_WRONLY | O_APPEND, 0);
        if (fd < 0) {
            if (errno == 2) { // Not Found
                send_response(req, 404);
            } else { // Forbidden
                send_response(req, 403);
            }
            return;
        }
    }

    write(fd, req->msg_bdy, req->cnt_len);
    //send_response(req, status);
    //printf("\nmessage-body is %s length is %lu cnt_len is %lu\n", req->msg_bdy,
    //strlen(req->msg_bdy), req->cnt_len);
    /*
    if (strlen(req->msg_bdy) != req->cnt_len) {
        send_response(req, 400);
    }
    */
    if (req->cnt_len > strlen(req->msg_bdy)) {
        //printf("should read more\n");
        //char reread[BUF_SIZE];
        //int bytes;
        //read(req->socket, reread, BUF_SIZE);
        return;
    }
    send_response(req, status);
    if (req->cnt_len < strlen(req->msg_bdy)) {
        send_response(req, 400);
    }
    close(fd);
}

//
// TODO: Why store for HOST header
// TODO: check for all valid keys
//
int parse_line(char *line, Request *req) {
    //printf("connfd is %d\n", req->socket);
    int key_type = -1;
    bool isKey = true;
    char value[5][VALUE_SIZE];

    // Extract method and key
    char *tok = NULL;
    tok = strtok(line, " ");
    if (isKey) {
        //printf("find a key %s\n", tok);
        for (int i = 0; i < TOTAL; i++) {
            if (strstr(tok, string[i])) {
                key_type = i;
                break;
            }
        }
        isKey = false;
        tok = strtok(NULL, " ");
    }

    // Extract URI, Version, and value
    int count = 0;
    while (tok != NULL) {
        //printf("\neach value is %s\n", tok);
        strncpy(value[count], tok, VALUE_SIZE);
        count++;
        tok = strtok(NULL, " ");
    }

    // Unexpected line
    if (key_type == -1) {
        //fprintf(stderr, "Unknown params, ignoring line\n");
        errx(EXIT_FAILURE, "Unknown params, ignoring line\n");
        return -1;
    }

    switch (key_type) {
    case GET: {
        if (req->has_mtd) {
            return -1;
        }
        req->method = GET;
        req->has_mtd = true;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        //printf("\nmethod is PUT path is %s version is %s\n", value[0], value[1]);

        break;
    }

    case PUT: {
        if (req->has_mtd) {
            return -1;
        }
        req->method = PUT;
        req->has_mtd = true;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        //printf("\nmethod is PUT path is %s version is %s\n", value[0], value[1]);

        break;
    }

    case APPEND: {
        if (req->has_mtd) {
            return -1;
        }
        req->method = APPEND;
        req->has_mtd = true;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        break;
    }

    case HOST: {
        if (count > 2) {
            return -1;
        }
        // host value from hostname
        tok = strtok(value[0], ":");
        tok = strtok(NULL, " ");
        strcpy(req->hostvalue, tok);
        break;
    }

    case USER_AGENT: {
        break;
    }

    case ACCEPT: {
        break;
    }

    // Assume length is a valid number generated by client curl
    case CONTENT_LENGTH: {
        unsigned long int length = atoi(value[0]);
        req->cnt_len = length;
        req->has_len = true;
        break;
    }

    case CONTENT_TYPE: {
        break;
    }

    case EXPECT: {
        break;
    }

    default: {
        //fprintf(stderr, "Unknown param type: %i\n", key_type);
        errx(EXIT_FAILURE, "Unknown param type: %i\n", key_type);
        break;
    }
    }

    return 1;
}

// basically done
int extract_line(char *buffer, Request *req) {

    //printf("\n%s\n", buffer);

    const char *delim = "\r\n\r\n";
    char *msg;
    msg = strstr(buffer, delim);

    if (msg != NULL) {
        msg[0] = '\0';
        msg[1] = '\0';
        msg[2] = '\0';
        msg[3] = '\0';
        strncpy(req->msg_bdy, msg + strlen(delim), VALUE_SIZE);
    } else {
        return -1;
    }
    //printf("original message is %s\n", msg + strlen(delim));

    //printf("message line is-%s-real length is %lu\n", req->msg_bdy, strlen(req->msg_bdy));
    //printf("connfd is %d\n", req->socket);

    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        buffer += strlen(line) + strlen("\r\n"); // manually set index
        if (parse_line(line, req) < 0) {
            return -1;
        }
        line = strtok(buffer, "\r\n"); // get next line
    }

    return 1;
}

//
// TODO: process three operations
//
void process_request(int connfd, char *buffer) {
    Request req = { 0 };
    req.has_len = false;
    req.has_mtd = false;

    req.socket = connfd;
    int status = extract_line(buffer, &req);

    if (status < 0) {
        send_response(&req, 400);
        return;
    }

    // Check request fields satisfy requirements
    if (!check_format(&req)) {
        send_response(&req, 400);
        return;
    }

    // Check commands
    if (req.method == PUT || req.method == APPEND) {
        if (req.has_len == false) {
            send_response(&req, 400);
            return;
        }
        process_put_append(&req);
        return;
    }

    else if (req.method == GET) {
        if (req.has_len == true) {
            send_response(&req, 400);
            return;
        }
        process_get(&req);
        return;
    }

    else {
        send_response(&req, 501);
        return;
    }
    return;
}

// Basically done
void handle_connection(int connfd) {
    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);
    ssize_t bytez = 0;

    /// read all bytes from connfd until we see an error or EOF
    while ((bytez = read(connfd, buffer, BUF_SIZE)) > 0) {

        /*ssize_t bytez_written = 0, curr_write = 0;
        while (bytez_written < bytez) {
            curr_write = write(STDOUT_FILENO, buffer + bytez_written, bytez - bytez_written);
            if (curr_write < 0) {
                // we ded.
                return;
            }
            bytez_written += curr_write;
        }
        printf("\n");*/

        process_request(connfd, buffer);
    }

    // make the compiler not complain
    (void) connfd;
    close(connfd);
    return;
}

// Basically done
int main(int argc, char *argv[]) {
    int listenfd;
    uint16_t port;
    if (argc != 2) {
        errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
    }
    port = strtouint16(argv[1]);
    if (port == 0) {
        errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
    }
    listenfd = create_listen_socket(port);

    signal(SIGPIPE, SIG_IGN);

    while (1) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        handle_connection(connfd);
        //close(connfd);
    }

    return EXIT_SUCCESS;
}
