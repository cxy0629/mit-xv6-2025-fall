#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void sixfive(const char *filename);

int main(int argc, char *argv[])
{
    int i;
    if (argc == 1)
    {
        fprintf(2, "usage: sixfive <filepath>\n");
        exit(1);
    }
    for (i = 1; i < argc; i++)
        sixfive(argv[i]);
    exit(0);
}

void sixfive(const char *filename)
{
    int fd;
    char buf[512];
    char c;
    int i = 0, n;
    int is_finish = 0;
    int is_number = 1;
    memset(buf, 0, sizeof(buf));
    if ((fd = open(filename, O_RDONLY)) < 0)
    {
        fprintf(2, "error: cannot open %s\n", filename);
        return;
    }
    while (1)
    {
        if (read(fd, &c, 1) == 0)
            is_finish = 1;
        if (c == '-' || c == '\r' || c == '\t' || c == '\n' || c == '.' || c == '/' || c == ',' || is_finish == 1)
        {
            if (is_number && strlen(buf) != 0)
            {
                n = atoi(buf);
                if (n % 5 == 0 || n % 6 == 0)
                    printf("%d\n", n);
            }
            i = 0;
            memset(buf, 0, sizeof(buf));
            is_number = 1;
        }
        else if (c >= '0' && c <= '9')
        {
            buf[i++] = c;
        }
        else
            is_number = 0;
        if (is_finish)
            break;
    }
    close(fd);
    return;
}

// 注意：因为读取文件结尾不修改字符c的值，需要先进行数字结束符号的判断