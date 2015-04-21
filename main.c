#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

//向服务器发送的请求
static const char* request="GET http://127.0.0.1/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\n";

int SetNonblocking(int fd)
{
	int old_option=fcntl(fd,F_GETFL);
	int new_option=old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

void AddFd(int epoll_fd,int fd)
{
	struct epoll_event event;
	event.data.fd=fd;
	event.events=EPOLLOUT | EPOLLET | EPOLLERR;
	epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
	SetNonblocking(fd);
}

//向服务器写入N字节数据
int WriteNbytes(int sockfd,const char* buffer,int len)
{
	int bytes_write=0;
	printf("write out %d bytes to socket %d\n",len,sockfd);
	while(1)
	{
		bytes_write=send(sockfd,buffer,len,0);
		if(bytes_write==-1)
		{
			return 1;
		}
		else if(bytes_write==0)
		{
			return 1;
		}
		len-=bytes_write;
		buffer=buffer+bytes_write;
		if(len<=0)
		{
			return 0;
		}
	}
}

//从服务器读取数据
int ReadOnce(int sockfd,char* buffer,int len)
{
	int bytes_read=0;
	memset(buffer,'\0',len);
	bytes_read=recv(sockfd,buffer,len,0);
	if(bytes_read==-1)
	{
		return 0;
	}
	else if(bytes_read==0)
	{
		return 0;
	}
	printf("read in %d bytes from socket %d with content: %s\n",bytes_read,sockfd,buffer);
	return 1;
}

//发起num个TCP连接
void StartConn(int epoll_fd,int num,const char* ip,int port)
{
	int i=0;
	int ret=0;
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family=AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port=htons(port);

	for(i=0;i<num;++i)
	{
		//sleep(1);
		int sockfd=socket(PF_INET,SOCK_STREAM,0);
		printf("create 1 sock\n");
		if(sockfd<0)
		{
			continue;
		}
		if(connect(sockfd,(struct sockaddr*)&address,sizeof(address))==0)
		{
			printf("build connection %d\n",i);
		}
	}
}

void CloseConn(int epollfd,int sockfd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,sockfd,0);
	close(sockfd);
}

int main()
{
	int i=0;
	int num=1000;
	char* ip="127.0.0.1";
	int port=12345;
	int epollfd=epoll_create(100);
	StartConn(epollfd,num,ip,port);
	struct epoll_event events[10000];
	char buffer[2048];
	while(1)
	{
		int fds=epoll_wait(epollfd,events,10000,2000);
		for(i=0;i<fds;++i)
		{
			int sockfd=events[i].data.fd;
			if(events[i].events  & EPOLLIN)
			{
				if(!ReadOnce(sockfd,buffer,2048))
				{
					CloseConn(epollfd,sockfd);
				}
				struct epoll_event event;
				event.events=EPOLLOUT | EPOLLET | EPOLLERR;
				event.data.fd=sockfd;
				epoll_ctl(epollfd,EPOLL_CTL_MOD,sockfd,&event);
			}
			else if(events[i].events & EPOLLOUT)
			{
				if(!WriteNbytes(sockfd,request,strlen(request)))
				{
					CloseConn(epollfd,sockfd);
				}
				struct epoll_event event;
				event.events=EPOLLIN | EPOLLET |EPOLLERR;
				event.data.fd=sockfd;
				epoll_ctl(epollfd,EPOLL_CTL_MOD,sockfd,&event);
			}
			else if(events[i].events & EPOLLERR)
			{
				CloseConn(epollfd,sockfd);
			}
		}
	}
}
