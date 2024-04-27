#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


struct wfs_sb *file_system; //define a file system we can use

// this fuse operation makes a directory
static int my_getattr(const char *path, struct stat *stbuf) {
    printf("the stat function is now running\n");
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) { // Assuming '/' is your root directory
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2; // Common for directories
    } else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024; // Example size
    }

    // Properly set time fields and others as necessary
    return 0;
}

static int wfs_mkdir(const char *path, mode_t mode) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...
    printf("mkdir is now running\n");
    return 0; // Return 0 on success
}

// add fuse ops here
static struct fuse_operations ops = {
    // Add other functions (read, write, mkdir, etc.) here as needed
    // usage: .function_name = c_method_name
    .getattr = my_getattr,
    .mkdir = wfs_mkdir,
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
    int fd = open(disk_img_path, O_RDWR);
    if(fd < 0) {
        printf("ERROR: cannot open disk image, verify the path.\nPATH GIVEN: %s\n", disk_img_path);
        exit(1);
    }
    struct stat stat;
    fstat(fd,&stat);
    off_t size = stat.st_size;

    file_system = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0); // Corrected mmap call    int test = file_system->d_bitmap_ptr;
    // Accessing the data structure
    int test = file_system->d_bitmap_ptr;
    printf("bitmap pointer is: %d\n", test);
    
    close(fd);

    // remove disk image path from args to give to fuse main
    char ** fuse_args = (char **) malloc((argc-1) * sizeof(char *));
    fuse_args[0] = argv[0];
    for(int i = 1; i < argc - 1; i++) {
        fuse_args[i] = argv[i+1];
    }
    
    return fuse_main(argc - 1, fuse_args, &ops, NULL);;
}
