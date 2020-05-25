#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include "fpm.h"
#include <linux/rtnetlink.h>
#include <pthread.h>

#define MAXLINE 4096

void decode(void *buffer,int len){
    struct nlmsghdr *nlh;
    struct rtmsg *r;
    char    destination_address[32];
    unsigned char    route_netmask = 0;
    unsigned char    route_protocol = 0;
    char    gateway_address[32];
    struct  rtattr *route_attribute; /* This struct contain route \
                                            attributes (route type) */
    int     route_attribute_len = 0;


    nlh = (struct nlmsghdr *)buffer;
    r = (struct rtmsg*)(buffer+sizeof(struct nlmsghdr));
    if (nlh->nlmsg_type == NLMSG_DONE)
    {
        printf("NLMSG_DONE!\n");
    }
    printf("len:%d \n",len);
    if(NLMSG_OK(nlh,len))
        printf("NLMSG_OK. \n");
    else
        printf("NLMSG_OK error. \n");

    printf("sizeof(struct nlmsghdr):%d nlmsg_len:%d nlmsg_type:%d nlmsg_flags:%d nlmsg_seq:%d nlmsg_pid:%d\n",sizeof(struct nlmsghdr),nlh->nlmsg_len,nlh->nlmsg_type,nlh->nlmsg_flags,nlh->nlmsg_seq,nlh->nlmsg_pid);
    printf("sizeof(struct rtmsg):%d \n",sizeof(struct rtmsg));
    /* If we received all data ---> break */
    /* We are just intrested in Routing information */
    for ( ; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len))
    {
        struct rtmsg * route_entry = (struct rtmsg *) NLMSG_DATA(nlh);
	int route_netmask = route_entry->rtm_dst_len;
	int route_protocol = route_entry->rtm_protocol;
        int route_table_id = route_entry->rtm_table;

        bzero(destination_address, sizeof(destination_address));
        bzero(gateway_address, sizeof(gateway_address));

	 route_attribute = (struct rtattr *) RTM_RTA(route_entry);
        /* Get the route atttibutes len */
        route_attribute_len = RTM_PAYLOAD(nlh);

        for ( ; RTA_OK(route_attribute, route_attribute_len); \
            route_attribute = RTA_NEXT(route_attribute, route_attribute_len))
        {
       	    if (route_attribute->rta_type == RTA_DST)
            {
                inet_ntop(AF_INET, RTA_DATA(route_attribute), \
                        destination_address, sizeof(destination_address));
            }
            if (route_attribute->rta_type == RTA_GATEWAY)
            {
                inet_ntop(AF_INET, RTA_DATA(route_attribute), \
                        gateway_address, sizeof(gateway_address));
            }
	}
        if (nlh->nlmsg_type == RTM_DELROUTE)
            printf("Deleting route to destination --> %s/%d proto %d table id %d and gateway %s\n", destination_address, route_netmask, route_protocol,route_table_id, gateway_address);
        if (nlh->nlmsg_type == RTM_NEWROUTE)
            printf("Adding route to destination --> %s/%d proto %d table id %d and gateway %s\n", destination_address, route_netmask, route_protocol, route_table_id, gateway_address);
    }
}

void *thread_run(void *args){
    unsigned char    buff[4096];
    int connfd = *(int*)args;
    printf("thread_run connfd:%d \n",connfd);
    int n;
    int ret;
    struct fpm_msg_hdr_t_ *fpm_head;
    while(1){
        memset(&buff, 0, sizeof(buff));
        n = recv(connfd, buff, MAXLINE, 0);
        buff[n] = '\0';
        printf("recv %d byte from client: \n",n);
        fpm_head = (struct fpm_msg_hdr_t_ *)buff;
        ret = fpm_msg_ok(fpm_head,n);
        if(ret){
                printf("msg ok!\n");
                printf("%d %d fpm_msg_len:%d fpm_msg_date_len:%d\n",fpm_head->version,fpm_head->msg_type,fpm_msg_len(fpm_head),fpm_msg_data_len(fpm_head));
                if(fpm_head->msg_type == FPM_MSG_TYPE_NETLINK) {
                        printf("FPM_MSG_TYPE_NETLINK \n");
                        decode(fpm_msg_data(fpm_head),fpm_msg_data_len(fpm_head));
                } else if(fpm_head->msg_type == FPM_MSG_TYPE_PROTOBUF) {
                        printf("FPM_MSG_TYPE_PROTOBUF \n");
                }
        } else {
            printf("msg error!\n");
            //close(connfd);
           // return ;
           continue;
        }
    }
    printf("thread end...\n");
}

int main(int argc, char** argv)
{
    int    listenfd, connfd;
    struct sockaddr_in     servaddr;
    unsigned char    buff[4096];
    int     n;
    int tid;
    int i;
    struct fpm_msg_hdr_t_ fpm_head;
    int ret;
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(1234);

    if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
    	printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

    if( listen(listenfd, 10) == -1){
    	printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
    	exit(0);
    }

    printf("======waiting for client's request======\n");
    while(1){
    	if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
        	printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        	continue;
    	}
        printf("accept connfd:%d",connfd);
        ret = pthread_create(&tid,NULL,thread_run,&connfd);
        if(ret)
  	    printf("thread create error!\n");
    }
    close(listenfd);
}

