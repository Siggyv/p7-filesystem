#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "wfs.h"

int main(int argc, char ** argv) {
    printf("MKFS TESTS:\n");

    if(argc != 2) {
        printf("USAGE: ./test_mkfs {disk image path using underscore as .}\n");
        exit(1);
    }

    char * disk_img_path = (char *)malloc(strlen(argv[1] + strlen("./")));
    if(disk_img_path == NULL) {
        printf("ERROR: failed to initialize disk image path.\n");
        exit(1);
    }

    strcpy(disk_img_path, "./");
    strcat(disk_img_path, argv[1]);
    for(int i = strlen(disk_img_path); i > 0; i--) {
        if(disk_img_path[i] == '_'){
            disk_img_path[i] = '.';
        }
    }

    printf("DISK IMAGE USED: %s\n", disk_img_path);
    
    int fd = open(disk_img_path, O_RDONLY); // open with read only
    if(fd < 0) {
        printf("ERROR: could not open disk image, validate path: %s\n", disk_img_path);
        free(disk_img_path);
        exit(1);
    }

    // test superblock init
    struct wfs_sb * superblock = (struct wfs_sb *) malloc(sizeof(struct wfs_sb));

    // read in superblock
    lseek(fd, 0, SEEK_SET);
    read(fd, superblock, sizeof(struct wfs_sb));

    // validate each part, num_inodes, num_data_blocks, and ptrs based off that
    int num_inodes = superblock->num_inodes;
    int num_blocks = superblock->num_data_blocks;

    // validate file can hold this
    struct stat statbuf;
    stat(disk_img_path, &statbuf);
    size_t file_size = statbuf.st_size;

    if(file_size < (num_blocks * BLOCK_SIZE)) {
        printf("ERROR: file is too small to hold the amount of blocks listed in superblock.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    if(num_blocks % 32 != 0) {
        printf("ERROR: num blocks was not rounded to nearest multiple of 32.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    // calculate and verify pointers
    off_t exp_i_bitmap = sizeof(struct wfs_sb);
    off_t exp_d_bitmap = exp_i_bitmap + num_inodes;
    off_t exp_i_block = exp_d_bitmap + num_blocks;
    off_t exp_d_block = exp_i_block + (sizeof(struct wfs_inode) * num_inodes);

    if(exp_i_bitmap != superblock->i_bitmap_ptr) {
        printf("ERROR: i bitmap values do not match up.\n");
        printf("EXP: %ld and GOT: %ld\n", exp_i_bitmap, superblock->i_bitmap_ptr);
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    if(exp_d_bitmap != superblock->d_bitmap_ptr) {
        printf("ERROR: d bitmap values do not match up.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }   

    if(exp_i_block != superblock->i_blocks_ptr) {
        printf("ERROR: i block pointer values do not match up.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    } 

    if(exp_d_block != superblock->d_blocks_ptr) {
        printf("ERROR: d block pointers do not match up.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    // verify inode bitmap has 1 for the root inode
    lseek(fd, superblock->i_bitmap_ptr, SEEK_SET);
    unsigned char inode_bitmap_value;
    read(fd, &inode_bitmap_value, 1);

    if(inode_bitmap_value != 1) {
        printf("ERROR: root inode is not marked in bitmap.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    // get root inode and validate its fields
    lseek(fd, superblock->i_blocks_ptr, SEEK_SET);
    struct wfs_inode * root = (struct wfs_inode *)malloc(sizeof(struct wfs_inode));
    read(fd, root, sizeof(struct wfs_inode));

    if(root->num != 1) {
        printf("ERROR: root node does not have correct number. Expected 1, got: %d\n", root->num);
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    if(!S_ISDIR(root->mode)) {
        printf("ERROR: root inode is not marked as a directory.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    if(root->nlinks != 0) {
        printf("ERROR: links value is nonzero upon initialization.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    if(root->size != 0) {
        printf("ERROR: size is nonzero upon initialization.\n");
        close(fd);
        free(disk_img_path);
        exit(1);
    }

    printf("ALL TESTS PASSED!\n");
    printf("NUM BLOCKS: %d and NUM INODES: %d\n", num_blocks, num_inodes);
    close(fd);
    return 0;
}