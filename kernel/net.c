#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

// UDP端口表
static struct udp_port udp_ports[MAX_UDP_PORTS];


void
netinit(void)
{
  initlock(&netlock, "netlock");
  // 初始化UDP端口表
  memset(udp_ports, 0, sizeof(udp_ports));
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  // 获取端口号参数
  int port;
  argint(0, &port);

  acquire(&netlock);

  // 检查端口号是否已绑定
  for(int i = 0; i < MAX_UDP_PORTS; i++){
    if(udp_ports[i].used == 1 && udp_ports[i].port == port){
      release(&netlock);
      return -1;
    }
  }

  // 查找未使用的端口表项
  for(int i = 0; i < MAX_UDP_PORTS; i++){
    // 找到未使用的端口表项，进行绑定
    if(udp_ports[i].used == 0){
      udp_ports[i].used = 1;
      udp_ports[i].port = port;
      memset(udp_ports[i].queue, 0, sizeof(udp_ports[i].queue));
      udp_ports[i].head = 0;
      udp_ports[i].tail = 0;
      udp_ports[i].count = 0;
      release(&netlock);
      return 0;
    }
  }

  // UDP端口表已满，无法绑定新端口
  release(&netlock);
  return -1;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  struct proc *p = myproc();

  // 获取参数
  int dport;
  uint64 src_ip_addr;
  uint64 src_port_addr;
  uint64 buf;
  int maxlen;
  argint(0, &dport);
  uint16 dst_port = (uint16)dport;
  argaddr(1, &src_ip_addr);
  argaddr(2, &src_port_addr);
  argaddr(3, &buf);
  argint(4, &maxlen);
  
  acquire(&netlock);

  // 查找绑定的端口表项
  struct udp_port *port_entry = 0;
  for(int i = 0; i < MAX_UDP_PORTS; i++){
    if(udp_ports[i].used == 1 && udp_ports[i].port == dst_port){
      port_entry = &udp_ports[i];
      break;
    }
  }

  // 若未找到绑定的端口表项，则返回错误
  if(port_entry == 0){
    release(&netlock);
    return -1;
  }

  // 若接收队列为空，则等待
  while(port_entry->count == 0)
    sleep(port_entry, &netlock);
  
  // 接收队列不为空
  // 获取源ip地址和端口号
  uint32 src_ip = port_entry->queue[port_entry->head].src_ip;
  uint16 src_port = port_entry->queue[port_entry->head].src_port;

  // 写回源ip地址和端口号到用户空间
  if((copyout(p->pagetable, src_ip_addr, (char *)&src_ip, sizeof(src_ip))) < 0){
    release(&netlock);
    return -1;
  }
  if((copyout(p->pagetable, src_port_addr, (char *)&src_port, sizeof(src_port))) < 0){
    release(&netlock);
    return -1;
  }

  // 有效载荷写回用户缓冲区
  char *payload = port_entry->queue[port_entry->head].payload;
  int payload_len = port_entry->queue[port_entry->head].len;
  if(payload_len > maxlen)
    payload_len = maxlen;
  if((copyout(p->pagetable, buf, payload, payload_len)) < 0){
    release(&netlock);
    return -1;
  }

  // 清理内核缓冲区
  kfree(payload);

  // 更新接收队列头指针和计数
  port_entry->head = (port_entry->head + 1) % UDP_QUEUE_SIZE;
  port_entry->count--;

  release(&netlock);

  return payload_len;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  // 拆解各级数据包头部和有效载荷
  struct eth *eth = (struct eth *)buf;
  struct ip *ip = (struct ip *)(eth + 1);
  struct udp *udp = (struct udp *)(ip + 1);
  char *payload = (char *)(udp + 1);

  // 获取数据包头部信息
  uint32 src_ip = ntohl(ip->ip_src);
  uint32 dst_ip = ntohl(ip->ip_dst);
  uint16 src_port = ntohs(udp->sport);
  uint16 dst_port = ntohs(udp->dport);

  // 检查目的ip地址、mac地址和udp协议是否匹配
  if(dst_ip != local_ip || memcmp(eth->dhost, local_mac, ETHADDR_LEN) != 0 || ip->ip_p != IPPROTO_UDP){
    kfree(buf);
    return;
  }

  // 计算有效载荷长度
  int payload_len = ntohs(udp->ulen) - sizeof(struct udp);

  acquire(&netlock);

  // 查找绑定的端口表项
  struct udp_port *port_entry = 0;
  for(int i = 0; i < MAX_UDP_PORTS; i++){
    if(udp_ports[i].used == 1 && udp_ports[i].port == dst_port){
      port_entry = &udp_ports[i];
      break;
    }
  }

  // 若未找到绑定的端口表项或接收队列已满，则丢弃数据包
  if(port_entry == 0 || port_entry->count == UDP_QUEUE_SIZE){
    kfree(buf);
    release(&netlock);
    return;
  }

  // 创建新的内核缓冲区，用于存放有效载荷，等待sys_recv接收
  char *new_payload = kalloc();
  if(new_payload == 0){
    kfree(buf);
    release(&netlock);
    return;
  }
  memmove(new_payload, payload, payload_len);

  // 释放原始缓冲区
  kfree(buf);

  // 将有效载荷放入接收队列
  port_entry->queue[port_entry->tail].src_ip = src_ip;
  port_entry->queue[port_entry->tail].src_port = src_port;
  port_entry->queue[port_entry->tail].payload = new_payload;
  port_entry->queue[port_entry->tail].len = payload_len;

  // 更新接收队列的尾指针和计数
  port_entry->tail = (port_entry->tail + 1) % UDP_QUEUE_SIZE;
  port_entry->count++;

  // 唤醒等待接收的进程
  wakeup(port_entry);

  release(&netlock);
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
