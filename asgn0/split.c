// $Id: split.c,v 1.1 2022-04-02 23:38:47-04 monaz - $

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
int readfile(const char *filename, const char *delimiter);
//int readstdin(const char *delimiter);
int readfile(const char *filename, const char *delimiter) {
    //printf ("delimiter is %s\n", delimiter);
    int fd, sz;
    int flag = 0;
    char chr[200];
    fd = open(filename, O_RDONLY);
    if (filename[0] == '-') {
        fd = 0;
    }
    if (fd < 0) {
        printf("split: %s: No such file or dirctory\n", filename);
        return 2;
    }

    while ((sz = read(fd, chr, 200)) > 0) {
        for (int i = 0; i < sz; i++) {
            if (chr[i] == delimiter[0])
                chr[i] = 10;
        }
        write(1, chr, sz);
    }
    close(fd);
    return flag;
}

int main(int argc, char **argv) {
    int flag = 0;
    //printf ("number of arguments: %d\n", argc);
    const char *delimiter = argv[1];
    if (strlen(delimiter) > 1) {
        printf("Cannot handle multi-character splits: %s\n", delimiter);
        printf("usage: %s: <split_char> [<file1> <file2> ...]\n", argv[0]);
        return 22;
    }
    //printf ("delimiter is %s\n", delimiter);
    if (argc < 3) {
        printf("Not enough arguments\n");
        printf("usage: %s: <split_char> [<file1> <file2> ...]\n", argv[0]);
        return 22;
    } else if (argc == 3) {
        const char *filename = argv[2];
        //printf ("filename is %s\n", filename)
        flag = readfile(filename, delimiter);
    }

    else {
        for (int argi = 2; argi != argc; ++argi) {
            const char *filename = argv[argi];
            if (readfile(filename, delimiter) == 2) {
                flag = 2;
            }
        }
    }
    return flag;
}
