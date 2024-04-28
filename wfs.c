#include <sys/types.h>
#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

char *file_system; // define a file system we can use
struct wfs_sb *super_block;

// return 0 on success, -1 on failure
// fills up a given inode, given a path of a inode
static int get_inode(const char *path, struct wfs_inode *inode)
{
    // printf("the path is: %s\n", path);
    // fetch the root inode
    struct wfs_inode *curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr);
    // loop through the path
    char *token;
    char *copy_path = strdup(path);
    struct wfs_dentry *datablock;
    int i;
    int found; // ensures the path has been found
    while ((token = strsep(&copy_path, "/")) != NULL)
    {
        // skip empty tokens
        if (strcmp(token, "") == 0)
        {
            continue;
        }
        // printf("the token is: %s\n", token);
        // nothing found yet
        found = 0;
        // fetch the datablock entries
        // look through current inodes data blocks
        for (i = 0; i < N_BLOCKS; i++)
        {
            // gets the starting offset of the datablocks, and adds the current block offset to get the datablock
            datablock = (struct wfs_dentry *)(file_system + super_block->d_blocks_ptr + curr_inode->blocks[i]);
            // compare the current path token to see if it exists in a datablock entry
            if (strcmp(datablock->name, token) == 0)
            {
                found = 1;
                break;
            }
        }
        // if the path was never found, it does not exist
        if (!found)
        {
            free(copy_path);
            return -1;
        }

        // if it is found, update the current node, TODO, make sure this arithmetic is correct.
        curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (datablock->num * sizeof(struct wfs_inode)));
    }
    // copy over relevant struct data
    // TODO make sure this is the correct inode that should be copied over.
    // also make sure this correctly copies it over
    memcpy(inode, curr_inode, sizeof(struct wfs_inode));
    free(copy_path);
    return 0;
}

// this fuse operation makes a directory
static int wfs_getattr(const char *path, struct stat *stbuf)
{
    // printf("entering getarr code\n");
    struct wfs_inode inode;
    if (get_inode(path, &inode) != 0)
    {
        return -ENOENT;
    }

    // TODO, add all needed attributes here
    stbuf->st_ino = inode.num; // not sure if this one is correct.
    stbuf->st_mode = inode.mode;
    stbuf->st_uid = inode.uid;
    stbuf->st_gid = inode.gid;
    stbuf->st_size = inode.size;
    stbuf->st_nlink = inode.nlinks;
    // time attributes
    stbuf->st_atime = inode.atim;
    stbuf->st_mtime = inode.mtim;
    stbuf->st_ctime = inode.ctim;
    stbuf->st_blocks = N_BLOCKS; // not sure if this one is write either, probably need to check which are allocated.

    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("\nentered mknod code function\n");
    //last slash before path has the new inode file name/location
    // ex: /siggy, here we would just grab siggy from this
    //or /siggy/adam, here we would just grab adam

    int last_slash_index;
    for(int i=0; i < strlen(path)-1; i++){
        if(path[i] == '/'){
            last_slash_index = i;
        }
    }
    //copy the string
    char* parent_path = strdup(path);
    //seperate the parent path and child path
    parent_path[last_slash_index+1] = '\0';
    char* file_name = strdup(path + last_slash_index+1);
    printf("The filename is: %s\n", file_name);
    printf("The parent path is: %s\n", parent_path);

    //check that parent path exists
    struct wfs_inode inode;
    if (get_inode(parent_path, &inode) != 0)
    {
        // printf("it does not exist, returnig.\n");
        return -ENOENT;
    }
    
    printf("it does exist\n");

    return 0; // Success
}

// static int wfs_mkdir(const char *path, mode_t mode) {
//     // Implementation of getattr function to retrieve file attributes
//     // Fill stbuf structure with the attributes of the file/directory indicated by path
//     // ...
//     printf("in mkdir\n");
//     return 0; // Return 0 on success
// }

// add fuse ops here
static struct fuse_operations ops = {
    // Add other functions (read, write, mkdir, etc.) here as needed
    // usage: .function_name = c_method_name
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    // .mkdir = wfs_mkdir,
};

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("USAGE: ./wfs disk_path [FUSE options] mount_point\n");
        exit(1);
    }

    // get disk image path and remove from fuse args
    char *disk_img_path = strdup(argv[1]);
    for (int i = strlen(disk_img_path); i > 0; i--)
    {
        // replace _ with .
        if (disk_img_path[i] == '_')
        {
            disk_img_path[i] = '.';
            break;
        }
    }

    // attempt to open disk img to verify path
    int fd = open(disk_img_path, O_RDWR);
    if (fd < 0)
    {
        printf("ERROR: cannot open disk image, verify the path.\nPATH GIVEN: %s\n", disk_img_path);
        exit(1);
    }
    struct stat stat;
    fstat(fd, &stat);
    off_t size = stat.st_size;

    // setup pointers
    file_system = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0); // Corrected mmap call
    super_block = (struct wfs_sb *)file_system;

    close(fd);
    // remove disk image path from args to give to fuse main
    char **fuse_args = (char **)malloc((argc - 1) * sizeof(char *));
    fuse_args[0] = argv[0];
    for (int i = 1; i < argc - 1; i++)
    {
        fuse_args[i] = argv[i + 1];
    }

    return fuse_main(argc - 1, fuse_args, &ops, NULL);
    ;
}
