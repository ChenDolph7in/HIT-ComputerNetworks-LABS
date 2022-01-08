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
    //��ȡ�ֶ����ݣ�����һ���ֽ���Ҫ�����ֽ���
    char version = pBuffer[0] >> 4; //4bit�汾��
    char IHL = pBuffer[0] & 0xf;   //4bit�ײ�����
    char TTL = pBuffer[8];    //8bit����
    int destination = ntohl(*(unsigned int*)(pBuffer + 16));    //32bitĿ��IP��ַ

    //������ȷ��
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

	//����У���
    unsigned long cksum = 0;
    for (int i = 0; i < IHL*4; i += 2) {//ͷ��ÿ2�ֽ����
        unsigned short sum = *((unsigned short*)(pBuffer + i));
        cksum += (unsigned long)sum;
    }
    cksum = (cksum >> 16) + (cksum & 0xffff);//����16bit���16bit���
    cksum += cksum >> 16;//ȡ��λ�����16bit���
    unsigned short checksum = (unsigned short)(cksum);//ȡ��16bit
    if (checksum != 0xffff) {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }
    
    //ת�����ϼ�
    ip_SendtoUp(pBuffer, length);
    return 0;
}

int stud_ip_Upsend(char* pBuffer, unsigned short len, unsigned int srcAddr,
    unsigned int dstAddr, byte protocol, byte ttl)
{
    //�������ݺ��ײ�����֮��ȷ������洢�ռ��С������
    char* buffer = (char*)malloc((20 + len) * sizeof(char));
    memset(buffer, 0, 20 + len);
    //�����ϲ㴫���Ĳ�����д���ݱ��ֶΣ�����һ���ֽڵ���Ҫ�ı��ֽ���
    buffer[0] = 0x45;//�汾�ź��ײ�����
    buffer[8] = ttl;//����
    buffer[9] = protocol;//�ϲ�Э��

    //���ݱ�����
    unsigned short sum_len = htons(len + 20);
    memcpy(buffer + 2, &sum_len, sizeof(unsigned short));

    //Ip��ַ
    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);
    memcpy(buffer + 12, &src, sizeof(unsigned int));
    memcpy(buffer + 16, &dst, sizeof(unsigned int));

    //����У���
    unsigned long cksum = 0;
    for (int i = 0; i < 20; i += 2) {//ͷ��ÿ2�ֽ����
        unsigned short sum = ntohs(*((unsigned short*)(buffer + i)));
        cksum += (unsigned long)sum;
    }
    //printf("1:%x\n",cksum);
    cksum = (cksum >> 16) + (cksum & 0xffff);//����16bit���16bit���
    cksum += cksum >> 16;//ȡ��λ�����16bit���
    unsigned short checksum = (unsigned short)(cksum);//ȡ��16bit
    checksum = ~checksum;
    checksum = htons(checksum);
    memcpy(buffer + 10, &checksum, 2);
    
    //�����ֶ�
    memcpy(buffer + 20, pBuffer, len);
    
    ip_SendtoLower(buffer, len + 20);
    return 0;
}