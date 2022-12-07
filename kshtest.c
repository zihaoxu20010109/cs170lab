/*
 * exec.c -- exec's the argtest program
 */

#include <stdio.h>
#include <stdlib.h>

main()
{
  char *path = "argtest";
  char *argv[10] = { "argtest", "Once", "I", "Had", "a", "Girl", "on", 
                 "Rocky", "Top", NULL};

  printf("printf 0x%x, 0x%x %s\n", argv, argv[0], argv[0]);
  execve(path, argv);

  exit(1);

  perror("exec");
}