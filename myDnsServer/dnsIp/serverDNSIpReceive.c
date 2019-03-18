#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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


#define QUEUE 200
#define BUFFER_SIZE 1024
#define MAX_THREAD_NUM 200
#define HOSTNAME_SIZE 64
#define IP_SIZE 32
#define SHIELD_IP_NUM 100

char * logFilePath="./dns_ip_upload-log";//日志路径
char * logFileName="/ipReceive.log";//日志文件名
char logFile[256]={'\0'};//日志文件的路径+文件名
int thread_count=0;//线程计数器
pthread_t threads[MAX_THREAD_NUM];
int threads_head_index=0;
int threads_tail_index=0;
struct ipType{//该结构体保存ip和对应掩码，用来保存需要被屏蔽的ip和其掩码。掩码用来确定到底是屏蔽哪种类型对的ip
	char ipArray[IP_SIZE];
};
struct shieldIp{//该结构用来存储配置文件中写入的想要屏蔽的ip类型或者具体的某个ip
	struct ipType shi_array[SHIELD_IP_NUM];
	int index;//当前数组中存储的ip数
	int size;//结构体中数组的大小
	//shieldIp()
	//{//这里结构体初始化函数编译无法通过，于是将初始化放在了init函数中
	//	index=0;
	//	size=SHIELD_IP_NUM;
	//	bzero(shi_array,sizeof(struct ipType)*SHIELD_IP_NUM);
	//};
};
struct shieldIp shid_ip;//用来存储欲屏蔽ip的结构体变量


char *restartNamed_chroot_CentOS="systemctl restart named-chroot";//重启named-chroot域名服务器的命令，CentOS7以及以上版本
char *restartNamed_chroot_otherLinux="service named-chroot restart";//重启named-chroot域名服务器的命令，CentOS7以下或者其他linux版本

char * conf="./conf/ipMap.conf";//服务器端配置文件路径
int port=0;//服务器监听的端口
char serverIp[32]={'\0'};//在客户端，这个字段存储服务器的ip地址；在服务器端，该字段暂时空闲
char hostName[256]={'\0'};//在客户端，该字段存储客户机主机名；在服务器端，空闲
char dnsConfigFile_com[256]={'\0'};//该字段存储dns服务器配置文件路径
char dnsConfigFile_com_rev[256]={'\0'};//该字段存储dns服务器反向查询配置文件路径
char dnsConfigFile_com_chroot[256]={'\0'};//该字段存储dns服务器配置文件路径chroot
char dnsConfigFile_com_rev_chroot[256]={'\0'};//该字段存储dns服务器反向查询配置文件路径chroot
char domainNameSuffix[64]={'\0'};//客户机域名后缀

pthread_mutex_t arr_mutex;//写内存中的主机名ip地址对数组的互斥锁

struct pthread_para{
	int socket_ser;
	struct sockaddr_in client_addr;
};

struct hostName_Ip{//在内存中存储客户机主机名及其ip
	char hostName[HOSTNAME_SIZE];//客户主机名
	char hostIp[BUFFER_SIZE];//客户机ip串
};
struct hostName_Ip hosts[MAX_THREAD_NUM];//存储主机名及其ip信息的数组，数组大小和最大线程数相同
int hosts_index=0;//hosts数组的当前可存储位置的下标


void init();
long file_size(char*);
char *getLocalTime();
void child_thread(struct pthread_para*);
int search_host(struct hostName_Ip *);
int recod_host(struct hostName_Ip *);
void outputLog(char *);
int writeHostInfo(struct hostName_Ip*,int);
int isShield(char *);

