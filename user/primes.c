#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#pragma GCC diagnostic ignored "-Winfinite-recursion"

#define MAXSIZE 35

void prime_sieve(int input_fd) __attribute__((noinline));

int main(int argc, char *argv[])
{
    int init_pipe[2];
    int i;
    pipe(init_pipe);
    for (i = 2; i <= MAXSIZE; i++)
        write(init_pipe[1], &i, sizeof(int));
    close(init_pipe[1]);
    prime_sieve(init_pipe[0]);
    exit(0);
}

void prime_sieve(int input_fd)
{
    int p[2];
    int prime, num;
    // int pid = getpid();
    if (read(input_fd, &prime, sizeof(int)) != sizeof(int))
        exit(0);
    printf("prime %d\n", prime);
    pipe(p);
    // printf("this is process %d, latest fd is %d\n", pid, p[1]);
    // printf("this is process %d, read from %d, write to %d\n", pid, input_fd, p[1]);
    if (fork() == 0)
    {
        close(input_fd);
        close(p[1]);
        prime_sieve(p[0]);
        exit(0);
    }
    else
    {
        close(p[0]);
        while (read(input_fd, &num, sizeof(int)) == sizeof(int))
        {
            if (num % prime != 0)
                write(p[1], &num, sizeof(int));
        }
        close(input_fd);
        close(p[1]);
        wait(0);
        // printf("this is %d, exit\n",pid);
        exit(0);
    }
}

// 我设想的每一个进程应当做的工作是：创建出下一个管道，从左侧管道读，向右侧管道写
// 但主进程初始时刻并没有管道信息可读，因此我给主进程的main函数内设计了一个管道
// 当主进程main函数调用prime_sieve时，主进程从main中的管道读取数据，并创建出新的管道，这与后续进程行为完全一致
// 如果只是fork的话这个递归将无休止地进行下去，因此我在prime_sieve一开始设计了一个检查
// 若管道内无数据且写端口被释放，则说明该质数筛已经工作完成
// 因此实际上总的筛进程数目是质数的个数加1（每个进程筛出一个质数，最后一个进程只做递归结束的检查）
// 每次筛进程向下一个管道写入的数据数目是不确定的，下一个筛进程也无法确定数据读取何时结束
// 为了完成这个机制，我设计了一个循环read，利用read阻塞的机制，并及时将写端口释放，让读者知晓何时结束read
// 一般的if/fork语句只能让父进程与子进程执行不同的任务，但本实验中子进程被创建后即将做与父进程相同的任务
// 这也是本代码递归的精髓之处，递归不仅表现了数据的流动，也表现了进程身份的流动，当子进程创建出自己的子进程时，它将变成父进程
// 子进程会从父进程fork时继承所有的文件描述符，如果我们不对文件描述符资源进行控制，那么系统中的总文件描述符数目将随着进程增加指数增长
// 因此，我在我的代码中对文件描述符进行了精准的close，确保不会产生文件描述符泄漏问题
// 可以运行注释的printf语句查看当前进程号，当前最大文件描述符和读写的文件描述符
// 可以发现文件描述符总量在每个进程运行至第一次调用prime_sieve创建新管道时维持为总量6（主进程是main调用prime_sieve，后续进程是递归调用）
// 除去标准输入、输出、错误外，还包括当前进程的读端口，创建的新管道的读、写端口，每个进程只工作在相邻两个管道的公共侧
// tips:虽然当前进程使用不到新管道的读端口，但为了子进程能顺利从父进程继承管道资源，此时还不可以close
// 由于释放管道，用于读的文件描述符在3、4之间循环，用于写的文件描述符固定为了5
// 为了保证进程的顺序（防止父进程提前终止，子进程变成孤儿进程），需要使用wait语句
// 当注释wait，取消注释exit前的输出语句时会观察到很神奇的现象
// 首先退出是乱序的，是根据实际进程调度顺序和进程推进进度决定的，其次将看到信息乱码显示，这与多进程并发有直接关系
// 这里涉及到更深层次的操作系统调度、时间片长度和锁竞争的问题，剩下的区域留给以后来探索吧！