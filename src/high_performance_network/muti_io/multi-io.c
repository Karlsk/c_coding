#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>

#define TCP_PORT 9999 
#define ENABLE_NOBLOCK 0
#define ENABLE_MUTITHREAD 1

#define BUFFER_LEN 1024 

// 1 con 1 thread
void *client_thread(void *arg)
{
    int clientfd = *(int*)arg;

    while (1)
    {
        char buffer[BUFFER_LEN] = {0};
        int ret = recv(clientfd, buffer, BUFFER_LEN, 0);
        if (ret == 0)
        {
            close(clientfd);
            break;
        }
        printf("ret: %d, bufer: %s\n", ret, buffer);
        send(clientfd, buffer, ret, 0);
    }
}

int main(int argc, char const *argv[])
{
    int sockfd =  socket(AF_INET,SOCK_STREAM, 0);

    struct sockaddr_in  servaddr;
    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    if (-1 == bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)))
    {
        printf("bind failed: %s", strerror(errno));
        return -1;
    }
    listen(sockfd, 10);

#if ENABLE_NOBLOCK
    int flags = fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);
#endif
    struct sockaddr_in  clientaddr;
    memset(&clientaddr, 0, sizeof(struct sockaddr_in));
    socklen_t len  = sizeof(struct sockaddr_in);
    int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
    while(1) {
        int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
#if ENABLE_MUTITHREAD
        pthread_t threadid;
        pthread_create(&threadid, NULL, client_thread, &clientfd); 
#else
        char buffer[BUFFER_LEN] = {0};
        int ret = recv(clientfd, buffer, BUFFER_LEN, 0);
        printf("ret: %d, bufer: %s\n", ret, buffer);
        send(clientfd, buffer, ret, 0);
#endif
    }
    close(sockfd);
    return 0;
}