int main()
{
	//初始化
	init();


	pthread_mutex_init(&arr_mutex,NULL);
	memset(hosts,0,sizeof(hosts));

	///定义sockfd
	int server_sockfd = socket(AF_INET,SOCK_STREAM, 0);

	///定义sockaddr_in
	struct sockaddr_in server_sockaddr;
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(port);
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	///bind，成功返回0，出错返回-1
	if(bind(server_sockfd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr))==-1)
	{
		char error_str[256]={'\0'};
		sprintf(error_str,"bind %d ",port);
		perror(error_str);
		exit(1);
	}

	///listen，成功返回0，出错返回-1
	while(listen(server_sockfd,QUEUE) == -1)
	{
		perror("listen");
	}

	// 开始处理客户端连接,初始化线程标识符数组
	bzero(&threads, sizeof(pthread_t) * MAX_THREAD_NUM);
	while(1)
	{
		///客户端套接字
		struct pthread_para *client;//client存放与客户机的连接句柄和客户机ip
        	client=(struct pthread_para*)calloc(1,sizeof(struct pthread_para));
		socklen_t length=sizeof(client->client_addr);

		///建立客户端链接，成功返回非负描述字，出错返回-1
		if((client->socket_ser = accept(server_sockfd, (struct sockaddr*)&(client->client_addr), &length))<0)
		{
			perror("connect error");
			continue;
		}

		if (pthread_create(threads + (thread_count++), NULL, (void *)child_thread, (void *)client) != 0)
		{
			printf("Create thread Failed!\n");
			return EXIT_FAILURE;
		}

	}
	close(server_sockfd);
	return 0;
}

void init()
{
//初始化存放欲屏蔽ip的结构体
	shid_ip.index=0;
	shid_ip.size=SHIELD_IP_NUM;
	bzero(shid_ip.shi_array,sizeof(struct ipType)*SHIELD_IP_NUM);

	char *flag=NULL;//辅助指针

	//初始化日志文件路径
	flag=logFile;
	strncpy(logFile,logFilePath,strlen(logFilePath));
	flag+=strlen(logFilePath);
	strncpy(flag,logFileName,strlen(logFileName));
	flag=NULL;

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
		if( ( flag=strstr(confBuffer,"port=") ) != NULL && strlen(confBuffer) > strlen("port=") )
		{//从配置文件中提取端口号
			cpLen=strlen(confBuffer)-5-1;
			flag+=5;
			char portTemp[32]={'\0'};
			strncpy(portTemp,flag,cpLen);
			port=atoi(portTemp);
			flag=NULL;
		}/*else if( ( flag=strstr(confBuffer,"serverIp=") ) != NULL && strlen(confBuffer) > strlen("serverIp=") )
        	{//从配置文件中提取服务器ip
            	cpLen=strlen(confBuffer)-9-1;
            	flag+=9;
            	strncpy(serverIp,flag,cpLen);
			flag=NULL;
		}*/else if( ( flag=strstr(confBuffer,"dnsConfigFile_com=") ) != NULL && strlen(confBuffer) > strlen("dnsConfigFile_com=") )
		{//从配置文件中提取DNS服务器配置文件路径
            	cpLen=strlen(confBuffer)-18-1;
            	flag+=18;
            	strncpy(dnsConfigFile_com,flag,cpLen);
			flag=NULL;
		}else if( ( flag=strstr(confBuffer,"dnsConfigFile_com_rev=") ) != NULL && strlen(confBuffer) > strlen("dnsConfigFile_com_rev=") )
		{//从配置文件中提取DNS服务器反向查询配置文件路径
            	cpLen=strlen(confBuffer)-22-1;
            	flag+=22;
            	strncpy(dnsConfigFile_com_rev,flag,cpLen);
			flag=NULL;
		}else if( ( flag=strstr(confBuffer,"dnsConfigFile_com_chroot=") ) != NULL && strlen(confBuffer) > strlen("dnsConfigFile_com_chroot=") )
        	{//从配置文件中提取DNS服务器反向查询配置文件路径chroot
			cpLen=strlen(confBuffer)-25-1;
            	flag+=25;
            	strncpy(dnsConfigFile_com_chroot,flag,cpLen);
			flag=NULL;
		}else if( ( flag=strstr(confBuffer,"dnsConfigFile_com_rev_chroot=") ) != NULL && strlen(confBuffer) > strlen("dnsConfigFile_com_rev_chroot=") )
        	{//从配置文件中提取DNS服务器反向查询配置文件路径chroot
			cpLen=strlen(confBuffer)-29-1;
			flag+=29;
			strncpy(dnsConfigFile_com_rev_chroot,flag,cpLen);
			flag=NULL;
		}else if( ( flag=strstr(confBuffer,"domainNameSuffix=") ) != NULL && strlen(confBuffer) > strlen("domainNameSuffix=") )
		{//从配置文件中提取客户机域名后缀
			cpLen=strlen(confBuffer)-17-1;
			flag+=17;
			strncpy(domainNameSuffix,flag,cpLen);
			flag=NULL;
		}else if( ( flag=strstr(confBuffer,"shieldIp=") ) != NULL && strlen(confBuffer) > strlen("shieldIp=") )
		{//从配置文件中提取欲屏蔽的ip
			cpLen=strlen(confBuffer)-9-1;
			flag+=9;
			if(shid_ip.index < shid_ip.size)
				strncpy(shid_ip.shi_array[shid_ip.index++].ipArray,flag,cpLen);
			flag=NULL;
		}
	}
	fclose(fconf);
	if(port==0 || strlen(domainNameSuffix)==0 || strlen(dnsConfigFile_com_rev)==0 || strlen(dnsConfigFile_com)==0 || strlen(dnsConfigFile_com_chroot)==0 || strlen(dnsConfigFile_com_rev_chroot)==0 )
	 {
        perror("get port, domain name suffix,dnsConfigFile_com_rev,dnsConfigFile_com_rev,dnsConfigFile_com_rev_chroot or dnsConfigFile_com_chroot error");
        exit(1);
	}
	//恢复备份的nds服务器配置文件,每次启动都恢复dns服务器配置文件，以免文件过长和重复
	char commandStr[256]={'\0'};
	sprintf(commandStr,"rm -f %s",dnsConfigFile_com);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"cp %s.backup %s",dnsConfigFile_com,dnsConfigFile_com);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"chgrp named %s",dnsConfigFile_com);
	system(commandStr);

	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"rm -f %s",dnsConfigFile_com_rev);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"cp %s.backup %s",dnsConfigFile_com_rev,dnsConfigFile_com_rev);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"chgrp named %s",dnsConfigFile_com_rev);
	system(commandStr);


	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"rm -f %s",dnsConfigFile_com_chroot);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"cp %s.backup %s",dnsConfigFile_com_chroot,dnsConfigFile_com_chroot);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"chgrp named %s",dnsConfigFile_com_chroot);
	system(commandStr);

	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"rm -f %s",dnsConfigFile_com_rev_chroot);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"cp %s.backup %s",dnsConfigFile_com_rev_chroot,dnsConfigFile_com_rev_chroot);
	system(commandStr);
	bzero(commandStr,sizeof(commandStr));
	sprintf(commandStr,"chgrp named %s",dnsConfigFile_com_rev_chroot);
	system(commandStr);
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

