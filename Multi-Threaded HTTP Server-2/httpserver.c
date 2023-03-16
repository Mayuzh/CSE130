#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>
#include <stdbool.h>
#include <ctype.h>

#define OPTIONS              "t:l:"
#define VERSION              "HTTP/1.1"
#define BUF_SIZE             4096
#define VALUE_SIZE           2048
#define DEFAULT_THREAD_COUNT 4

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

typedef enum key {
    GET,
    PUT,
    APPEND,
    REQUEST_ID,
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
    [REQUEST_ID] = "Request-Id:", //id
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

    unsigned long int cnt_len; // Content-Length
    int req_id; //Request-Id

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

void send_response(Request *req, int status) {
    //fprintf(logfile, "%s,/%s,%d,%d\n", string[req->method], req->path, status, req->req_id);
    LOG("%s,/%s,%d,%d\n", string[req->method], req->path, status, req->req_id);
    fflush(logfile);

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

void process_get(Request *req) {
    int fd = 0, sz = 0, total_read = 0;
    if ((fd = open(req->path, O_RDONLY, 0)) < 0) {
        if (errno == 2) {
            send_response(req, 404);
        } else {
            send_response(req, 403);
        }
        return;
    }

    int buf_sz = 300;
    char *chr = (char *) malloc(buf_sz * sizeof(char));
    while ((sz = read(fd, chr + total_read, 200)) > 0) {
        total_read += sz;
        if (total_read + 200 > buf_sz) {
            buf_sz += 200;
        }
        chr = (char *) realloc(chr, buf_sz * sizeof(char));
    }

    req->read_len = total_read;
    send_response(req, 200);

    write(req->socket, chr, total_read);
    close(fd);
    free(chr);
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
            }
            /*else { // Forbidden
                send_response(req, 403);
            }*/
            return;
        }
    }

    write(fd, req->msg_bdy, req->cnt_len);
    close(fd);

    if (req->cnt_len > strlen(req->msg_bdy)) {
        return;
    }
    send_response(req, status);
    /*
    if (req->cnt_len < strlen(req->msg_bdy)) {
    	//printf("send bad response\n");
        send_response(req, 400);
    }*/
}

int parse_line(char *line, Request *req) {
    int key_type = -1;
    int count = 0;
    char value[10][VALUE_SIZE];

    // Extract method and key
    char *tok = NULL;
    tok = strtok(line, " ");
    while (tok != NULL) {
        //printf("\neach value is %s\n", tok);
        if (count == 0) {
            for (int i = 0; i < TOTAL; i++) {
                if (strstr(tok, string[i])) {
                    key_type = i;
                    break;
                }
            }
        } else {
            strncpy(value[count - 1], tok, VALUE_SIZE);
        }
        count += 1;
        tok = strtok(NULL, " ");
    }

    // Unexpected line
    /*if (key_type == -1) {
        //fprintf(stderr, "Unknown params, ignoring line\n");
        errx(EXIT_FAILURE, "Unknown params, ignoring line\n");
        return -1;
    }*/

    switch (key_type) {
    case GET: {
        req->method = GET;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        break;
    }

    case PUT: {
        req->method = PUT;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        break;
    }

    case APPEND: {
        req->method = APPEND;
        strcpy(req->path, value[0]);
        strcpy(req->version, value[1]);
        break;
    }

    case REQUEST_ID: {
        req->req_id = atoi(value[0]);
        break;
    }

    case HOST: {
        break;
    }

    case USER_AGENT: {
        break;
    }

    case ACCEPT: {
        break;
    }

    case CONTENT_LENGTH: {
        unsigned long int length = atoi(value[0]);
        req->cnt_len = length;
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

int extract_line(char *buffer, Request *req) {
    const char *delim = "\r\n\r\n";
    char *msg;
    msg = strstr(buffer, delim);

    if (msg != NULL) {
        msg[0] = '\0';
        msg[1] = '\0';
        msg[2] = '\0';
        msg[3] = '\0';
        strncpy(req->msg_bdy, msg + strlen(delim), VALUE_SIZE);
    }
    //printf("message line is-%s-real length is %lu\n",
    //req->msg_bdy, strlen(req->msg_bdy));

    char *line = strtok(buffer, "\r\n");
    while (line != NULL) {
        // manually set index
        buffer += strlen(line) + strlen("\r\n");
        if (parse_line(line, req) < 0) {
            return -1;
        }
        line = strtok(buffer, "\r\n"); // get next line
    }

    return 1;
}

void process_request(int connfd, char *buffer) {
    Request req = { 0 };
    req.req_id = 0;
    req.socket = connfd;

    int status = extract_line(buffer, &req);
    if (status < 0) {
        printf("send bad response\n");
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
        process_put_append(&req);
        return;
    }

    else if (req.method == GET) {
        process_get(&req);
        return;
    }

    else {
        send_response(&req, 501);
        return;
    }

    return;
}

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

static void handle_connection(int connfd) {
    char buf[BUF_SIZE];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes_read;

    // Read from connfd until EOF or error.
    while ((bytes_read = read(connfd, buf, BUF_SIZE)) > 0) {
        /*ssize_t bytes_written, bytes;
        // Write to stdout.
        bytes = 0;
        do {
            bytes_written = write(STDOUT_FILENO, buf + bytes, bytes_read - bytes);
            if (bytes_written < 0) {
                return;
            }
            bytes += bytes_written;
        } while (bytes_written > 0 && bytes < bytes_read);
		
        // Write to connfd.
        bytes = 0;
        do {
            bytes_written = write(connfd, buf + bytes, bytes_read - bytes);
            if (bytes_written < 0) {
                return;
            }
            bytes += bytes_written;
        } while (bytes_written > 0 && bytes < bytes_read);*/

        // process request
        process_request(connfd, buf);
    }
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);

    int listenfd = create_listen_socket(port);
    //LOG("port=%" PRIu16 ", threads=%d\n", port, threads);

    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        handle_connection(connfd);
        close(connfd);
    }

    return EXIT_SUCCESS;
}
