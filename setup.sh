rmdir mnt
make
./create_disk.sh
./mkfs -d disk.img -i 100 -b 2000
mkdir mnt
./wfs disk.img -f -s ./mnt

