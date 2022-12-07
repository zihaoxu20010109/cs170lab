#include <stdio.h>
char *words[] = {"dog", "rat", "bat", "ant", "cow", "pig", "fox", "hen", "bee", "emu", "eel", "owl"};

#define Syscall(x) exit(x)

main()
{
	int i, rc, pipe1[2], pipe2[2], pipe3[2], *rd_fd, *wr_fd;
	char rd_buf1[256], rd_buf2[256], rd_buf3[256], rd_buf4[256];

	rc = pipe(pipe1);
	if (rc != 0) {
		perror("pipe() failed");
		Syscall(0);
	}
	printf("created pipe1: %d, %d\n", pipe1[0], pipe1[1]);

	rc = pipe(pipe2);
	if (rc != 0) {
		perror("pipe() failed");
		Syscall(0);
	}
	printf("created pipe2: %d, %d\n", pipe2[0], pipe2[1]);

	rc = pipe(pipe3);
	if (rc != 0) {
		perror("pipe() failed");
		Syscall(0);
	}
	printf("created pipe3: %d, %d\n", pipe3[0], pipe3[1]);

	write(pipe1[1], words[0], 4);
	write(pipe2[1], words[1], 4);
	read(pipe2[0], rd_buf2, 4);
	printf("rd_buf2: %s\n",rd_buf2);
	read(pipe1[0], rd_buf1, 4);
	printf("rd_buf1: %s\n",rd_buf1);
	if (strcmp(words[0], rd_buf1) || !strcmp(rd_buf1, rd_buf2) || strcmp(words[1], rd_buf2)) {
		printf("pipes crossed!\n");
		Syscall(0);
	}

	rc = dup2(pipe2[1], pipe1[1]);
	printf("dup2ed %d into %d, returned %d\n", pipe2[1], pipe1[1], rc);

	rc = dup2(pipe2[0], pipe1[0]);
	printf("dup2ed %d into %d, returned %d\n", pipe2[0], pipe1[0], rc);
	write(pipe1[1], words[2], 4);
	write(pipe2[1], words[3], 4);
	write(pipe1[1], words[4], 4);
	write(pipe2[1], words[5], 4);
	printf("Put '%s', '%s', '%s', '%s' into pipe...\n", words[2],
			words[3], words[4], words[5]);
	read(pipe1[0], rd_buf1, 4);
	printf("rd_buf1: %s", rd_buf1);
	read(pipe1[0], rd_buf2, 4);
	read(pipe2[0], rd_buf3, 4);
	read(pipe2[0], rd_buf4, 4);
	printf("Got '%s', '%s', '%s', '%s' from pipe...\n", rd_buf1, rd_buf2, rd_buf3, rd_buf4);

	rc = dup2(pipe2[1], pipe3[1]);
	printf("dup2ed %d into %d, returned %d\n", pipe2[1], pipe3[1], rc);

	/* if dup2 closed pipe1[1], this won't block */
	/* r c should return 0 */
	printf("reading from %d... ", pipe3[0]);
	rc = read(pipe3[0], rd_buf1, sizeof(words[1]));
	printf("returned %d\n", rc);

	close(pipe1[0]);
	close(pipe1[1]);
	close(pipe2[0]);
	close(pipe2[1]);
	close(pipe3[0]);
	close(pipe3[1]);

//	for(i=0; i < 10000; i++);

	Syscall(0);
}
