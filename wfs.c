#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// this fuse operation makes a directory
static int my_getattr(const char *path, struct stat *stbuf) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...
    printf("testing\n");
    return 0; // Return 0 on success
}


// add fuse ops here
static struct fuse_operations ops = {
    // Add other functions (read, write, mkdir, etc.) here as needed
    // usage: .function_name = c_method_name
    .getattr = my_getattr,
};


int main(int argc, char ** argv) {
    if(argc < 3) {
        printf("USAGE: ./wfs disk_path [FUSE options] mount_point\n");
        exit(1);
    }

    // get disk image path and remove from fuse args
    char * disk_img_path = strdup(argv[1]);
    for(int i = strlen(disk_img_path); i > 0; i--){
        // replace _ with .
        if(disk_img_path[i] == '_') {
            disk_img_path[i] = '.';
            break;
        }
    }

    // attempt to open disk img to verify path
    int fd = open(disk_img_path, O_RDONLY);
    if(fd < 0) {
        printf("ERROR: cannot open disk image, verify the path.\nPATH GIVEN: %s\n", disk_img_path);
        exit(1);
    }
    close(fd);

    // remove disk image path from args to give to fuse main
    char ** fuse_args = (char **) malloc((argc-1) * sizeof(char *));
    fuse_args[0] = argv[0];
    for(int i = 1; i < argc - 1; i++) {
        fuse_args[i] = argv[i+1];
    }
    
    return fuse_main(argc - 1, fuse_args, &ops, NULL);;
}
