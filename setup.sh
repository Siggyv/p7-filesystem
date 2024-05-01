rmdir mnt
make
./create_disk.sh
./mkfs -d disk.img -i 32 -b 64
mkdir mnt
./wfs disk.img -f -s ./mnt