void child_thread(struct pthread_para *para)
{
	struct pthread_para *client=para;
	int sockfd=client->socket_ser;
	char recv_buffer[BUFFER_SIZE];
	char send_buffer[BUFFER_SIZE];
	bzero(recv_buffer, BUFFER_SIZE);
	bzero(send_buffer, BUFFER_SIZE);
	char tmpIp[64]={'\0'};
	char clientIp[64]={'\0'};
	char clientHostname[128]={'\0'};

	int len = recv(sockfd, recv_buffer, BUFFER_SIZE, 0);
	replace(recv_buffer,"\n","\0");//收到的字符串末尾可能会有一个换行，去掉换行符
	replace(recv_buffer,"\r","\0");//收到的字符串末尾可能会有一个回车，去掉回车符
	len=strlen(recv_buffer);//重新计算接收到的字符串的长度
	if(!len)
		goto tail;//如果收到的信息长度为0，则不需要进行后面的操作

	//获取客户机ip地址等信息
	while( inet_ntop(AF_INET,&client->client_addr.sin_addr,tmpIp,64) == NULL )
	{
		perror("child_thread(struct pthread_para *) : inet_ntop ");
	}

	//将客户机主机名ip对记录到hosts数组中
	struct hostName_Ip tempHost;//记录该线程连接的主机名ip对
	bzero(&tempHost, sizeof(struct hostName_Ip));
	strncpy(tempHost.hostName,recv_buffer,indexOf(recv_buffer,"@") );//把刚刚收到的主机名保存到临时的host信息变量中
	strncpy(tempHost.hostIp, tmpIp, strlen(tmpIp));//先将公网ip加入到ip字符串中
	char *pptr=tempHost.hostIp+strlen(tmpIp);//临时辅助指针
	*pptr++=';';//公网ip和内网ip之间像其他所有ip之间一样，用";"分隔。
	*pptr='\0';//字符串结尾加上这个
	strncpy(pptr, recv_buffer+indexOf(recv_buffer,"@")+1, strlen(recv_buffer)-indexOf(recv_buffer,"@")-1);//把刚刚收到的内网ip串保存到临时的host信息变量中
	pptr=NULL;//该变量已经没用了，以后不再使用
	bzero(recv_buffer, BUFFER_SIZE);//清除变量，用于后面重写
	sprintf(recv_buffer,"%s@%s",tempHost.hostName,tempHost.hostIp);//将客户机主机名和所有的ip用@连接起来，以备后面返回给客户机，也用于记录在DNS文件。

	//写日志文件
	sprintf(send_buffer,"threads num : %d\t%s\t%s",thread_count,recv_buffer,getLocalTime());
	outputLog(send_buffer);

	//向客户机返回信息
	bzero(recv_buffer, BUFFER_SIZE);
	sprintf(recv_buffer,"OK\t%s",send_buffer);
	send(sockfd, recv_buffer, strlen(recv_buffer), 0);

	//记录当前客户机主机名ip对
	recod_host(&tempHost);

	tail:

	close(sockfd);
	free(client);
	thread_count--;
}

