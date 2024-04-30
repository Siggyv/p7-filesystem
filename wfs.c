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
    struct wfs_dentry *entry;
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

            for(int j = 0; j < BLOCK_SIZE; j+=sizeof(struct wfs_dentry))
            {
                entry = (struct wfs_dentry *)(file_system + curr_inode->blocks[i] + j);
                printf("Entry name: %s\n", entry->name);
                if(strcmp(entry->name, token) == 0)
                {
                    found = 1;
                    break;
                }
            }
            if(found)
            {
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
        curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (entry->num * sizeof(struct wfs_inode)));
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

    // printf("the inode number is (getattr): %d\n",inode->num);

    stbuf->st_ino = inode->num; // not sure if this one is correct.
    // printf("the inode number is (getattr): %ld\n", stbuf->st_ino);
    stbuf->st_mode = inode->mode;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_nlink = inode->nlinks;
    stbuf->st_blksize = BLOCK_SIZE;
    // time attributes
    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_ctime = inode->ctim;
    // calculate total number of blocks
    int num_blocks = 0;
    for (int i = 0; i < N_BLOCKS; i++)
    {
        if (inode->blocks[i] != 0)
        {
            num_blocks++;
        }
    }
    stbuf->st_blocks = num_blocks;

    return 0;
}

// allocates an inode, and returns pointer to it.
// sets basic attributes, inode num, uid, gid, time.
// if not enough space, returns NULL
struct wfs_inode *allocate_inode(mode_t mode)
{
    char * bitmap = super_block->i_bitmap_ptr;

    for(int i  = 0; )

    // if not enough space, will return null.
    return NULL;
}

// returns a offset from the datablock
// Otherwise returns -1 if no more space left
off_t allocate_datablock()
{
    

    return -1;
}

