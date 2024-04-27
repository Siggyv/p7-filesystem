#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>








char* file_system; //define a file system we can use
struct wfs_sb* super_block;

//return 0 on success, -1 on failure
//fills up a given inode, given a path of a inode
int get_inode(const char *path, struct wfs_inode *inode){
    //fetch the root inode
    struct wfs_inode *curr_inode = (struct wfs_inode *) (file_system + super_block->i_blocks_ptr);
    //loop through the path
    char* token;
    char* copy_path = strdup(path);
    struct wfs_dentry* datablock;
    int i;
    int found; //ensures the path has been found
    while((token = strsep(&copy_path, "/")) != NULL){
        //nothing found yet
        found = 0; 
        //fetch the datablock entries
        //look through current inodes data blocks
        for(i=0; i < N_BLOCKS; i++){
            //gets the starting offset of the datablocks, and adds the current block offset to get the datablock
            datablock = (struct wfs_dentry*)(file_system+super_block->d_blocks_ptr+curr_inode->blocks[i]);
            //compare the current path token to see if it exists in a datablock entry
            if (strcmp(datablock->name,token) == 0){
                found = 1;
                break;
            }
        }
        //if the path was never found, it does not exist
        if(!found){
            return -1;
        }

        //if it is found, update the current node, TODO, make sure this arithmetic is correct.
        curr_inode = (struct wfs_inode *) (file_system + super_block->i_blocks_ptr + (datablock->num*sizeof(struct wfs_inode)));
        
    }

    // free(copy_path);
    return 0;
}

// this fuse operation makes a directory
static int my_getattr(const char *path, struct stat *stbuf) {
    struct wfs_inode inode;
    get_inode("path",&inode);

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
    int fd = open(disk_img_path, O_RDWR);
    if(fd < 0) {
        printf("ERROR: cannot open disk image, verify the path.\nPATH GIVEN: %s\n", disk_img_path);
        exit(1);
    }
    struct stat stat;
    fstat(fd,&stat);
    off_t size = stat.st_size;

    //setup pointers
    file_system = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0); // Corrected mmap call
    super_block = (struct wfs_sb*) file_system;

    close(fd);
    // remove disk image path from args to give to fuse main
    char ** fuse_args = (char **) malloc((argc-1) * sizeof(char *));
    fuse_args[0] = argv[0];
    for(int i = 1; i < argc - 1; i++) {
        fuse_args[i] = argv[i+1];
    }
    
    return fuse_main(argc - 1, fuse_args, &ops, NULL);;
}