int search_host(struct hostName_Ip* host)
{//查找该主机名书否已经存在于当前的缓存信息中，如果客户机信息没有改变，则返回-2，如果缓存数组大小为0，则返回-3,如果客户机信息没有找到，则返回-1
	int i=0;
	if(hosts_index<=0)
		return -3;
	for(i=0;i<hosts_index;i++)
	{
		if(strcmp(hosts[i].hostName,host->hostName)==0 )
			if( strcmp(hosts[i].hostIp,host->hostIp)==0 )
				return -2;
			else
				return i;
	}
	return -1;
}

int recod_host(struct hostName_Ip *host)
{
	if(hosts_index>=MAX_THREAD_NUM)
	{
		outputLog("warning : The buffer array of hostName was full!");
		return 0;
	}
	int temp=search_host(host);
	if(temp==-2)
		return 1;//客户机信息没有改变，什么都不用做
	if( temp==-1 || temp==-3 )
	{//当前缓存数组为空或者没有找到客户机信息，则记录该客户机信息到缓存中

		pthread_mutex_lock(&arr_mutex);//写内存中的主机名ip地址对数组的互斥锁
		memcpy(&hosts[hosts_index++],host,sizeof(struct hostName_Ip));
		writeHostInfo(host,0);//将该客户机信息写入dns服务器配置文件
		pthread_mutex_unlock(&arr_mutex);//写内存中的主机名ip地址对数组的互斥锁
	}
	if(temp>=0 && temp<MAX_THREAD_NUM)
	{
		pthread_mutex_lock(&arr_mutex);
		memcpy(&hosts[temp],host,sizeof(struct hostName_Ip));//将客户机信息写入文件内存
		writeHostInfo(host,1);//将客户机主机名和ip写入dnf服务器配置文件
		pthread_mutex_unlock(&arr_mutex);
	}
}
int writeHostInfo(struct hostName_Ip* host,int flag)
{//flag为0时表示该主机第一次连接服务器
//flag为1时，表示该主机ip已发生变动
	char wbuffer[BUFFER_SIZE]={'\0'};//要写入配置文件的字符串	bzero(wbuffer, BUFFER_SIZE);
	char *ptr1=NULL;//辅助指针
	char *ptr2=NULL;//辅助指针
	if(flag)
	{//该主机ip已发生变动www     IN  A   192.168.10.10
	//该主机信息发生了改变的话，则需要首先删除该主机在服务器配置文件中的所有记录
	//sed -i '/ddd.*$/d' ./sss
	//sed -i 's#enabled=.*$#enabled=1#g' /etc/yum.repos.d/nux-dextop.repo
		sprintf(wbuffer,"sed -i \'/%s.*$/d\' %s",host->hostName,dnsConfigFile_com);
		system(wbuffer);
		bzero(wbuffer, BUFFER_SIZE);
		sprintf(wbuffer,"sed -i \'/%s.*$/d\' %s",host->hostName,dnsConfigFile_com_chroot);
		system(wbuffer);
		bzero(wbuffer, BUFFER_SIZE);
	}

	ptr1=ptr2=NULL;
	ptr1=host->hostIp;//获取ip串的指针
	while(strlen(ptr1)>=8)//ip地址最小最小也有8位，如:2.2.2.2;每个ip都是以";"结尾的
	{
	//这里对于以182或10开头的ip都予以屏蔽，不写入DNS服务器配置文件中
	//
		ptr2=ptr1+indexOf(ptr1,";");
		*(ptr2)='\0';//将目前的第一个";"替换成空字符'\0'以便于下面处理
		if( !isShield(ptr1) )
		{//如果该ip不需要被屏蔽，则写入该ip到DNS服务器配置文件中
			sprintf(wbuffer,"echo  \'%s     IN  A   %s\' >> %s",host->hostName,ptr1,dnsConfigFile_com);
			system(wbuffer);
			bzero(wbuffer, BUFFER_SIZE);
			sprintf(wbuffer,"echo  \'%s     IN  A   %s\' >> %s",host->hostName,ptr1,dnsConfigFile_com_chroot);
			system(wbuffer);
			bzero(wbuffer, BUFFER_SIZE);
		}
		ptr1=ptr2+1;//准备处理下一个ip
		ptr2=NULL;
	}
	system(restartNamed_chroot_otherLinux);//每次更新文件后都重启服务器，在其他发行版上执行
	system(restartNamed_chroot_CentOS);//每次更新文件后都重启服务器,在CentOS7以及以上版本系统上执行
	return 0;
}

