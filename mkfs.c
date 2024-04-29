#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "wfs.h"

// PRESUMING: You may presume the block size is always 512 bytes (according to instructions)

int main(int argc, char ** argv) {
    int num_blocks = -1;
    int num_inodes = -1;
    char * DISK_IMG_PATH = NULL;

    // argument parsing
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-d") == 0) {
            DISK_IMG_PATH = (char *)malloc(strlen(argv[i+1]) + strlen("./") + 1);
            if(DISK_IMG_PATH == NULL) {
                perror("ERROR: could not get memory for the disk image path.\n");
                exit(1);
            }
            strcpy(DISK_IMG_PATH, "./");
            strcat(DISK_IMG_PATH, argv[i+1]);
            // replace all _ with a . go backwards, and only do once.
            for(int j = strlen(DISK_IMG_PATH); j > 0; j--) 
            {
                if(DISK_IMG_PATH[j] == '_')
                {
                    DISK_IMG_PATH[j] = '.';
                }
            }
            i++; // skip the next arg
        }
        else if(strcmp(argv[i], "-i") == 0) {
            num_inodes = atoi(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i], "-b") == 0) {
            num_blocks = atoi(argv[i+1]);
            i++;
        }
    }
    if(num_blocks == -1 || num_inodes == -1 || DISK_IMG_PATH == NULL) {
        printf("USAGE: -d {fs img path} -i {number i nodes} -b {number data blocks}\n");
        if(DISK_IMG_PATH != NULL) {
            free(DISK_IMG_PATH);
        }
        exit(1);
    }

    // round up num blocks to nearest higher multiple of 32
    if(num_blocks % 32 != 0)
        num_blocks = (num_blocks - num_blocks % 32) + 32;
    if(num_inodes % 32 != 0)
        num_inodes = (num_inodes - num_inodes % 32) + 32;

    // check that num of blocks is possible with disk img file size
    struct stat statbuf;
    if(stat(DISK_IMG_PATH, &statbuf) != 0) {
        perror("ERROR: cannot get disk image size.\n");
        free(DISK_IMG_PATH);
        exit(1);
    }

    if(num_blocks * BLOCK_SIZE > statbuf.st_size) {
        perror("ERROR: block size is too large to fit in disk image.\n");
        free(DISK_IMG_PATH);
        exit(1);
    }

    // initialize super block
    // open disk img
    int fd = open(DISK_IMG_PATH, O_RDWR);
    if(fd < 0) {
        perror("ERROR: failed to open disk image for initialization.\n");
        exit(1);
    }

    // define super block
    struct wfs_sb * superblock = (struct wfs_sb *)malloc(sizeof(struct wfs_sb));
    off_t i_map_ptr = sizeof(struct wfs_sb);
    off_t d_map_ptr = i_map_ptr + (num_inodes/8);
    off_t i_block_ptr = d_map_ptr + (num_blocks/8);
    off_t d_block_ptr = i_block_ptr + (BLOCK_SIZE * num_inodes);
    superblock->num_data_blocks = num_blocks;
    superblock->num_inodes = num_inodes;
    superblock->i_bitmap_ptr = i_map_ptr;
    superblock->d_bitmap_ptr = d_map_ptr;
    superblock->i_blocks_ptr = i_block_ptr;
    superblock->d_blocks_ptr = d_block_ptr;

    // write super block to beginning of disk img
    lseek(fd, 0, SEEK_SET);
    write(fd, (void *)superblock, sizeof(struct wfs_sb));

    // initialize root inode; CONSIDER: may need to zero the bitmaps, dont think so so I wont do it.
    struct wfs_inode * inode = (struct wfs_inode *) malloc(sizeof(struct wfs_inode));
    inode->num = 1; // NOTE ROOT DIRECTORY IS FIRST INODE
    inode->mode = S_IFDIR | 0755; // directory with rwx for all users. Piazza said it doesnt really matter what permissions we give it.
    inode->uid = 0;
    inode->gid = 0;
    inode->size = 0; // for . and .. dirs
    inode->nlinks = 0;
    inode->atim = inode->mtim = inode->ctim = time(NULL);
    for(int i = 0; i < N_BLOCKS; i++) {
        inode->blocks[i] = -1; // updated as a block at offset 0 is possible.
    }

    // write at correct spot
    lseek(fd, superblock->i_blocks_ptr, SEEK_SET);
    write(fd, inode, sizeof(struct wfs_inode));


    // set IBITMAP to 1 for the first spot for the root inode, skip 0 since this means empty
    lseek(fd, superblock->i_bitmap_ptr+1, SEEK_SET);
    unsigned char value = 1; // write one for the root inode.
    write(fd, &value, 1);

    close(fd);
    free(DISK_IMG_PATH);
    return 0;
}