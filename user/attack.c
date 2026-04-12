#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int main(int argc, char *argv[])
{
  // Your code here.
  int size = 1024 * 1024;
  int offset = 16;
  char hint[] = "This may help.";
  while (1)
  {
    char *buf = sbrk(size);
    for (int i = 0; i < size - offset; i++)
    {
      if (strcmp(buf + i, hint) == 0)
      {
        printf("%s\n", buf + i + offset);
        exit(0);
      }
    }
  }
  exit(1);
}
