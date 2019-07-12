#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define adxl_x "/dev/adxl_x"
#define adxl_y "/dev/adxl_y"
#define adxl_z "/dev/adxl_z"

#define SHELLSCRIPT "\
#/bin/bash \n\
sudo chmod 777 /dev/adxl_* \n\
"

int main()
{
	int fd;  //fd- file desriptor
	char ch;
	int unsigned read_buf[1];
	printf("Running Shell Script to provide execute permission to device files....\n");
	puts(SHELLSCRIPT);	
	system(SHELLSCRIPT);
	printf("Select the axis for which data has to be read.\n");
	printf("Enter 'x' / 'y' / 'z' for that respective axis: ");
	scanf("%c",&ch);

	switch(ch){
	case 'x':
		fd=open(adxl_x, O_RDWR); //open file for reading output for x axis
                if(fd==-1){
                printf("file %s either does not exist or could not be accessed\n",adxl_x);
                exit(-1); }
                read(fd,read_buf,4);
                close(fd);
		printf("adxl_x: %u\n", *read_buf);
                break;
	case 'y':
                fd=open(adxl_y, O_RDWR); //open file for reading output for y axis
                if(fd==-1){
                printf("file %s either does not exist or could not be accessed\n",adxl_y);
                exit(-1); }
                read(fd,read_buf,4);
                close(fd);
		printf("adxl_y: %u\n", *read_buf);
                break;
	case 'z':
                fd=open(adxl_z, O_RDWR); //open file for reading output for z axis
                if(fd==-1){
                printf("file %s either does not exist or could not be accessed\n",adxl_z);
                exit(-1); }
                read(fd,read_buf,4);
                close(fd);
		printf("adxl_z: %u\n", *read_buf);
                break;	
	default:
		printf("\nCommand not recognised,make sure input is in lowercase : x or y or z\n");
		break;

	}

	return 0;
}
