#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libunix.h"

// allocate buffer, read entire file into it, return it.   
// buffer is zero padded to a multiple of 4.
//
//  - <size> = exact nbytes of file.
//  - for allocation: round up allocated size to 4-byte multiple, pad
//    buffer with 0s. 
//
// fatal error: open/read of <name> fails.
//   - make sure to check all system calls for errors.
//   - make sure to close the file descriptor (this will
//     matter for later labs).
// 
void *read_file(unsigned *size, const char *name) {
    // How: 
    //    - use stat() to get the size of the file.
    //    - round up to a multiple of 4.
    //    - allocate a buffer
    //    - zero pads to a multiple of 4.
    //    - read entire file into buffer (read_exact())
    //    - fclose() the file descriptor
    //    - make sure any padding bytes have zeros.
    //    - return it. 

    // save the size using stat 
    struct stat st;
    if (stat(name, &st) < 0)
        sys_die("stat failed on %s\n", name);

    unsigned nbytes = st.st_size;
    
    // allocate buffer aaa
    char *buffer = calloc(1, nbytes+4);

    // open fd 
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        sys_die("open failed on %s\n", name);

    // read in to buffer
    if (nbytes != 0) {
        read_exact(fd, buffer, nbytes);
    }
    
    // save size
    *size = nbytes;
    // close fd
    if (close(fd) < 0)
        sys_die("close failed\n", name);
    return buffer;

}
