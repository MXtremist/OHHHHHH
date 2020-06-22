#include "lib.h"
#include "types.h"

int uEntry(void)
{
	///*
	//main test
	int fd = 0;
	int i = 0;
	char tmp = 0;
	//test ls
	ls("/");
	ls("/boot/");
	ls("/usr/");
	printf("create /usr/test and write alphabets to it\n");
	//test open
	fd = open("/usr/test", O_READ | O_WRITE | O_CREATE);
	ls("/usr/");
	//test write, cat and close
	for (i = 0; i < 26; i++)
	{
		tmp = (char)(i + 'A');
		write(fd, (uint8_t *)&tmp, 1);
	}
	close(fd);
	cat("/usr/test");
	//test lseek
	fd = open("/usr/test", O_READ | O_WRITE);
	lseek(fd, 11, SEEK_SET);
	char C = '?';
	write(fd, (uint8_t *)&C, 1);
	close(fd);
	cat("/usr/test");
	//test remove
	remove("/usr/test");
	ls("/usr/");
	exit();
	return 0;
}
