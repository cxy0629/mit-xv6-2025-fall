#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int is_valid_number(const char *s);
int string_to_int(const char *s);

int main(int argc, char *argv[])
{
    int ticks;
    if (argc != 2)
    {
        fprintf(2, "usage: sleep <ticks>\n");
        exit(1);
    }
    if (!is_valid_number(argv[1]))
    {
        fprintf(2, "error: <ticks> must be a non-negative integer\n");
        exit(1);
    }
    ticks = string_to_int(argv[1]);
    pause(ticks);
    exit(0);
}

int is_valid_number(const char *s)
{
    if (*s == '\0')
        return 0;
    while (*s)
    {
        if (*s < '0' || *s > '9')
            return 0;
        s++;
    }
    return 1;
}

int string_to_int(const char *s)
{
    int res = 0;
    while (*s)
    {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res;
}