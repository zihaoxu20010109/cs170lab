#include <stdio.h>

main()
{
	int prev_fd, new_fd, fd;

	prev_fd = 0;

	new_fd = 3;

	fd = dup2(0, 3);

	printf("DUP %d\n", fd);


	Syscall(0);
}
