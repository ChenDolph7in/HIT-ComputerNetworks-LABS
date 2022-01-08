// system support
#include "sysInclude.h"
#include <stdio.h>
#include <malloc.h>
extern void ip_DiscardPkt(char* pBuffer, int type);

extern void ip_SendtoLower(char* pBuffer, int length);

extern void ip_SendtoUp(char* pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students

int stud_ip_recv(char* pBuffer, unsigned short length)
{
    //读取字段内容，大于一个字节需要更改字节序
    char version = pBuffer[0] >> 4; //4bit版本号
    char IHL = pBuffer[0] & 0xf;   //4bit首部长度
    char TTL = pBuffer[8];    //8bit寿命
    int destination = ntohl(*(unsigned int*)(pBuffer + 16));    //32bit目的IP地址

    //检验正确性
    if (TTL <= 0) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }
    
    if (version != 4) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }
    
    if (IHL < 5) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }
    
    if (destination != getIpv4Address() && destination != 0xffff) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }

	//计算校验和
    unsigned long cksum = 0;
    for (int i = 0; i < IHL*4; i += 2) {//头部每2字节相加
        unsigned short sum = *((unsigned short*)(pBuffer + i));
        cksum += (unsigned long)sum;
    }
    cksum = (cksum >> 16) + (cksum & 0xffff);//将高16bit与低16bit相加
    cksum += cksum >> 16;//取进位再与低16bit相加
    unsigned short checksum = (unsigned short)(cksum);//取低16bit
    if (checksum != 0xffff) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }
    
    //转发给上级
    ip_SendtoUp(pBuffer, length);
    return 0;
}

int stud_ip_Upsend(char* pBuffer, unsigned short len, unsigned int srcAddr,
    unsigned int dstAddr, byte protocol, byte ttl)
{
    //根据数据和首部长度之和确定分配存储空间大小并申请
    char* buffer = (char*)malloc((20 + len) * sizeof(char));
    memset(buffer, 0, 20 + len);
    //根据上层传来的参数填写数据报字段，大于一个字节的需要改变字节序
    buffer[0] = 0x45;//版本号和首部长度
    buffer[8] = ttl;//寿命
    buffer[9] = protocol;//上层协议

    //数据报长度
    unsigned short sum_len = htons(len + 20);
    memcpy(buffer + 2, &sum_len, sizeof(unsigned short));

    //Ip地址
    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);
    memcpy(buffer + 12, &src, sizeof(unsigned int));
    memcpy(buffer + 16, &dst, sizeof(unsigned int));

    //计算校验和
    unsigned long cksum = 0;
    for (int i = 0; i < 20; i += 2) {//头部每2字节相加
        unsigned short sum = ntohs(*((unsigned short*)(buffer + i)));
        cksum += (unsigned long)sum;
    }
    //printf("1:%x\n",cksum);
    cksum = (cksum >> 16) + (cksum & 0xffff);//将高16bit与低16bit相加
    cksum += cksum >> 16;//取进位再与低16bit相加
    unsigned short checksum = (unsigned short)(cksum);//取低16bit
    checksum = ~checksum;
    checksum = htons(checksum);
    memcpy(buffer + 10, &checksum, 2);
    
    //数据字段
    memcpy(buffer + 20, pBuffer, len);
    
    ip_SendtoLower(buffer, len + 20);
    return 0;
}