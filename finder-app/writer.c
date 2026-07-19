// Accepts the following arguments: the first argument is a full path to a file (including filename) on the 
// filesystem, referred to below as writefile; the second argument is a text string which will be written 
// within this file, referred to below as writestr
// 
// Exits with value 1 error and print statements if any of the arguments above were not specified
// 
// Creates a new file with name and path writefile with content writestr, overwriting any existing file and 
// creating the path if it doesn’t exist. Exits with value 1 and error print statement if the file could not 
// be created.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <writefile> <writestr>\n", argv[0]);
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    int fd = open(writefile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        trace_log(LOG_ERR, "Failed to open %s for writing: %s", writefile, strerror(errno));
        return 1;
    }

    ssize_t bytes_written = write(fd, writestr, strlen(writestr));
    if (bytes_written == -1) {
        trace_log(LOG_ERR, "Failed to write into %s: %s", writefile, strerror(errno));
        return 1;
    } else {
        trace_log(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    }

    close(fd);
    return 0;
}