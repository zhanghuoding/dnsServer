#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include "my_c_string_deal_tools.h"

#define BUFFER_SIZE 1024
#define LOCAL_IP_SIZE 64

char localIp[LOCAL_IP_SIZE] = {'\0'};//内网中的ip地址
char send_recvbuffer[BUFFER_SIZE];
//每隔一段时间复制更新resolv.conf文件的命令
//static char *com="rm -f /etc/resolv.conf ; cp /etc/resolv.conf.frequentlyUsed /etc/resolv.conf ; chmod 644 /etc/resolv.conf";
static char *com="cp /etc/resolv.conf.frequentlyUsed /etc/resolv.conf ; chmod 644 /etc/resolv.conf";
char send_recvbuffer_backup[BUFFER_SIZE]={'\0'};//保存上次的主机和ip信息。

char * conf="./conf/ipMap.conf";//客户端配置文件路径
char * logFilePath="./dns_ip_upload-log";//日志路径
char * logFileName="/ipSend.log";//日志文件名
char logFile[256]={'\0'};//日志文件的路径+文件名
pthread_t cp_thread;//负责间隔时间拷贝文件的线程句柄

int port=NULL;//服务器监听的端口
char serverIp[32]={'\0'};//在客户端，这个字段存储服务器的ip地址；在服务器端，该字段暂时空闲
char hostName[256]={'\0'};//在客户端，该字段存储客户机主机名；在服务器端，空闲
char dnsConfigFile_com[256]={'\0'};//该字段存储dns服务器配置文件路径
char dnsConfigFile_com_rev[256]={'\0'};//该字段存储dns服务器反向查询配置文件路径
int sleepTime=0;//客户端发送自己信息后睡眠时间，以秒为单位

void init();
long file_size(char*);
char *getLocalTime();
void outputLog(char *);
char *getLocalIp(char*);
void keepFile_resolvconf();

int main()
{
        //初始化
        init();

	if (pthread_create(&cp_thread, NULL, (void *)keepFile_resolvconf, NULL) != 0)
	{
		printf("pthread_create(&cp_thread, NULL, (void *)keepFile_resolvconf, NULL)");
		return EXIT_FAILURE;
	}

        while (1)
	{
            ///定义sockfd
            int sock_cli = socket(AF_INET,SOCK_STREAM, 0);
		bzero(send_recvbuffer,BUFFER_SIZE);

            ///定义sockaddr_in
		struct sockaddr_in *servaddr=NULL;
		while((servaddr=(struct sockaddr_in*)calloc(1,sizeof(struct sockaddr_in)))==NULL);
		servaddr->sin_family = AF_INET;
		servaddr->sin_port = htons(port);  ///服务器端口
		servaddr->sin_addr.s_addr = inet_addr(serverIp);  ///服务器ip

            ///连接服务器，成功返回0，错误返回-1
		while (connect(sock_cli, (struct sockaddr *)servaddr, sizeof(struct sockaddr_in)) < 0)
		{
			perror("connect");
		}
		sprintf(send_recvbuffer,"%s@",hostName);//将主机名写入发送缓存，主机名和ip之间用@分割
		getLocalIp(send_recvbuffer);
		if( strcmp(send_recvbuffer_backup,send_recvbuffer) )
		{//如果本次获得的信息和上次的不同，说明在很大程度上网络状况已经发生了改变，这时需要更新/etc/resolv.conf文件，以确保其正确性质。
			system(com);
			bzero(send_recvbuffer_backup,BUFFER_SIZE);
			strncpy(send_recvbuffer_backup,send_recvbuffer,strlen(send_recvbuffer));
		}
            send(sock_cli, send_recvbuffer, strlen(send_recvbuffer),0); ///发送本机主机名和ip给服务器
		bzero(send_recvbuffer,BUFFER_SIZE);//清空发送缓存准备接收服务器的返回信息
            int len = recv(sock_cli, send_recvbuffer, BUFFER_SIZE,0); ///接收
		replace(send_recvbuffer,"\n","\0");//收到的字符串末尾可能会有一个换行，去掉换行符，以免格式错乱
		replace(send_recvbuffer,"\r","\0");//收到的字符串末尾可能会有一个回车，去掉回车符，以免格式错乱
		len=strlen(send_recvbuffer);//重新计算接收到的字符串的长度
            char *p=send_recvbuffer+len;
            sprintf(p,"\t%s",getLocalTime());//客户端日志文件中有两个时间戳，第一个时服务器发送这条消息的时间，第二个是客户端收到这条消息的时间
            outputLog(send_recvbuffer);

		close(sock_cli);
		memset(send_recvbuffer, 0, sizeof(send_recvbuffer));
		free(servaddr);

		sleep(sleepTime);//客户机休眠sleepTime秒再发送
        }

	return 0;
}

