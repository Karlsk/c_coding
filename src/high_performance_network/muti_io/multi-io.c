#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#define TCP_PORT 9999 
#define ENABLE_NOBLOCK 0
#define ENABLE_MUTITHREAD 0
#define ENABLE_SELECT 0
#define ENABLE_POLL 0

#define BUFFER_LEN 1024 
#define POLL_SIZE 1024
#define EPOLL_EVENT_SIZE 1024

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
#if 0
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
}
#endif

#elif ENABLE_SELECT // select 
    //select(maxfd，rfds, wfds, efds, timeout);
    fd_set rfds, rset;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int maxfd = sockfd;
    int clientfd = 0;
    while(1)
    {
        rset = rfds;
        int nready = select(maxfd+1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(sockfd, &rset))
        {
            clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
            printf("clientfd: %d\n", clientfd);
            FD_SET(clientfd, &rfds);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            if (--nready == 0) continue;
        }
        int i = 0;
        for (i = sockfd+1; i <= maxfd; i++)
        {
            if (FD_ISSET(i, &rset))
            {
                char buffer[BUFFER_LEN] = {0};
                int ret = recv(i, buffer, BUFFER_LEN, 0);
                if (ret == 0)
                {
                    close(i);
                    break;
                }
                printf("ret: %d, bufer: %s\n", ret, buffer);
                send(i, buffer, ret, 0);
            }
            
        }
        

    }
#elif ENABLE_POLL
    // int poll(struct pollfd *fds, nfds_t nfds, int timeout);
    struct pollfd fds[POLL_SIZE] = {0};
    fds[sockfd].fd = sockfd;
    fds[sockfd].events = POLLIN;
    
    int maxfd = sockfd;
    int clientfd = 0;
    while(1) {
        int nready =  poll(fds, maxfd+1, -1);
        if (fds[sockfd].revents & POLLIN)
        {
            clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
            printf("clientfd: %d\n", clientfd);
            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            if (--nready == 0) continue;
        }
        int i = 0;
        for ( i = sockfd; i <= maxfd+1; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                char buffer[BUFFER_LEN] = {0};
                int ret = recv(i, buffer, BUFFER_LEN, 0);
                if (ret == 0)
                {
                    fds[i].fd = -1;
                    fds[i].events = 0;
                    close(i);
                    break;
                }
                printf("ret: %d, bufer: %s\n", ret, buffer);
                send(i, buffer, ret, 0);
            }
        }
        
    }
#else // epoll
    // 1. epoll_create(int size)
    // 2. epool_ctl(int epfd, EPOLL_OP, int fd, event)
    // 3. epoll_wait(epfd,  events, length, 0)

    int epfd = epoll_create(1);
    int clientfd = 0;
    struct epoll_event ev;
    ev.data.fd = sockfd;    //用来事件触发的时候传参数
    ev.events = EPOLLIN;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    while (1)
    {
        struct epoll_event events[EPOLL_EVENT_SIZE] = {0};
        int nready = epoll_wait(epfd, events, EPOLL_EVENT_SIZE, -1);
        if(nready < 0 ) continue;

        int i = 0;
        for ( i = 0; i < nready; i++)
        {
            int connfd = events[i].data.fd;
            if (connfd == sockfd && events[i].events & EPOLLIN) // accept
            {
                clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
                printf("clientfd: %d\n", clientfd);
                ev.data.fd = clientfd;
                ev.events = EPOLLIN;
                epoll_ctl(epfd, EPOLL_CTL_ADD, clientfd, &ev);
            }else if (events[i].events & EPOLLIN)
            {
                    char buffer[BUFFER_LEN] = {0};
                    // ET需要配合非阻塞循环
                    int ret = recv(connfd, buffer, BUFFER_LEN, 0);
                    if (ret == 0)
                    {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, connfd, NULL);
                        close(connfd);
                        printf("close!!!\n");
                        break;
                    }
                    printf("connfd: %d, ret: %d, bufer: %s\n", connfd, ret, buffer);
                    send(connfd, buffer, ret, 0);
            }
            else if(events[i].events & EPOLLOUT )
            {

                
            }
        }
        
    }
    

#endif
    
    close(sockfd);
    return 0;
}
