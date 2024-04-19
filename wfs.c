#include "wfs.h"
#include <fuse.h>
#include <stdio.h>


// add fuse ops here
static struct fuse_operations ops = {
    // Add other functions (read, write, mkdir, etc.) here as needed
    // usage: .function_name = c_method_name
};


int main(int argc, char ** argv) {
    if(argc < 3) {
        printf("USAGE: ./wfs disk_path [FUSE options] mount_point\n");
        exit(1);
    }
    
    return fuse_main(argc, argv, &ops, NULL);
}
