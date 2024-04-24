BINS = wfs mkfs mkfs_test
CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`
.PHONY: all
default: 
	$(CC) $(CFLAGS) wfs.c $(FUSE_CFLAGS) -o wfs
	$(CC) $(CFLAGS) -o mkfs mkfs.c
	$(CC) $(CFLAGS) -o mkfs_test test_mkfs.c

clean:
	rm -rf $(BINS)
