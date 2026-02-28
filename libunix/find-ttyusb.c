// engler, cs140e: your code to find the tty-usb device on your laptop.
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "libunix.h"

#define _SVID_SOURCE
#include <dirent.h>
static const char *ttyusb_prefixes[] = {
    "ttyUSB",	// linux
    "ttyACM",   // linux
    "cu.SLAB_USB", // mac os
    "cu.usbserial", // mac os
    // if your system uses another name, add it.
	0
};

static int filter(const struct dirent *d) {
    // scan through the prefixes, returning 1 when you find a match.
    // 0 if there is no match.
    const char *name = d->d_name;
    for (int i = 0; ttyusb_prefixes[i]; i++) {
        if (strncmp(name, ttyusb_prefixes[i], strlen(ttyusb_prefixes[i])) == 0) {
            return 1;
        }
    }
    return 0;

}

// find the TTY-usb device (if any) by using <scandir> to search for
// a device with a prefix given by <ttyusb_prefixes> in /dev
// returns:
//  - device name.
// error: panic's if 0 or more than 1 devices.
char *find_ttyusb(void) {
    // use <alphasort> in <scandir>
    // return a malloc'd name so doesn't corrupt.
    int n;
    struct dirent **list = 0;
    // Scan current directory using alphasort for alphabetical sorting
    // list is where results are stored 
    // for every file in dev, call filter(file)
    n = scandir("/dev", &list, filter, alphasort);
    // if we find no devices or more than one, error 
    if (n != 1) {
        panic("0 or more than 1 devices");
    }
    // panic(n == 1);
    // malloced result, return it 
    char *result = strdupf("/dev/%s", list[0]->d_name);
    return result;
}

// return the most recently mounted ttyusb (the one
// mounted last).  use the modification time 
// returned by state.
char *find_ttyusb_last(void) {
    int n;
    struct dirent **list = 0;
    // Scan current directory using alphasort for alphabetical sorting
    // list is where results are stored 
    // for every file in dev, call filter(file)
    n = scandir("/dev", &list, filter, alphasort);
    // if we find no devices or more than one, error 

    if (n == 0) {
        perror("No matching tty-serial devices");
    }
    time_t last_t;
    char *result;
    for (int i = 0; i < n; i ++) {
        struct stat st;
        stat(list[i]->d_name, &st);
        if (i == 0) {
            last_t = st.st_mtime;
            result = list[i]->d_name;
        }
        if (st.st_mtime > last_t) {
            last_t = st.st_mtime;
            result = list[i]->d_name;
        }
    }
    result = strdupf("/dev/%s", result);
    return result;
}

// return the oldest mounted ttyusb (the one mounted
// "first") --- use the modification returned by
// stat()
char *find_ttyusb_first(void) {
    int n;
    struct dirent **list = 0;
    // Scan current directory using alphasort for alphabetical sorting
    // list is where results are stored 
    // for every file in dev, call filter(file)
    n = scandir("/dev", &list, filter, alphasort);
    // if we find no devices or more than one, error 

    if (n == 0) {
        perror("No matching tty-serial devices");
    }
    time_t first_t;
    char *result;
    for (int i = 0; i < n; i ++) {
        struct stat st;
        stat(list[i]->d_name, &st);
        if (i == 0) {
            first_t = st.st_mtime;
            result = list[i]->d_name;
        }
        if (st.st_mtime < first_t) {
            first_t = st.st_mtime;
            result = list[i]->d_name;
        }
    }
    result = strdupf("/dev/%s", result);
    return result;
}
