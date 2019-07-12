DEVICE ON FILE:
Implemented and tested on kernel version: 4.10.0-29-generic

Steps:

1. Create userspace file: /etc/sample.txt

2. run make command: $ make

3. Insert module: $ sudo insmod main.ko

4. To View Partitions: $ sudo fdisk -l /dev/dof

   In the logs, it can be verified that MBR is read from the sector 0.

5. Write data into userspace file:

   (**Important): Change to superUser mode: $ sudo su	

   Create text file: "test.txt" with some test string written into it.
   Run: # dd if=test.txt of=/dev/dof
   View same string : # vim /etc/sample.txt

6. Write using echo
	# echo "sample_string" > /dev/dof
	View same string : # vim /etc/sample.txt

7. Write and read using echo and cat

	# echo "sample_string" > /dev/dof1
	# cat /dev/dof1

