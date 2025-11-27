#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    printf("%x", 'a');
    printf("Example 1:\n");
    int a[2] = {61810, 2025};
    memdump("ii", (char *)a);

    printf("Example 2:\n");
    memdump("S", "a string");

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s);

    struct sss
    {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example);

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example);
  }
  else if (argc == 2)
  {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data))
    {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  }
  else
  {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void memdump(char *fmt, char *data)
{
  while (*fmt)
  {
    switch (*fmt++)
    {
    case 'i':
      int i = *(int *)data;
      printf("%d\n", i);
      data += 4;
      break;
    case 'p':
      long long p = *(long long *)data;
      printf("%llx\n", p);
      data += 8;
      break;
    case 'h':
      short h = *(short *)data;
      printf("%d\n", h);
      data += 2;
      break;
    case 'c':
      char c = *(char *)data;
      printf("%c\n", c);
      data += 1;
      break;
    case 's':
      printf("%s\n", *(char **)data);
      data += 8;
      break;
    case 'S':
      printf("%s\n", data);
      break;
    default:
      break;
    }
  }
}

// s: the next 8 bytes of the data contain a 64-bit pointer to a C string; print the string.
// 这句话的是说字符串data开头的八个字节是一个指向C字符串的指针，所以实际上data是字符串指针的指针
// 但因为传入的data已经是字符指针了，无法通过*data获取data指向的8字节信息
// 应当*(char**)data
