BINS = wfs mkfs mkfs_test
CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`
.PHONY: all
all: $(BINS)
wfs:
	$(CC) $(CFLAGS) wfs.c $(FUSE_CFLAGS) -o wfs
mkfs:
	$(CC) $(CFLAGS) -o mkfs mkfs.c
mkfs_test:
	$(CC) $(CFLAGS) -o mkfs_test test_mkfs.c
.PHONY: clean
clean:
	rm -rf $(BINS)