void init()
{//主要是读取初始化文件
        //初始化日志文件路径
        char *lp=logFile;
        strncpy(logFile,logFilePath,strlen(logFilePath));
        lp+=strlen(logFilePath);
        strncpy(lp,logFileName,strlen(logFileName));

        ///读取配置文件./conf/ipMap.conf
        char confBuffer[BUFFER_SIZE]={'\0'};
        FILE * fconf=NULL;
        if((fconf=fopen(conf,"r"))==NULL)
         {
            perror("open config file");
            exit(1);
        }
        while(!feof(fconf))
        {
            memset(confBuffer,0,sizeof(confBuffer));
            fgets(confBuffer,sizeof(confBuffer)-1,fconf);
            char *flag=NULL;
            int cpLen=0;
            if(flag=strstr(confBuffer,"port="))
            {//从配置文件中提取端口号
                cpLen=strlen(confBuffer)-5-1;
                flag+=5;
                char portTemp[16]={'\0'};
                strncpy(portTemp,flag,cpLen);
                port=atoi(portTemp);
            }else if(flag=strstr(confBuffer,"sleepTime="))
            {//从配置文件中提取客户端休眠时常，以秒为单位
                cpLen=strlen(confBuffer)-10-1;
                flag+=10;
                char timeTemp[16]={'\0'};
                strncpy(timeTemp,flag,cpLen);
                sleepTime=atoi(timeTemp);
            }else if(flag=strstr(confBuffer,"serverIp="))
            {//从配置文件中提取服务器ip
                cpLen=strlen(confBuffer)-9-1;
                flag+=9;
                strncpy(serverIp,flag,cpLen);
            }else if(flag=strstr(confBuffer,"hostName="))
            {//从配置文件中提取当前主机名
                cpLen=strlen(confBuffer)-9-1;
                flag+=9;
                strncpy(hostName,flag,cpLen);
            }
        }
        fclose(fconf);
        if(port==0 || strlen(serverIp)==0 || strlen(hostName)==0 || sleepTime==0)
         {
            perror("get port, server ip or host name");
            exit(1);
        }

}

long file_size(char* filename)
{//获取文件大小
	struct stat statbuf;
	stat(filename,&statbuf);
	long size=statbuf.st_size;

	return size;
}

char *getLocalTime()
{
	time_t timep;
	time(&timep);
	return asctime(localtime(&timep));
}


void outputLog(char *buffer)
{
    //组织创建日志路径的语句
    char logPathBuffer[256]={'\0'};
    sprintf(logPathBuffer,"mkdir -p %s",logFilePath);

	FILE *log=NULL;
	if( (access( logFile, 0 )) == -1 )
	{//日志文件不存在则新建
        system(logPathBuffer);//创建日志文件路径
		creat(logFile,0755);
	}else
	{//日志文件没有读写权限时删除并新建
		if( (access( logFile, 6 )) == -1 )
		{
			remove(logFile);
            creat(logFile,0755);
        }
	}
	if(file_size(logFile)>10485760)
	{//日志文件存在且大于10M时，删除文件，重新建立
        remove(logFile);
		creat(logFile,0755);
	}
	if((log=fopen(logFile,"a+"))==NULL)
	{
		perror("open log file");
		exit(1);
	}

	fputs(buffer,log);

	fclose(log);
}

char *getLocalIp(char* send_recvbuffer)
{//本函数获取本机本地的（内网中的）ip，因为如果在路由器下，内外网获得的本机ip是不同的。
//本函数提取第四部分不等于1的ip地址，查找ip第四部分不等于"1"并且子网掩码不为空的ip，并返回该ip的字符串指针
//对于所有网卡的接口，只要子网掩码和ip都不为空，则将其上传至服务器
	struct ifaddrs *ifc, *ifc1;
	char *ptr=NULL;

	while(getifaddrs(&ifc))
	{//获取网络信息结构体链表
		perror("getifaddrs(&ifc)");
	}
	ifc1 = ifc;

	for(; NULL != ifc; ifc = (*ifc).ifa_next)
	{
		bzero(localIp,LOCAL_IP_SIZE);
        	if( NULL == (*ifc).ifa_netmask || NULL == (*ifc).ifa_addr )
			continue;//如果该节点子网掩码或者ip为空，则查看下个节点

		while( inet_ntop(AF_INET, &(((struct sockaddr_in*)((*ifc).ifa_addr))->sin_addr), localIp, LOCAL_IP_SIZE) == NULL )
		{
			perror("getLocalIp(char*) :inet_ntop(...) ");
		}
		//函数lastIndexOf(char* str1,char* str2)返回str1中最后一次出现str2下标

		ptr=localIp+lastIndexOf(localIp,".")+1;//获取ip地址第四部分的字符指针，例如：182.45.324.4，这里就获取"4"的指针
		if( atoi(ptr) == 0 )//将第四部分转换成int类型,并判断该值是否等于0,将末尾为0的ip过滤掉
			continue;

		ptr=send_recvbuffer+strlen(send_recvbuffer);//定位接下来要写入的下标
		sprintf(ptr,"%s;",localIp);//将ip记录下来，并以分号分隔
	}
	freeifaddrs(ifc1);
	return send_recvbuffer;
}
void keepFile_resolvconf()
{//该函数用于在一个独立的线程内部，每隔10分钟，将/etc/resolv.conf.frequentlyUsed文件拷贝成为/etc/resolv.conf,以保持resolv.conf文件的正确，从而正确访问DNS服务器
	//刚开机的时候，为了覆盖系统开机修改配置文件的习惯，每10秒修改一次文件，等到两分钟过后，系统修改期过了，再进如平稳的修改期。
	int i=18;
	while(i--)
	{
		system(com);
		sleep(10);
	}
	//接下来每10分钟修改一次文件，以保证文件是正确的
	while(1)
	{
		system(com);
		sleep(600);
	}
}
