#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

void Memset(unsigned char *buf,unsigned char c, int size)
{
	int i;
	for(i=0; i < size; i++) {
		buf[i]=c;
	}
	return;
}

#define SIZE 30

int main(int argc, char **argv)
{
	int pipe_desc[2];
	unsigned char read_buffer[100];
	unsigned char write_buffer[100];
	int i;
	int count;
	int err;

	Memset(read_buffer,0,sizeof(read_buffer));
	Memset(write_buffer,0,sizeof(write_buffer));

	/*
	 * create the pipe
	 */
	err = pipe(pipe_desc);
	if(err < 0) {
		printf("error creating pipe\n");
		exit(1);
	}

	for(i=0; i < SIZE; i++) {
		write_buffer[i] = i;
	}

	write_buffer[SIZE/2] = -1;

	write(pipe_desc[1],write_buffer,SIZE);
	count = read(pipe_desc[0],read_buffer,sizeof(read_buffer));
	if(count != SIZE) {
		printf("failed to read correct number of bytes\n");
		exit(1);
	}
	for(i=0; i < count; i++) {
		if(read_buffer[i] != write_buffer[i]) {
			printf("buffers fail to compare at %d\n",i);
			exit(1);
		}
	}
	printf("all bytes written and read correctly\n");
	close(pipe_desc[1]);
	close(pipe_desc[0]);
	exit(0);

}


