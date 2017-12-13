#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdarg.h>
#include "osa_serial.h"

int Serial_Open(char *dev_name)
{
	int fd;
    struct termios  oldtio, newtio;

    fd = open(dev_name, O_RDWR|O_NOCTTY);

    if (fd < 0) {
        perror(dev_name);
        return -1;
    }
	
    tcgetattr(fd, &oldtio);
    bzero(&newtio, sizeof(newtio));
	
	newtio = oldtio;

	cfsetispeed(&newtio, SDS_BAUDRATE);
	cfsetospeed(&newtio, SDS_BAUDRATE);
	cfmakeraw(&newtio);
	
    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);

	return fd;

}

int Serial_Close(int fd)
{
	close(fd);

	return 0;
}

int Serial_Read(int fd, void *pData, int size)
{
	system("echo 0 > /sys/class/gpio/gpio33/value");
	
	return read(fd, pData, size);
}

int Serial_Write(int fd, void *pData, int size)
{
	system("echo 1 > /sys/class/gpio/gpio33/value");
	
	return write(fd, pData, size);
}

int fd_printf(int fd, char * format, ...)
{
  char buffer[256];
  int size, ret;
  va_list args;
  va_start (args, format);
  size = vsprintf (buffer,format, args);
  if(size > 0){
	ret = write (fd, buffer, size);
  }
  else
	ret = -1;
  va_end (args);
  tcdrain( fd );
  return ret;
}
