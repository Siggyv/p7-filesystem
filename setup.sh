rmdir mnt
make
./create_disk.sh
./mkfs -d disk.img -i 96 -b 200
mkdir mnt
./wfs disk.img -f -s ./mnt

