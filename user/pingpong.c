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

// read:阻塞等待一次完整的写后才能读，但可以多次读，第三个参数指明读的最大数据量
// 当写端口绑定的文件描述符全部被释放或者达到最大读取数据量时read结束
// 读会导致数据对后续读者不可见，一般一个管道读端口只绑定一个文件描述符
// 管道写端口可以按任务需求绑定多个文件描述符
// 真实的读写顺序是根据代码编写的read、write阻塞逻辑+cpu真实调度顺序决定的
// 不需要的读写端口可以close，虽然不解绑在某些时刻不影响程序的运行
// 但为了防止文件描述符泄漏和科学编码的角度需要close！
// 管道是一种系统资源，它可以由用户进程创建，但它的存在不依赖于用户进程
// 用户进程只拥有文件描述符，但当所有进程close时，管道自然就释放了
// 同时用户进程结束时系统会自动close，会根据close规则决定释放管道
