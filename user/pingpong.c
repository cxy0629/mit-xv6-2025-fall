#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int parent_to_child[2];
    int child_to_parent[2];
    char buf[1];
    int pid;
    pipe(parent_to_child);
    pipe(child_to_parent);
    if (fork() == 0)
    {
        pid = getpid();
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        read(parent_to_child[0], buf, 1);
        fprintf(1, "%d: received ping\n", pid);
        write(child_to_parent[1], buf, 1);
        exit(0);
    }
    else
    {
        char msg = 'P';
        pid = getpid();
        close(parent_to_child[0]);
        close(child_to_parent[1]);
        write(parent_to_child[1], &msg, 1);
        read(child_to_parent[0], buf, 1);
        fprintf(1, "%d: received pong\n", pid);
        exit(0);
    }
}