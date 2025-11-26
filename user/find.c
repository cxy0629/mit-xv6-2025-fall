#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"

void find(int dir_fd, const char *cur_dir_path, const char *target_filename);

int main(int argc, char *argv[])
{
    int dir_fd;
    char *dir_path;
    char *filename;
    if (argc != 3)
    {
        fprintf(2, "usage: find <directory> <filename>\n");
        exit(1);
    }
    dir_path = argv[1];
    filename = argv[2];
    if ((dir_fd = open(dir_path, O_RDONLY)) < 0) 
    {
        fprintf(2, "error: cannot open %s\n", dir_path);
        exit(1);
    }
    find(dir_fd, dir_path, filename);
    exit(0);
}

void find(int dir_fd, const char *dir_path, const char *target_filename)
{
    char file_path[512];
    char *p, *q;
    struct stat st;
    struct dirent de;
    if(strlen(dir_path) + 1 + DIRSIZ + 1 > sizeof(file_path))
    {
        fprintf(2, "error: path too long: %s\n",dir_path);
        close(dir_fd);
        return;
    }
    strcpy(file_path, dir_path);
    p = file_path + strlen(file_path);
    *p = '/';
    p++;
    while (read(dir_fd, &de, sizeof(struct dirent)) == sizeof(struct dirent))
    {
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;
        if(de.inum == 0)
            continue;
        q = p;
        memmove(q, de.name, DIRSIZ);
        q[DIRSIZ] = 0;
        int fd = open(file_path, O_RDONLY);
        fstat(fd, &st);
        switch (st.type)
        {
        case T_DIR:
            find(fd, file_path, target_filename);
            break;
        case T_FILE:
            if(strcmp(target_filename, q) == 0)
                printf("%s\n",file_path);
            close(fd);
            break;
        case T_DEVICE:
            close(fd);
            break;
        }
    }
    close(dir_fd);
}

// fs.h定义了文件系统的持久化数据结构，dirent是目录项，dinode是外存索引节点
// file.h定义了运行时内存文件数据结构，file是内存文件对象，inode是内存索引节点
// 注意在完成文件资源调入后，文件访问流程应该为文件描述符->file->inode
// xv6并未实现在file内指向目录项，这是因为目录项只在查找阶段使用
// 一旦文件打开后，我们只需要文件描述符、索引节点和内存文件对象
// 索引节点分为外存索引节点和内存索引节点，调入后使用的是内存索引节点
// 一方面内存访问可以加速，另一方面内存索引节点包含了一些额外的运行时信息
// xv6甚至提供了额外的缓存机制来帮助索引节点的调入
// stat.h定义了实体文件信息数据结构stat，它记录的是“实体文件”“部分”运行时和“部分”持久化信息的快照
// 它只针对实体文件，包括文件、目录和设备，不支持临时文件，如管道，而file作为文件对象支持所有文件类型
// 它不像目录项和索引节点存储在外存记录元数据，它是按用户需求生成提供的“统一”文件实体信息
// 若同一个文件被多次open（未做文件修改），会有多个文件对象，且文件对象内记录信息可能不同
// 若多次生成stat（未做文件修改），获取的信息是完全一致的
// 本质上来说文件对象针对单个打开实例，stat针对实体文件本身，stat存在的意义是为用户层代码提供标准化、安全、易用的文件信息接口

// 代码借鉴了多处ls.c的优秀设计，在复制目录路径至缓冲区前检查了缓冲区溢出问题
// 在复制目录路径和添加文件名称时分别使用了strcpy和memcpy，对不同安全情况下使用了不同的控制
// 本质原因是文件名可能写满字符串导致结尾处0丢失，0丢失会导致strcmp失败，因此在文件路径最后强制补了一个0
// 使用递归进行深度搜索解决目录嵌套问题，让每个函数控制当前目录对应文件描述符的关闭，被调用函数的目录对应文件描述符由调用函数创建
// 及时释放文件类型和设备类型描述符，保证文件描述符不溢出

// 一些更深层次的发现：每个目录内记录的目录项都含有.,..，实际上是两个硬链接，指向当前目录和上一级目录
// 作为根目录，.和..指向的inode都是1，毕竟根目录没有上一级目录了
// 文件系统采用一种懒惰的机制管理目录项的变化，被删除的文件对应的目录项会简单的将inum置为0
// 即索引节点编号从1开始，0作为一个不会使用的编号在目录项中承担了无效的含义，根目录的inum也确实是1
