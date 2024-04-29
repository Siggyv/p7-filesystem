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
            if (curr_inode->blocks[i] == -1)
            {
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
        // printf("the current node is found, and its num is: %d\n", curr_inode->num);
    }
    // printf("the current node is found, and its num is: %d\n", curr_inode->num);
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

    // printf("the path is %s, and the inode fetched num is: %d, and uid: %d\n", path, inode->num, inode->uid);

    // printf("the inode number is: %d\n",inode->num);
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
struct wfs_inode *allocate_inode(mode_t mode)
{
    off_t curr_inode_bit = super_block->i_bitmap_ptr + 1; // need to start at 1 index (0 means empty)
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
            inode_ptr = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (free_inode_num * sizeof(struct wfs_inode)));
            // update inode basic attributes
            // printf("the inode number that is found free is: %d\n", free_inode_num);
            inode_ptr->num = free_inode_num;
            inode_ptr->mode = mode;
            inode_ptr->uid = getuid();
            inode_ptr->gid = getgid();
            inode_ptr->size = 0;   // initially empty
            inode_ptr->nlinks = 0; // initially empty
            inode_ptr->atim = time(NULL);
            inode_ptr->mtim = time(NULL);
            inode_ptr->ctim = time(NULL);
            // set all blocks to unallocated
            for (int i = 0; i < N_BLOCKS; i++)
            {
                inode_ptr->blocks[i] = -1;
            }
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

// inserts a entry into the given directory and returns 0
// if it is full, will return -1
int insert_entry_into_directory(struct wfs_inode *directory, char *file_name, int new_inode_num)
{
    int i = 0;
    // map the new inode in the parent directory
    for (i = 0; i < N_BLOCKS; i++)
    {
        // look to find a empty entry
        if (directory->blocks[i] == -1)
        {
            off_t new_datablock = allocate_datablock();
            directory->blocks[i] = new_datablock;
            struct wfs_dentry *data_entry = ((struct wfs_dentry *)(file_system + super_block->d_blocks_ptr + new_datablock));
            // copy over name and inode number
            memcpy(data_entry->name, file_name, MAX_NAME);
            data_entry->num = new_inode_num;
            return 0;
        }
    }
    return -1;
}

// handles the basic inode insertion
// creating inode, making space for it
// and adding the inode to the parent directory
int handle_inode_insertion(const char *path, mode_t mode)
{
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
    char *parent_path = strdup(path);
    // seperate the parent path and child path
    parent_path[last_slash_index + 1] = '\0';
    char *file_name = strdup(path + last_slash_index + 1);
    // get the parent, and allocate space for a new inode
    struct wfs_inode *parent = get_inode(parent_path);
    // check that parent path exists
    if (parent == NULL)
    {
        return -ENOENT;
    }

    // allocate the new inode
    struct wfs_inode *new_inode = allocate_inode(mode);
    // make sure their is sufficient space for the inode
    if (new_inode == NULL)
    {
        return -ENOSPC;
    }

    // insert the new node into the parent directory
    int is_inserted = insert_entry_into_directory(parent, file_name, new_inode->num);
    if (is_inserted == -1)
    {
        return -ENOSPC;
    }

    // if it is a directory being inserted in, also need to add links for
    //  cd . & cd ..
    // TODO, figure out if 509 is correct
    if (S_ISDIR(mode))
    {
        // insert cd .
        is_inserted = insert_entry_into_directory(new_inode, ".", new_inode->num);
        if (is_inserted == -1)
        {
            return -ENOSPC;
        }
        // insert cd ..
        is_inserted = insert_entry_into_directory(new_inode, "..", parent->num);
        if (is_inserted == -1)
        {
            return -ENOSPC;
        }
        // printf("I made it here for mkdir.... path was: %s and mode was %d\n", file_name, mode);
    }
    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    printf("entering mknod\n");
    int handled_insertion = handle_inode_insertion(path, mode);
    if (handled_insertion != 0)
    {
        return handled_insertion;
    }
    return 0; // Success
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    printf("entering mkdir\n");
    mode |= S_IFDIR;
    int handled_insertion = handle_inode_insertion(path, mode);
    if (handled_insertion != 0)
    {
        return handled_insertion;
    }
    return 0;
    
}

// static int wfs_mkdir(const char *path, mode_t mode) {
//     // Implementation of getattr function to retrieve file attributes
//     // Fill stbuf structure with the attributes of the file/directory indicated by path
//     // ...
//     printf("in mkdir\n");
//     return 0; // Return 0 on success
// }

	/** Read data from an open file
	 *
	 * Read should return exactly the number of bytes requested except
	 * on EOF or error, otherwise the rest of the data will be
	 * substituted with zeroes.	 An exception to this is when the
	 * 'direct_io' mount option is specified, in which case the return
	 * value of the read system call will reflect the return value of
	 * this operation.
	 *
	 * Changed in version 2.2
	 */
//static int wfs_read(const char *path, char *buf, size_t n, off_t offset, struct fuse_file_info * file) {
  //  printf("In read...\n");




  //  printf("Finished read...\n");
  //  return 0; 
//}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t offset, struct fuse_file_info *file_info) {
    printf("In readdir\n");
    struct wfs_inode * parent_dir = get_inode(path); // get parent_dir
    printf("parent dir %p\n", (void *)parent_dir);
    if(parent_dir == NULL) {
        return -ENOENT;
    }
    printf("After get inode\n");
    // check that it is directory
    if(!S_ISDIR(parent_dir->mode)){
        printf("%d", parent_dir->mode);
        printf("Must pass in a directory.\n");
        return 1;
    }

    // read in names, check d bitmap, once a zero is reached break out of while True
    char * d_bitmap = file_system + super_block->d_bitmap_ptr;
    char * data_blocks = file_system + super_block->d_blocks_ptr;
    int safety_counter = 0;

    for(int i = 0; i < N_BLOCKS; i++) {
        printf("%d", *(d_bitmap + i));
    }
    printf("\n");
    // initially was a while true, but thought this would be better to avoid worse case scenarios.
    for(int i = 0; i < N_BLOCKS; i++) {
        // get correct bitmap number
        int d_offset = parent_dir->blocks[i];
    }
    printf("End of readdir\n");
    return 0;
}

// add fuse ops here
static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  //.unlink  = wfs_unlink,
  //.rmdir   = wfs_rmdir,
  //.read    = wfs_read,
  //.write   = wfs_write,
  .readdir = wfs_readdir,
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
    /*
    for (int i = strlen(disk_img_path); i > 0; i--)
    {
        // replace _ with .
        if (disk_img_path[i] == '_')
        {
            disk_img_path[i] = '.';
            break;
        }
    }
    */

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
}
