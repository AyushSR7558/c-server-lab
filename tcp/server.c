#include <sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include <sys/socket.h>
#include <netdb.h>
#include<signal.h>

#define PORT "3000"
#define BACKLOG 20


void signchld_handler(int s) {
    (void) s; // quite unused variable warning
        
    // waitpid() might overwrite errno, so we save and restore it
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa) {
    if(sa -> sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa) -> sin_addr);
    }

    return &(((struct sockaddr_in6*) sa) -> sin6_addr);
}

int main() {    
    int sockfd, new_fd;
    struct addrinfo hints, *res, *p;
    int yes = 1;
    struct sockaddr_storage their_addr; // connector's address info
    socklen_t sin_size;
    struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    int rv;


    // Listen on sock_fd, new connection on new_fd

    memset(&hints, 0, sizeof(hints));

    hints.ai_flags = AI_PASSIVE; // Use host machine address
    hints.ai_family = AF_INET; // Address will only have IPv4 addresses
    hints.ai_socktype = SOCK_STREAM; // Use tcp connection

    if((rv = getaddrinfo(NULL, PORT, &hints, &res) != 0)) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p -> ai_next) {
        if ((sockfd = socket(p -> ai_family, p -> ai_socktype, 0 )) == -1) {
            perror("server: socket ");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(res); // all done with this structure
    
    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = signchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if(sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server:waiting for connections...\n");

    while(1) {
        // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if(new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        if(!fork()) {
            close(sockfd); // child doen't need the listener
            if(send(new_fd, "Hello, World!", 13, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd); // parent doesn't need these
    }
    
    return 0;
}
