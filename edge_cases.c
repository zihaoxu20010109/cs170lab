int main(int argc, char **argv) {
    int pipe_desc[2];
	int err;
	char *string;
	char buff[4096];

    err = pipe(pipe_desc);
    if (err < 0) {
        printf("error creating pipe\n");
        exit(1);
    } else {
        printf("pipe_desc[0]: %d\npipe_desc[1]: %d\n", pipe_desc[0], pipe_desc[1]);
    }

    switch(fork()) {
        case -1:
            exit(-1);
        case 0:
            close(pipe_desc[1]);
            bzero(buff, 0, sizeof(buff));
            read(pipe_desc[0], buff, sizeof(buff));
            printf("Child read %s\n", buff);
            // while (read(pipe_desc[0], buff, sizeof(buff)) > 0) {
            //     printf("Child read %s\n", buff);
            //     bzero(buff, 0, sizeof(buff));
            // }
            bzero(buff, 0, sizeof(buff));
            printf("Child is exiting !\n");
            return 0;
        default:
            close(pipe_desc[0]);
            string = "areallylongstring";
            err = write(pipe_desc[1], string, strlen(string));
            // printf("error: %d\n", err);
            if (err < 0) {
                printf("error: %d\n", err);
            }
            // write(pipe_desc[1], "world", strlen("world"));
            wait();
            printf("Parent is exiting!\n");
            return 0;
    }
    return(0);
}