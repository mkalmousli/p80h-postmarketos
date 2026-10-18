/* devmem ADDR [VALUE]: read or write one 32-bit register through /dev/mem (bring-up helper, pushed to the tablet by `p80h devmem`). */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	unsigned long a, page = 4096;
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	volatile unsigned int *p;

	if (argc < 2 || fd < 0)
		return 1;
	a = strtoul(argv[1], 0, 0);
	p = mmap(0, page, PROT_READ | PROT_WRITE, MAP_SHARED, fd, a & ~(page - 1));
	if (p == MAP_FAILED)
		return 2;
	p += (a & (page - 1)) / 4;
	if (argc > 2)
		*p = strtoul(argv[2], 0, 0);
	printf("%08x\n", *p);
	return 0;
}
