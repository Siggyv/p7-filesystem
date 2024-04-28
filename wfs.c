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

// return a pointer to inode, or NULL if not found
// fills up a given inode, given a path of a inode
struct wfs_inode *get_inode(const char *path)
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
            // printf("entering each block, current block: %ld\n", curr_inode->blocks[i]);
            if(curr_inode->blocks[i] == -1){
                continue;
            }
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
            return NULL;
        }

        // if it is found, update the current node, TODO, make sure this arithmetic is correct.
        curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (datablock->num * sizeof(struct wfs_inode)));
    }
    // return the address of the inode, and free memory
    free(copy_path);
    return curr_inode;
}

// this fuse operation makes a directory
static int wfs_getattr(const char *path, struct stat *stbuf)
{
    // printf("entering getarr code\n");
    struct wfs_inode *inode = get_inode(path);
    if (inode == NULL)
    {
        return -ENOENT;
    }

    printf("the inode number is: %d\n",inode->num);
    // TODO, add all needed attributes here
    stbuf->st_ino = inode->num; // not sure if this one is correct.
    stbuf->st_mode = inode->mode;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_nlink = inode->nlinks;
    // time attributes
    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_ctime = inode->ctim;
    stbuf->st_blocks = N_BLOCKS; // not sure if this one is write either, probably need to check which are allocated.

    return 0;
}

// allocates an inode, and returns pointer to it.
// sets basic attributes, inode num, uid, gid, time.
// if not enough space, returns NULL
struct wfs_inode *allocate_inode()
{
    off_t curr_inode_bit = super_block->i_bitmap_ptr+2; // need to start at 1 index (0 means empty)
    off_t upper_bound = super_block->d_bitmap_ptr;
    int free_inode_num;
    struct wfs_inode *inode_ptr;
    // printf("first inode: %d, start: %ld, end: %ld\n",*(file_system + curr_inode_bit),curr_inode_bit,upper_bound);

    // loop through each inode looking for a free spot.
    while (curr_inode_bit < upper_bound)
    {
        // find the first free inode
        if (*(file_system + curr_inode_bit) == 0)
        {
            // allocate the inode
            *(file_system + curr_inode_bit) = 1;
            // printf("curr inode is %ld", curr_inode_bit);
            free_inode_num = curr_inode_bit - super_block->i_bitmap_ptr;
            inode_ptr = (struct wfs_inode *)(file_system + (free_inode_num * sizeof(struct wfs_inode)));
            // update inode basic attributes
            printf("the inode number that is found free is: %d\n", free_inode_num);
            inode_ptr->num = free_inode_num;
            inode_ptr->uid = getuid();
            inode_ptr->gid = getgid();
            inode_ptr->size = 0; // initially empty
            inode_ptr->nlinks=0; // initially empty
            inode_ptr->atim = time(NULL);
            inode_ptr->mtim = time(NULL);
            inode_ptr->ctim = time(NULL);
            // returns the address of a inode pointer
            return inode_ptr;
        }
        curr_inode_bit++;
    }

    // if not enough space, will return null.
    return NULL;
}

// returns a offset from the datablock
// Otherwise returns -1 if no more space left
off_t allocate_datablock()
{
    off_t free_datablock;
    off_t curr_datablock_bit = super_block->d_bitmap_ptr;
    off_t upper_bound = super_block->i_blocks_ptr;

    while (curr_datablock_bit < upper_bound)
    {
        // look for the free data bit
        if (*(file_system + curr_datablock_bit) == 0)
        {
            // allocate the datablock bit
            *(file_system + curr_datablock_bit) = 1;
            // figure out which datablock this is (index)
            free_datablock = (curr_datablock_bit - super_block->d_bitmap_ptr) * BLOCK_SIZE;
            // calculate the address of this datablock
            return free_datablock;
        }

        curr_datablock_bit++;
    }

    return -1;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("\nentered mknod code function\n");
    // last slash before path has the new inode file name/location
    //  ex: /siggy, here we would just grab siggy from this
    // or /siggy/adam, here we would just grab adam

    int last_slash_index;
    for (int i = 0; i < strlen(path) - 1; i++)
    {
        if (path[i] == '/')
        {
            last_slash_index = i;
        }
    }
    // copy the string
    char *parent_path = strdup(path);
    // seperate the parent path and child path
    parent_path[last_slash_index + 1] = '\0';
    char *file_name = strdup(path + last_slash_index + 1);
    printf("The filename is: %s\n", file_name);
    printf("The parent path is: %s\n", parent_path);

    // get the parent, and allocate space for a new inode
    struct wfs_inode *parent = get_inode(parent_path);
    // check that parent path exists
    if (parent == NULL)
    {
        return -ENOENT;
    }

    // allocate the new inode
    struct wfs_inode *new_inode = allocate_inode();
    new_inode->mode = mode;

    int i = 0;
    // map the new inode in the parent directory
    for (i = 0; i < N_BLOCKS; i++)
    {
        if (parent->blocks[i] == -1)
        {
            break;
        }
    }
    off_t new_datablock = allocate_datablock();
    parent->blocks[i] = new_datablock;
    struct wfs_dentry *data_entry = ((struct wfs_dentry *)(file_system + super_block->d_blocks_ptr + new_datablock));
    // copy over name and inode number
    memcpy(data_entry->name, file_name, MAX_NAME);
    data_entry->num = new_inode->num;
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
    // printf("the free inode is %d\n", allocate_inode()->num);
    return fuse_main(argc - 1, fuse_args, &ops, NULL);
    ;
}
