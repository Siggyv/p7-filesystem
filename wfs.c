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
                // printf("Entry name: %s and the token: %s\n", entry->name, token);
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
    char * bitmap = file_system + super_block->i_bitmap_ptr;
    int idx; // used to keep track of free spot in bitmap
    struct wfs_inode * inode_ptr = NULL;
    for(int i  = 0; i < (super_block->num_inodes / 8); i++)
    {
        // search by byte over i_bitmap
        char * currByte = (bitmap + i);
        for(int j = 0; j < 8; j++)
        {
            // check if value is equal to 0
            if(((*currByte >> j) & 1) == 0)
            {
                // found free spot, set to 1 and create inode
                *currByte |= (1 << j);
                idx = (i * 8) + j;
                inode_ptr = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (idx * sizeof(struct wfs_inode)));
                // update inode basic attributes
                // printf("the inode number that is found free is: %d\n", free_inode_num);
                inode_ptr->num = idx;
                inode_ptr->mode = mode;
                inode_ptr->uid = getuid();
                inode_ptr->gid = getgid();
                inode_ptr->size = 0;   // initially empty
                inode_ptr->nlinks = 0; // initially empty
                inode_ptr->atim = time(NULL);
                inode_ptr->mtim = time(NULL);
                inode_ptr->ctim = time(NULL);
                // set all blocks to unallocated
                for (int k = 0; k < N_BLOCKS; k++)
                {
                    inode_ptr->blocks[k] = -1;
                }
                // returns the address of a inode pointer
                return inode_ptr;
            }
        }
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
    int i;
    int j;
    char *curr_data_byte;
    for (i = 0; i < super_block->num_data_blocks; i++)
    {

        // get the char pointer to this byte
        curr_data_byte = file_system + curr_datablock_bit + i;
        // loop over each bit, checking if they are 1
        for (j = 0; j < 8; j++)
        {
            // find first open spot
            if (((*(curr_data_byte) >> j) & 1) == 0)
            {
                // update this bit to allocated
                *(curr_data_byte) |= (1 << j);
                // calculate offset for this
                free_datablock = super_block->d_blocks_ptr + ((i * 8) + j) * BLOCK_SIZE;
                return free_datablock;
            }
        }
    }

    return -1;
}

// inserts a entry into the given directory and returns 0
// if it is full, will return -1
int insert_entry_into_directory(struct wfs_inode *directory, char *file_name, int new_inode_num)
{
    struct wfs_dentry * currBlock;
    // search through blocks to find viable spots in created blocks
    for (int i = 0; i < N_BLOCKS; i++)
    {
        // since searching left to right, check if not allocated first.
        if(directory->blocks[i] == -1)
        {
            off_t new_datablock = allocate_datablock();
            directory->blocks[i] = new_datablock;
            struct wfs_dentry * new_entry = (struct wfs_dentry *)(file_system + new_datablock);
            
            memcpy(new_entry->name, file_name, MAX_NAME);
            new_entry->num = new_inode_num;
            
            return 0;

        } else 
        {
            // if allocated search for open entry.
            currBlock = (struct wfs_dentry *) (file_system + directory->blocks[i]);

            // check within block for empty name, if empty set to new file name and num
            for(int j = 0; j < BLOCK_SIZE; j += sizeof(struct wfs_dentry))
            {
                currBlock = (struct wfs_dentry *)((char *)currBlock + j);
                if(strcmp(currBlock->name, "") == 0)
                {
                    memcpy(currBlock->name, file_name, MAX_NAME);
                    currBlock->num = new_inode_num;

                    return 0; // much success
                }
            }
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
    // last slash before path has the new inode file name/location
    //  ex: /siggy, here we would just grab siggy from this
    // or /siggy/adam, here we would just grab adam

    char *file_name = get_file_name(path);
    char *parent_path = get_parent_path(path);
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
    int handled_insertion = handle_inode_insertion(path, mode);
    if (handled_insertion != 0)
    {
        return handled_insertion;
    }
    return 0; // Success
}

static int wfs_mkdir(const char *path, mode_t mode)
{
    // printf("entering mkdir\n");
    // set the mode to directory
    mode |= S_IFDIR;
    int handled_insertion = handle_inode_insertion(path, mode);
    if (handled_insertion != 0)
    {
        return handled_insertion;
    }
    return 0; // Success
}

// finds and removes an entry given by name, returns 0
// if entry is not found, will return -1
int find_and_remove_data_entry_from_directory(struct wfs_inode *directory, char *file_name)
{
    // search for entry
    char * curr_datablock = file_system;
    for(int i = 0; i < N_BLOCKS; i++)
    {
        // continue over non-available data blocks.
        if(directory->blocks[i] == -1) continue;

        curr_datablock += directory->blocks[i];

        // loop over data entries to find within block
        for(int j = 0; j < BLOCK_SIZE; j+= sizeof(struct wfs_dentry))
        {
            printf("blah\n");
        }
    }
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
    // printf("In readdir: %s\n", path);
    struct wfs_inode *parent_dir = get_inode(path); // get parent_dir
    // printf("the inode is: %d\n", parent_dir->num);
  

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

    int i;
    int j;
    struct wfs_dentry *dentry;
    // loop over every block
    for (i = 0; i < N_BLOCKS; i++)
    {

        int d_offset = parent_dir->blocks[i];
        // skip over empty entries
        if (d_offset == 0)
        {
            // printf("d_offset: %d\n", d_offset);
            continue;
        }

        // every block (512 bytes) has 16 possible dentries.
        for (j = 0; j < BLOCK_SIZE; j+=sizeof(struct wfs_dentry))
        {
            // calculate the address of the entry
            dentry = (struct wfs_dentry *)(file_system + d_offset + j);
            // printf("block: %d, offset: %d dentry is: %s\n", i,j,dentry->name);
            // if it is not an empty string (valid), add it
            if (strcmp(dentry->name, "") != 0)
            {
                printf("dentry: %s\n", dentry->name);
                // get statbuf for fill
                struct stat *statbuf = (struct stat *)malloc(sizeof(struct stat));
                // concat the entire path, path/dentry
                char subfile_path[MAX_NAME + 2 + strlen(path)];
                strcpy(subfile_path, path);
                strcat(subfile_path, "/");
                strcat(subfile_path, dentry->name);
                // fill the statbuf with the stats about this node
                if (wfs_getattr(subfile_path, statbuf) != 0)
                {
                    printf("ERROR with getattr\n");
                    return 1;
                }

                // add it to the buffer
                if (fill(buf, dentry->name, statbuf, 0) != 0)
                {
                    printf("Buffer is full...\n");
                    return 1;
                }
            }
        }
    }
    // printf("finish readdir\n");
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
    // struct wfs_inode *root_inode = get_inode("/");
    // for (int i = 0; i < N_BLOCKS; i++)
    // {
    //     printf("the current block is %ld\n",root_inode->blocks[i]);
    // }
    // initate root node cd . & cd ..
    // struct wfs_inode *root_inode = get_inode("/");
    // zero it out
    // for (int i = 0; i < N_BLOCKS; i++)
    // {
    //     root_inode->blocks[i] = -1; // updated as a block at offset 0 is possible.
    // }
    // insert cd .
    // insert_entry_into_directory(root_inode, ".", root_inode->num);
    // if (is_inserted == -1)
    // {
    //     return -ENOSPC;
    // }

    // // insert cd ..
    // // // insert_entry_into_directory(root_inode, "..", root_inode->num);
    // // // if (is_inserted == -1)
    // // // {
    // // //     printf("testingb\n");
    //     return -ENOSPC;
    // // // }

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