// inserts a entry into the given directory and returns 0
// if it is full, will return -1
int insert_entry_into_directory(struct wfs_inode *directory, char *file_name, int new_inode_num)
{
    // printf("the current node num is: %d\n", directory->num);
    int i = 0;
    // map the new inode in the parent directory
    // printf("Inserting for inode num: %d\n", directory->num);
    for (i = 0; i < N_BLOCKS; i++)
    {
        // printf("current block: %ld\n", directory->blocks[i]);
        // look to find a empty entry
        if (directory->blocks[i] == -1)
        {
            off_t new_datablock = allocate_datablock();
            directory->blocks[i] = new_datablock;
            struct wfs_dentry *data_entry = ((struct wfs_dentry *)(file_system + new_datablock));
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

char *get_parent_path(const char *path)
{
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

    return parent_path;
}

char *get_file_name(const char *path)
{
    int last_slash_index;
    for (int i = 0; i < strlen(path) - 1; i++)
    {
        if (path[i] == '/')
        {
            last_slash_index = i;
        }
    }
    char *file_name = strdup(path + last_slash_index + 1);
    return file_name;
}

int handle_inode_insertion(const char *path, mode_t mode)
{
    
    return 0;
}

static int wfs_mknod(const char *path, mode_t mode, dev_t dev)
{
    
    return 0; // Success
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    
    return 0; // Success
}

// finds and removes an entry given by name, returns 0
// if entry is not found, will return -1
int find_and_remove_data_entry_from_directory(struct wfs_inode *directory, char *file_name)
{
    
    return -1;
}

int handle_unlinking(const char *path)
{
    
    return 0;
}

/** Remove a file */
static int wfs_unlink(const char *path)
{
   
    return 0;
}

/** Remove a file */
static int wfs_rmdir(const char *path)
{
    int is_unlinked = handle_unlinking(path);

    if (is_unlinked != 0)
    {
        return is_unlinked;
    }
    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t offset, struct fuse_file_info *file_info)
{
    // printf("In readdir\n");
    struct wfs_inode *parent_dir = get_inode(path); // get parent_dir
    // printf("parent dir %s\n", path);
    if (parent_dir == NULL)
    {
        return -ENOENT;
    }
    // printf("After get inode\n");
    // check that it is directory
    if (!S_ISDIR(parent_dir->mode))
    {
        printf("%d", parent_dir->mode);
        printf("Must pass in a directory.\n");
        return 1;
    }

    return 0;
}

size_t min(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

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
static int wfs_read(const char *path, char *buf, size_t n, off_t offset, struct fuse_file_info * file) {
    printf("In read method\n");
    printf("offset passed %ld\n", offset);
    struct wfs_inode *file_node = get_inode(path);
    if (file_node == NULL)
    {
        printf("Invalid path: %s\n", path);
        return -ENOENT;
    }

    int num_blocks_to_read = n / BLOCK_SIZE;
    if (n % BLOCK_SIZE != 0)
    {
        num_blocks_to_read++;
    }

    // get start of datablocks
    char * datablocks = (char *)(file_system + super_block->d_blocks_ptr);
    size_t bytes_read = 0; // use to decide when to break
    size_t block_offset = offset % BLOCK_SIZE;
    int starting_block = offset / BLOCK_SIZE;
    for(int i = starting_block; i < N_BLOCKS; i++)
    {
        if(bytes_read == n)
        {
            break;
        }
        
        // for error checking, shouldnt have bytes_read be greater than size n
        if(bytes_read > n)
        {
            printf("read in too much...\n");
            break;
        }
        // printf("file node block: %d\n", file_node->blocks[i]);
        if(file_node->blocks[i] != -1)
        {
            // valid block so read in this block
            char * valid_block = datablocks + file_node->blocks[i] + block_offset;
            size_t length_left = file_node->size - offset - bytes_read;
            size_t end_length = min(length_left, n - bytes_read);
            size_t num_read = min(BLOCK_SIZE - block_offset, end_length); // do either whole block or whats left.
            printf("the valid block is: %s\n", valid_block);
            memcpy(buf + bytes_read, valid_block, num_read);
            printf("buf: %s\n", buf);
            bytes_read += num_read;
            block_offset = 0;            
        } else {
            break;
        }
    }

    printf("read over: %zu for %zu requested\n", bytes_read, n);
    return bytes_read;
}

// writes data to a inode
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // size is 1+ buffer
    printf("now entering write..\n");
    printf("the buff size is: %s\n", buf);
    printf("the size is: %ld\n", size);

    struct wfs_inode *inode = get_inode(path);
    if (inode == NULL)
    {
        return -ENOENT;
    }

    // this may be the wrong way to do it
    int data_blocks_needed = size / BLOCK_SIZE;
    if (size % BLOCK_SIZE != 0)
    {
        data_blocks_needed++;
    }
    off_t datablock;
    char *datablock_ptr;
    for (int i = 0; i < data_blocks_needed; i++)
    {
        // find a place to write to
        for (int j = 0; j < N_BLOCKS; j++)
            if (inode->blocks[j] == -1)
            {
                datablock = allocate_datablock();
                datablock_ptr = file_system + super_block->d_blocks_ptr + datablock;
                // copy over memory
                memcpy(datablock_ptr, buf + (i * BLOCK_SIZE), BLOCK_SIZE);
                printf("the data written is %s\n",datablock_ptr);
                // mark block as allocated
                inode->blocks[j] = datablock;
                // update size
                inode->size += BLOCK_SIZE;
                break;
            }
    }

    printf("everything written...\n");

    return size;
}

// add fuse ops here
static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod = wfs_mknod,
    .mkdir = wfs_mkdir,
    .unlink = wfs_unlink,
    .rmdir = wfs_rmdir,
    .read = wfs_read,
    .write = wfs_write,
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
    // initate root node cd . & cd ..
    struct wfs_inode *root_inode = get_inode("/");
    //insert cd .
    int is_inserted = insert_entry_into_directory(root_inode, ".", root_inode->num);
    if (is_inserted == -1)
    {
        printf("testing\n");
        return -ENOSPC;
    }

    // // insert cd ..
    // // is_inserted = insert_entry_into_directory(root_inode, "..", root_inode->num);
    // // if (is_inserted == -1)
    // // {
    // //     printf("testingb\n");
    //     return -ENOSPC;
    // // }

    close(fd);
    // remove disk image path from args to give to fuse main
    char **fuse_args = (char **)malloc((argc - 1) * sizeof(char *));
    fuse_args[0] = argv[0];
    for (int i = 1; i < argc - 1; i++)
    {
        fuse_args[i] = argv[i + 1];
    }
    // printf("the free inode is %d\n", allocate_inode()->num);
    for (int i = 0; i < argc - 1; i++)
    {
        printf("%s\n", fuse_args[i]);
    }
    return fuse_main(argc - 1, fuse_args, &ops, NULL);
}
