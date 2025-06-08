#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>


#define PORT "4444"  // the port users will be connecting to

#define BACKLOG 10   // how many pending connections queue will hold
#define MAXDATASIZE 255

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}




// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;                     // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;   // addrinfo is a linked list, ai_next points to next element
    struct sockaddr_storage their_addr;     // connector's address information, this can be either v4 or v6, cast to w.e you need
    socklen_t sin_size;
    struct sigaction sa;                    //signal handler
    int yes=1;

    char s[INET6_ADDRSTRLEN];
    int rv, numbytes, fd;
    char buf[MAXDATASIZE];
    char filename[MAXDATASIZE];
    char buffer[BUFSIZ];

    ssize_t len;
    int sent_bytes = 0;
    char file_size[256];
    int file_size_receive;
    int remain_data = 0;
    struct stat file_stat;
    off_t offset;
    FILE* received_file;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;        // ipv4 or ipv6, unspec is either one
    hints.ai_socktype = SOCK_STREAM;    // TCP stream - sets up connection to receiver then sends data in segments
    hints.ai_flags = AI_PASSIVE;        // use my IP


    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) // IP, PORTNUMBER, HINTS POINTS TO ADDRINFO
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // servinfo now points to a linked list of 1 or more addrinfos, each containing a struct sockaddr

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,      // ipv4/6, stream or datagram, tcp or udp
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, // allows to reuse the port
                       sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)      // sock() fd, address, address lenght
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1)      //sockfd, how many connections allowed in queue
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;            // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;                   //if you use scanf u need this or it works weird ?
    if (sigaction(SIGCHLD, &sa, NULL) == -1)    //signal, reference to sigaction struct, null because no old sigaction
    {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1)    // main accept() loop
    {
        sin_size = sizeof their_addr;

        //FD FOR NEW CONNECTION
        //sockfd, typecasted pointer to sockaddr_storage where the information will go, size(accept wont put more than that into addr)
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,                         // convert ip to string and print it
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from [%s]\n", s);

        if (!fork())   // this is the child process
        {
            close(sockfd); // child doesn't need the listener


            //RECEIVE1
            if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1)
            {
                perror("recv");
                exit(1);
            }

            buf[numbytes] = '\0';
            strcpy(filename, buf);

            printf("server: received connection from [%s]\n",filename);

            fd = open(strcat(filename, ".dat"), O_RDONLY);
            if (fd == -1)
            {
                fprintf(stderr, "Error opening file --> %s\n", strerror(errno));

                exit(EXIT_FAILURE);
            }

            /* Get file stats */
            if (fstat(fd, &file_stat) < 0)  // obtains info about file associated with fd, fills in file_stat struct
            {
                fprintf(stderr, "Error fstat --> %s\n", strerror(errno));

                exit(EXIT_FAILURE);
            }

            fprintf(stdout, "File Size: \n%d bytes\n", file_stat.st_size);
            sprintf(file_size, "%d", file_stat.st_size);    // sends formated file size from stat struct to filesize

            //SEND
            len = send(new_fd, file_size, sizeof(file_size), 0);
            if (len < 0)
            {
                fprintf(stderr, "Error on sending file --> %s\n", strerror(errno));

                exit(EXIT_FAILURE);
            }

            fprintf(stdout, "Server sent %d bytes for the size\n", len);

            offset = 0;
            remain_data = file_stat.st_size;
            /* Sending file data */
            //sends to newfd from fd, starting at 0, and sends BUFSIZ amount
            while (((sent_bytes = sendfile(new_fd, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
            {
                fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
                remain_data -= sent_bytes;
                fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
            }

            //RECEIVE
            recv(new_fd, buffer, BUFSIZ, 0);
            file_size_receive = atoi(buffer);
            //fprintf(stdout, "\nFile size : %d\n", file_size);

            received_file = fopen(filename, "w");
            if (received_file == NULL)
            {
                fprintf(stderr, "Failed to open file foo --> %s\n", strerror(errno));

                exit(EXIT_FAILURE);
            }

            remain_data = file_size_receive;

            while ((remain_data > 0) && ((len = recv(new_fd, buffer, BUFSIZ, 0)) > 0))
            {
                fwrite(buffer, sizeof(char), len, received_file);
                remain_data -= len;

            }
            fclose(received_file);


            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}
