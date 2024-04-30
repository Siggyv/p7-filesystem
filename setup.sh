# rmdir mnt
make
<<<<<<< HEAD
# ./create_disk.sh
# ./mkfs -d disk.img -i 100 -b 2000
# mkdir mnt
=======
./create_disk.sh
./mkfs -d disk.img -i 100 -b 2000
mkdir mnt
>>>>>>> 4a40af25a8fc911bb51cd07548ef70add1a70f1d
./wfs prebuilt_disk -f -s ./mnt