int isShield(char *currentIp)
{//该函数用于判断传入的ip字符串是否需要被屏蔽，
//如果要被屏蔽，则返回1，否则返回0
	struct in_addr tmp1,tmp2,tmp3,tmp4,tmp5;
	char ipp[16];//存储每次循环中欲屏蔽的ip
	char mask[16];//存放每次循环中欲屏蔽ip的掩码
//	struct in_addr mask_addr;//存放转换成网络字节序二进制的掩码
	bzero(&tmp1,sizeof(struct in_addr));
	bzero(&tmp2,sizeof(struct in_addr));
	bzero(&tmp3,sizeof(struct in_addr));
	bzero(&tmp4,sizeof(struct in_addr));
	bzero(&tmp5,sizeof(struct in_addr));
	//将传入的ip字符串转换成网络字节序二进制
	if( inet_pton(AF_INET,currentIp,&tmp1) <= 0 )
		return 0;//如果转换出错,转换函数返回非正值，则忽略这个ip，返回0，将其写入dns服务器缓存

	int i=0;
	for(i=0;i<shid_ip.index;i++)
	{
		bzero(ipp,sizeof(ipp));
		bzero(mask,sizeof(mask));

		//下面将欲屏蔽ip和其掩码分离出来
		strncpy(ipp, shid_ip.shi_array[i].ipArray, indexOf(shid_ip.shi_array[i].ipArray, "/"));//ip和其掩码之间用“/”分隔
		strncpy(mask, shid_ip.shi_array[i].ipArray+indexOf(shid_ip.shi_array[i].ipArray, "/")+1, strlen( shid_ip.shi_array[i].ipArray) - indexOf(shid_ip.shi_array[i].ipArray, "/") - 1 );//将网掩码提取出来
		if( inet_pton(AF_INET,ipp, &tmp2 ) <= 0 || inet_pton(AF_INET,mask, &tmp3 ) <= 0 )
			continue;//如果转换出错,则放弃该ip，测试下一个ip

		//接下来将两个ip分别和掩码按位与，并将结果存在tmp4和tmp5中
		tmp4.s_addr = tmp1.s_addr & tmp3.s_addr;
		tmp5.s_addr = tmp2.s_addr & tmp3.s_addr;
		//比较两个ip和该掩码按位与之后的结果，如果相同，则返回1，否则返回0
		if( !memcmp( &tmp4, &tmp5, sizeof(struct in_addr) ) )
		{
			char outp[256]={'\0'};
			sprintf(outp, "\033[40;31m该信息为调试而输出，并不是错误。\033[0m主机ip：%s 匹配到了屏蔽字段：%s ，将被屏蔽而不写入到DNS服务器配置文件中！", currentIp, shid_ip.shi_array[i].ipArray );
			perror(outp);//为了调试而输出提示信息
			return 1;
		}
	}

	return 0;
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
	{//日志文件存在且大于10M时，删除文件，然后重新创建该文件
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
