#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "wfs.h"
#include <sys/mman.h>

// PRESUMING: You may presume the block size is always 512 bytes (according to instructions)

int main(int argc, char **argv)
{
    int num_blocks = -1;
    int num_inodes = -1;
    char *DISK_IMG_PATH = NULL;

    // argument parsing
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            char DISK_IMG_PATH[28];

            strcpy(DISK_IMG_PATH,"disk.img");

            printf("the disk image path is: %s\n", DISK_IMG_PATH);
        }
        else if (strcmp(argv[i], "-i") == 0)
        {
            num_inodes = atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            num_blocks = atoi(argv[i + 1]);
        }
    }

    // round up num blocks to nearest higher multiple of 32
    if (num_blocks % 32 != 0)
        num_blocks = (num_blocks - num_blocks % 32) + 32;
    if (num_inodes % 32 != 0)
        num_inodes = (num_inodes - num_inodes % 32) + 32;


    printf("num blocks is %d, num nodes is %d\n", num_blocks, num_inodes);


    // initialize super block
    // open disk img
    int fd = open(DISK_IMG_PATH, RD);
    if (fd < 0)
    {
        perror("ERROR: failed to open disk image for initialization.\n");

        exit(1);
    }

    struct stat stat;
    fstat(fd, &stat);
    off_t size = stat.st_size;

    // setup pointers
    char *file_system = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    memset(file_system, 0, size);

    // define super block
    struct wfs_sb *super_block = (struct wfs_sb *)file_system;
    off_t i_map_ptr = sizeof(struct wfs_sb);
    off_t d_map_ptr = i_map_ptr + (num_inodes / 8);
    off_t i_block_ptr = d_map_ptr + (num_blocks / 8);
    off_t d_block_ptr = i_block_ptr + (BLOCK_SIZE * num_inodes);
    super_block->num_data_blocks = num_blocks;
    super_block->num_inodes = num_inodes;
    super_block->i_bitmap_ptr = i_map_ptr;
    super_block->d_bitmap_ptr = d_map_ptr;
    super_block->i_blocks_ptr = i_block_ptr;
    super_block->d_blocks_ptr = d_block_ptr;

    // close files
    close(fd);
    free(DISK_IMG_PATH);

    // initialize root inode; CONSIDER: may need to zero the bitmaps, dont think so so I wont do it.
    struct wfs_inode *inode = (struct wfs_inode *)(file_system + super_block->i_blocks_ptr);
    inode->num = 0;               // NOTE ROOT DIRECTORY IS FIRST INODE
    inode->mode = S_IFDIR | 0755; // directory with rwx for all users. Piazza said it doesnt really matter what permissions we give it.
    inode->uid = 0;
    inode->gid = 0;
    inode->size = 2 * sizeof(struct wfs_dentry); // for . and .. dirs
    inode->nlinks = 2;
    inode->atim = inode->mtim = inode->ctim = time(NULL);
    for (int i = 0; i < N_BLOCKS; i++)
    {
        inode->blocks[i] = 0; // updated as a block at offset 0 is possible.
    }

    // set IBITMAP to 1 for the first spot for the root inode
    *(file_system + super_block->i_bitmap_ptr) |= 1;

    return 0;
}