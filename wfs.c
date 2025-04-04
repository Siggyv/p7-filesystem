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

// bitmap is the specific bitmap pointer
// value is the value to set in the idx specified
// isBlocks is a boolean to decide which number of inodes or number of blocks to use.
int setbitmap(char *bitmap, int value, int idx, int isBlocks)
{
    int outer_iter = (isBlocks) ? super_block->num_data_blocks : super_block->num_inodes;
    for (int i = 0; i < (outer_iter / 8); i++)
    {
        char *currByte = bitmap + i;
        for (int j = 0; j < 8; j++)
        {
            if ((i * 8 + j) == idx)
            {
                if (value == 1)
                {
                    *(currByte) |= (value << j);
                }
                else
                {
                    *(currByte) &= ~(1 << j);
                }
                return value;
            }
        }
    }
    return -1;
}

// return a pointer to inode, or NULL if not found
// fills up a given inode, given a path of a inode
struct wfs_inode *get_inode(const char *path)
{

    // printf("the path is: %s\n", path);
    // fetch the root inode
    struct wfs_inode *curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr);
    // printf("the root inode is: %d\n", curr_inode->num);
    // printf("size of size: %ld\n", curr_inode->size);
    // printf("size of links: %d\n", curr_inode->nlinks);
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
            if (curr_inode->blocks[i] == 0)
            {
                continue;
            }

            for (int j = 0; j < BLOCK_SIZE; j += sizeof(struct wfs_dentry))
            {
                entry = (struct wfs_dentry *)(file_system + curr_inode->blocks[i] + j);
                // printf("Entry name: %s and the token: %s\n", entry->name, token);
                if (strcmp(entry->name, token) == 0)
                {
                    found = 1;
                    break;
                }
            }
            if (found)
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

        // printf("the entry num: %d\n", entry->num);
        // if it is found, update the current node, TODO, make sure this arithmetic is correct.
        curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (entry->num * BLOCK_SIZE));
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
    char *bitmap = file_system + super_block->i_bitmap_ptr;
    int idx; // used to keep track of free spot in bitmap
    struct wfs_inode *inode_ptr = NULL;
    for (int i = 0; i < (super_block->num_inodes / 8); i++)
    {
        // search by byte over i_bitmap
        char *currByte = (bitmap + i);
        for (int j = 0; j < 8; j++)
        {
            // check if value is equal to 0
            if (((*currByte >> j) & 1) == 0)
            {
                // found free spot, set to 1 and create inode
                *currByte |= (1 << j);
                idx = (i * 8) + j;
                inode_ptr = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (idx * BLOCK_SIZE));
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
                    inode_ptr->blocks[k] = 0;
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
    int i;
    int j;
    char *curr_data_byte;
    for (i = 0; i < (super_block->num_data_blocks / 8); i++)
    {

        // get the char pointer to this byte
        curr_data_byte = file_system + super_block->d_bitmap_ptr + i;
        // loop over each bit, checking if they are 1
        for (j = 0; j < 8; j++)
        {
            // find first open spot
            if (((*(curr_data_byte) >> j) & 1) == 0)
            {

                // update this bit to allocated
                // printf("\n\nopen idx: %d\n\n", (i*8 + j));
                setbitmap((file_system + super_block->d_bitmap_ptr), 1, (i * 8 + j), 1);
                // calculate offset for this
                free_datablock = super_block->d_blocks_ptr + ((i * 8) + j) * BLOCK_SIZE;
                // memset to 0
                memset(file_system + free_datablock, 0, BLOCK_SIZE);
                return free_datablock;
            }
        }
    }
    printf("this is happening in bitmap");

    return -1;
}

// inserts a entry into the given directory and returns 0
// if it is full, will return -1
int insert_entry_into_directory(struct wfs_inode *directory, char *file_name, int new_inode_num, mode_t mode)
{
    struct wfs_dentry *currBlock;
    int n_blocks = 0;
    if (S_ISDIR(mode))
    {
        n_blocks = N_BLOCKS;
    }
    else
    {
        // for a regular file
        n_blocks = N_BLOCKS - 1;
    }
    // search through blocks to find viable spots in created blocks
    for (int i = 0; i < n_blocks; i++)
    {
        // since searching left to right, check if not allocated first.
        if (directory->blocks[i] == 0)
        {
            off_t new_datablock = allocate_datablock();
            if (new_datablock == -1)
            {
                return -ENOSPC;
            }
            directory->blocks[i] = new_datablock;
            struct wfs_dentry *new_entry = (struct wfs_dentry *)(file_system + new_datablock);

            strcpy(new_entry->name, file_name);
            new_entry->num = new_inode_num;

            return 0;
        }
        else
        {
            // if allocated search for open entry.

            // check within block for empty name, if empty set to new file name and num
            for (int j = 0; j < BLOCK_SIZE; j += sizeof(struct wfs_dentry))
            {
                currBlock = (struct wfs_dentry *)(file_system + directory->blocks[i] + j);
                if (strcmp(currBlock->name, "") == 0)
                {
                    strcpy(currBlock->name, file_name);
                    // printf("the curr block name is: %s\n", currBlock->name);
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
    // printf("the parent path is: %s\n", parent_path);
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
    int is_inserted = insert_entry_into_directory(parent, file_name, new_inode->num, mode);
    if (is_inserted == -1)
    {
        printf("erere\n");
        return -ENOSPC;
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
int delete(struct wfs_inode *directory, char *file_name, int is_directory)
{

    for (int i = 0; i < N_BLOCKS; i++)
    {
        // continue over non-available data blocks.
        if (directory->blocks[i] == 0)
            continue;

        // loop over data entries to find within block
        for (int j = 0; j < BLOCK_SIZE; j += sizeof(struct wfs_dentry))
        {
            struct wfs_dentry *entry = (struct wfs_dentry *)(file_system + directory->blocks[i] + j);

            if (strcmp(entry->name, file_name) == 0)
            {
                // found so delete entry (set it to empty)
                strcpy(entry->name, "");
                // free from parents
                struct wfs_inode *curr_inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr + (entry->num * BLOCK_SIZE));

                // free inode
                setbitmap(file_system+  super_block->i_bitmap_ptr, 0, curr_inode->num, 1);

                // if it is a directory, loop through all blocks and set to 0
                if (is_directory)
                {
                    // free the direct pointers of this inode
                    for (int k = 0; k < N_BLOCKS; k++)
                    {
                        if (curr_inode->blocks[k] != 0)
                        {
                            // set the dbitmaps
                            setbitmap( file_system+ super_block->d_bitmap_ptr, 0, (curr_inode->blocks[k] - super_block->d_blocks_ptr) / 512, 1);
                        }
                    }
                    return 0;
                }

                // free the direct pointers of this inode
                for (int k = 0; k < N_BLOCKS - 1; k++)
                {
                    if (curr_inode->blocks[k] != 0)
                    {
                        // set the dbitmaps
                        setbitmap(file_system+  super_block->d_bitmap_ptr, 0, (curr_inode->blocks[k] - super_block->d_blocks_ptr) / 512, 1);
                    }
                }

                // free the indirect pointers
                if (curr_inode->blocks[7] != 0)
                {
                    off_t *offsets = (off_t *)(file_system + curr_inode->blocks[7]);
                    // free every indirect block
                    for (int k = 0; k < N_BLOCKS; k++)
                    {
                        if (offsets[k] != 0)
                        {
                            // set the dbitmaps
                            setbitmap(file_system+  super_block->d_bitmap_ptr, 0, (offsets[k] - super_block->d_blocks_ptr) / 512, 1);
                        }
                    }
                }

                return 0;
            }
        }
    }
    return 0;
}

int handle_unlinking(const char *path, int is_directory)
{
    char *parent_path = get_parent_path(path);
    struct wfs_inode *parent = get_inode(parent_path);

    char *file_name = get_file_name(path);
    // struct wfs_inode *inode = get_inode(file_name);
    // if it is a directory, first need to remove cd . & cd ..
    int is_unlinked;

    // unlink from parent directory, remove entry from data bitmap and inode bitmap
    is_unlinked = delete (parent, file_name,is_directory);
    if (is_unlinked == -1)
    {
        return -EEXIST;
    }

    //  unallocate it in the inode bitmap
    // //set this inode to free in inode bitmap
    // printf("the i node number is: %d and its allocationow haven: %d\n", inode->num, *(file_system + super_block->i_bitmap_ptr + inode->num));
    // setbitmap((char *)(file_system + super_block->i_bitmap_ptr), 0, inode->num, 0);
    // printf("the i node number is: %d and its allocation: %d\n", inode->num, *(file_system + super_block->i_bitmap_ptr + inode->num));
    // printf("now have freed the inode\n");

    return 0;
}

/** Remove a file */
static int wfs_unlink(const char *path)
{
    int is_unlinked = handle_unlinking(path, 0);

    if (is_unlinked != 0)
    {
        return is_unlinked;
    }

    return 0;
}

/** Remove a file */
static int wfs_rmdir(const char *path)
{
    int is_unlinked = handle_unlinking(path,1);

    if (is_unlinked != 0)
    {
        return is_unlinked;
    }
    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t offset, struct fuse_file_info *file_info)
{
    // printf("In readdir: %s\n", path);
    struct wfs_inode *directory = get_inode(path); // get parent_dir

    if (directory == NULL)
    {
        return -ENOENT;
    }
    // printf("After get inode\n");
    // check that it is directory
    if (!S_ISDIR(directory->mode))
    {
        printf("%d", directory->mode);
        printf("Must pass in a directory.\n");
        return 1;
    }

    // add cd . and cd ..
    char *cd[2] = {".", ".."};
    char path_checked[30];
    for (int i = 0; i < 2; i++)
    {

        struct stat *statbuf = (struct stat *)malloc(sizeof(struct stat));
        // concat the entire path, path/dentry
        // cd .
        if (i == 0)
        {
            strcpy(path_checked, path);
        }
        // cd ..
        else
        {
            char *parent_path = get_parent_path(path);
            strcpy(path_checked, parent_path);
        }

        if (wfs_getattr(path_checked, statbuf) != 0)
        {
            printf("ERROR with getattr\n");
            return 1;
        }

        // add it to the buffer
        if (fill(buf, cd[i], statbuf, 0) != 0)
        {
            printf("Buffer is full...\n");
            return 1;
        }
    }

    int i;
    int j;
    struct wfs_dentry *dentry;
    // loop over every block
    for (i = 0; i < N_BLOCKS; i++)
    {

        int d_offset = directory->blocks[i];
        // skip over empty entries
        if (d_offset == 0)
        {
            // printf("d_offset: %d\n", d_offset);
            continue;
        }

        // every block (512 bytes) has 16 possible dentries.
        for (j = 0; j < BLOCK_SIZE; j += sizeof(struct wfs_dentry))
        {

            // calculate the address of the entry
            dentry = (struct wfs_dentry *)(file_system + d_offset + j);
            // printf("block: %d, offset: %d dentry is: %s\n", i,j,dentry->name);

            // if it is not an empty string (valid), add it
            if (strcmp(dentry->name, "") != 0)
            {
                struct stat *statbuf = (struct stat *)malloc(sizeof(struct stat));
                // concat the entire path, path/dentry
                char subfile_path[MAX_NAME + 2 + strlen(path)];
                strcpy(subfile_path, path);
                strcat(subfile_path, "/");
                strcat(subfile_path, dentry->name);
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
    // printf("finished readdir\n");
    return 0;
}

size_t min(size_t a, size_t b)
{
    return (a < b) ? a : b;
}

static int wfs_read(const char *path, char *buf, size_t n, off_t offset, struct fuse_file_info *file)
{
    // printf("In read method\n");
    // printf("offset passed %ld\n", offset);
    // printf("size passed %ld\n", n);
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
    // char * datablocks = (char *)(file_system + super_block->d_blocks_ptr);
    size_t bytes_read = 0; // use to decide when to break
    size_t block_offset = offset % BLOCK_SIZE;
    int starting_block = offset / BLOCK_SIZE;
    for (int i = starting_block; i < N_BLOCKS; i++)
    {
        if (bytes_read == n)
        {
            break;
        }

        // for error checking, shouldnt have bytes_read be greater than size n
        if (bytes_read > n)
        {
            printf("read in too much...\n");
            break;
        }
        // printf("file node block: %ld\n", file_node->blocks[i]);
        if (file_node->blocks[i] != 0)
        {
            // valid block so read in this block
            if (i == N_BLOCKS - 1)
            {
                off_t *indirect_ptr = (off_t *)(file_system + file_node->blocks[i]);
                // loop over off_ts starting at indirect_ptr, for each one read the block
                for (int j = 0; j < (BLOCK_SIZE / sizeof(off_t)); j++)
                {
                    if (indirect_ptr[j] == 0)
                        continue;

                    char *indirect_block = file_system + indirect_ptr[j] + block_offset; // offset should be zero if this is not the first block otherwise adjust

                    size_t file_bytes_left = file_node->size - offset - bytes_read;
                    size_t remianing_bytes = min(file_bytes_left, n - bytes_read);     // whats shorter length till end of file or bytes still requested
                    size_t read_num = min(BLOCK_SIZE - block_offset, remianing_bytes); // if block offset is zero, then it should either write block size or whats left

                    memcpy(buf + bytes_read, indirect_block, read_num);
                    bytes_read += read_num;
                    block_offset = 0; // to only apply offset once
                }
            }
            else
            {
                char *valid_block = file_system + file_node->blocks[i] + block_offset;

                size_t length_left = file_node->size - offset - bytes_read;
                size_t end_length = min(length_left, n - bytes_read);
                size_t num_read = min(BLOCK_SIZE - block_offset, end_length); // do either whole block or whats left.

                memcpy(buf + bytes_read, valid_block, num_read);
                bytes_read += num_read;
                block_offset = 0;
            }
        }
    }

    // printf("read over: %zu for %zu requested\n", bytes_read, n);
    return bytes_read;
}

// writes data to a inode
static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // printf("now entering write..\n");

    struct wfs_inode *inode = get_inode(path);
    if (inode == NULL)
    {
        return -ENOENT;
    }

    off_t datablock;
    char *datablock_ptr;

    // if offset is empty,
    off_t starting_block = offset / BLOCK_SIZE;
    off_t starting_offset = offset % BLOCK_SIZE;
    // printf("starting block: %ld starting offset is: %ld vs offset: %ld and the size is: %ld\n", starting_block, starting_offset, offset, size);
    off_t offset_in_buffer = 0;
    // this may be the wrong way to do it
    size_t data_left_to_write = size;
    // loop over first 7 blocks
    // start at the block where the size ends, that is there is open space
    // loop over all blocks, looking for a frees
    // for (int i = 0; i < N_BLOCKS - 1; i++)
    // {
    //     printf("this block is: %ld\n", inode->blocks[i]);
    // }

    for (int i = starting_block; i < N_BLOCKS - 1; i++)
    {
        // no more blocks needed
        if (data_left_to_write <= 0)
        {
            break;
        }
        // if the offset it zero, need to allocate the block
        if (inode->blocks[i] == 0)
        {
            // printf("allocating a datablock in write, iteration: %d\n", i);
            datablock = allocate_datablock();
            if (datablock == -1)
            {
                return -ENOSPC;
            }
            inode->blocks[i] = datablock;
        }
        // otherwise, grab the data at the address
        else
        {
            datablock = inode->blocks[i] + (starting_offset % BLOCK_SIZE);
        }

        datablock_ptr = file_system + datablock;

        // calculate how much can be fit into this pointer
        size_t bytes_left_in_block = BLOCK_SIZE - ((datablock - super_block->d_blocks_ptr) % BLOCK_SIZE);
        size_t bytes_to_be_written = size - offset_in_buffer;
        size_t bytes_that_will_be_written = min(bytes_left_in_block, bytes_to_be_written);
        // printf("BLOCK: %d, BYTES LEFT IN BLOCK: %ld, BYTES TO BE WRITTEN: %ld, BYTES THAT WILL BE WRITTEN %ld WRITTEN TO: %ld\n",
        // i, bytes_left_in_block, bytes_to_be_written, bytes_that_will_be_written, datablock);

        // copy over memory
        memcpy(datablock_ptr, buf + offset_in_buffer, bytes_that_will_be_written);

        // mark block as allocated

        // update size
        inode->size += bytes_that_will_be_written;
        offset_in_buffer += bytes_that_will_be_written;
        starting_offset += bytes_that_will_be_written;
        starting_block++;
        data_left_to_write -= bytes_that_will_be_written;
    }

    // if there is still more, use an indirect block
    if (data_left_to_write > 0)
    {
        // check to see if the block is currently free
        // if it is not, allocate it
        if (inode->blocks[7] == 0)
        {
            inode->blocks[7] = allocate_datablock();
            if (inode->blocks[7] == -1)
            {
                return -ENOSPC;
            }
        }
        // find the start inside of the indirect blocks
        int index_in_indirect = starting_block - 7;
        // printf("the starting block is: %ld\n", starting_block);
        // printf("the current index is:%d\n ", index_in_indirect);
        // loop through each block in the indirect block
        printf("size of an offse it: %ld\n", sizeof(off_t));
        off_t *offsets = (off_t *)(file_system + inode->blocks[7]);

        for (int i = index_in_indirect; i < BLOCK_SIZE / sizeof(off_t); i++)
        {
            // no more blocks needed
            if (data_left_to_write <= 0)
            {
                break;
            }
            // if the offset it zero, need to allocate the block
            if (offsets[i] == 0)
            {
                // printf("allocating a datablock in write, iteration: %d\n", i);
                datablock = allocate_datablock();
                if (datablock == -1)
                {
                    return -ENOSPC;
                }
                offsets[i] = datablock;
            }
            // otherwise, grab the data at the address
            else
            {
                datablock = offsets[i] + (starting_offset % BLOCK_SIZE);
            }

            datablock_ptr = file_system + datablock;

            // calculate how much can be fit into this pointer
            size_t bytes_left_in_block = BLOCK_SIZE - ((datablock - super_block->d_blocks_ptr) % BLOCK_SIZE);
            size_t bytes_to_be_written = size - offset_in_buffer;
            size_t bytes_that_will_be_written = min(bytes_left_in_block, bytes_to_be_written);
            // printf("BLOCK: %d, BYTES LEFT IN BLOCK: %ld, BYTES TO BE WRITTEN: %ld, BYTES THAT WILL BE WRITTEN %ld WRITTEN TO: %ld\n",
            // i, bytes_left_in_block, bytes_to_be_written, bytes_that_will_be_written, datablock);

            // copy over memory
            memcpy(datablock_ptr, buf + offset_in_buffer, bytes_that_will_be_written);

            // mark block as allocated

            // update size
            inode->size += bytes_that_will_be_written;
            offset_in_buffer += bytes_that_will_be_written;
            starting_offset += bytes_that_will_be_written;
            starting_block++;
            data_left_to_write -= bytes_that_will_be_written;
        }
    }
    if (data_left_to_write > 0)
    {
        return -ENOSPC;
    }
    // printf("everything written...\n");
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

    printf("the super block inode count is: %ld\n", super_block->num_inodes);
    printf("super block dblock count: %ld\n", super_block->num_data_blocks);
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
