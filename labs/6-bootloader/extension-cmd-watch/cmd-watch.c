// simple program to run a command when any file that is "interesting" in a directory
// changes.
// e.g., 
//      cmd-watch make
// will run make at each change.
//
// This should use the scandir similar to how you did `find_ttyusb`
//
// key part will be implementing two small helper functions (useful-examples/ will be 
// helpful):
//  - static int pid_clean_exit(int pid);
//  - static void run(char *argv[]);
//
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "libunix.h"

#define _SVID_SOURCE
#include <dirent.h>

// scan the files in "./" (you can extend this) for those
// that match the suffixes in <suffixes> and check  if any
// have changed since the last time.
int check_activity(void) {
    char *suffixes[] = { ".c", ".h", ".S", "Makefile", 0 };
    const char *dirname = ".";
    int changed_p = 0;

    static time_t last_mtime = 0;   // this var is static so the it will see the old value saved
    time_t temp_last_time = last_mtime;
    // open directory
    DIR *d = opendir(".");
    struct dirent *dp;
    // scanning all files in directory
    while ((dp = readdir(d)) != NULL) {
        char *d_name = dp->d_name;
        for (int i = 0; suffixes[i]; i++) {
            int slen = strlen(suffixes[i]);
            if (slen > strlen(d_name))
                continue;
            // if this file has the suffix
            if (strcmp(d_name + strlen(d_name) - slen, suffixes[i]) == 0){
                struct stat st;
                stat(d_name, &st);
                // if that file's mod time is after the last time we ran this function, mark changed_p as 1. 
                if (st.st_mtime > temp_last_time) {
                    temp_last_time = st.st_mtime;
                    changed_p = 1;
                }
            }
        }
    }
    last_mtime = temp_last_time;
    closedir(d);

    // return 1 if anything that matched <suffixes>
    return changed_p;
}

// synchronously wait for <pid> to exit.  returns 1 if it exited
// cleanly (via exit(0)), 0 otherwise.
static int pid_clean_exit(int pid) {
    int status;
    int result = waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return 1;
    } else {
        return 0;
    }
}

// simple helper to print null terminated vector of strings.
static void print_argv(char *argv[]) {
    assert(argv[0]);

	fprintf(stderr, "about to exec =<%s ", argv[0]);
	for(int i =1; argv[i]; i++)
		fprintf(stderr, " %s", argv[i]);
	fprintf(stderr, ">\n");
}


// fork/exec <argv> and wait for the result: print an error
// and exit if the kid crashed or exited with an error (a non-zero
// exit code).
static void run(char *argv[]) {

    pid_t pid;

    pid = fork();
    if (pid == -1) {
        // Error case: fork() failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        execvp(argv[0], argv); 
        perror("execvp");   // only runs if execvp failed
        _exit(127);
    } else {
        pid_clean_exit(pid);
    }

}

int main(int argc, char *argv[]) {
    if(argc < 2)
        die("cmd-watch: not enough arguments\n");
    while(1) {
        if (check_activity()) {
            run(&argv[1]); 
        }
        sleep(1);
    }
    
    return 0;
}
