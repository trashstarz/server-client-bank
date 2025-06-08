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
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "4444" // the port client will be connecting to

#define MAXDATASIZE 255 // max number of bytes we can get at once


struct User
{
    char phone[50];
    char password[50];
    float balance;
};

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void sendFile(int sockfd, char *filename)
{
    int fd;
    struct stat file_stat;
    off_t offset;
    ssize_t len;
    int remain_data = 0;
    int sent_bytes = 0;
    char file_size_send[256];

    // Open the file
    fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        //fprintf(stderr, "Error opening file --> %s", strerror(errno));

        exit(EXIT_FAILURE);
    }
    /* Get file stats */

    if (fstat(fd, &file_stat) < 0)
    {
        //fprintf(stderr, "Error fstat --> %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    sprintf(file_size_send, "%d", file_stat.st_size);

    len = send(sockfd, file_size_send, sizeof(file_size_send), 0);
    if (len < 0)
    {
        //fprintf(stderr, "Error on sending greetings --> %s", strerror(errno));

        exit(EXIT_FAILURE);
    }

    //fprintf(stdout, "Server sent %d bytes for the size\n", len);

    offset = 0;
    remain_data = file_stat.st_size;
    /* Sending file data */
    while (((sent_bytes = sendfile(sockfd, fd, &offset, BUFSIZ)) > 0) && (remain_data > 0))
    {
        //fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
        remain_data -= sent_bytes;
        //fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
    }
    close(fd);
}

void receiveFile(int sockfd, char* filename)
{
    int remain_data = 0;
    ssize_t len;
    char buffer[BUFSIZ];
    FILE* received_file;
    int file_size_receive;

    recv(sockfd, buffer, BUFSIZ, 0);
    file_size_receive = atoi(buffer);

    received_file = fopen(strcat(filename, ".dat"), "w");
    if (received_file == NULL)
    {
        //fprintf(stderr, "Failed to open file foo --> %s\n", strerror(errno));

        exit(EXIT_FAILURE);
    }

    remain_data = file_size_receive;

    while ((remain_data > 0) && ((len = recv(sockfd, buffer, BUFSIZ, 0)) > 0))
    {
        fwrite(buffer, sizeof(char), len, received_file);
        remain_data -= len;

    }
    fclose(received_file);
}

void handle_sigint(int sig){
    printf("dont ctrl c or youll WIPE your ENTIRE bank account \n");
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv, fd;
    char s[INET6_ADDRSTRLEN];

    ssize_t len;
    int sent_bytes = 0;
    int file_size_receive;
    char file_size_send[256];
    int remain_data = 0;
    struct stat file_stat;
    off_t offset;
    char buffer[BUFSIZ];
    FILE* received_file;




    if (argc != 2)
    {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0)
    {
        //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        //fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    //printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure





    while (1)
    {
        struct User usr, recipient;
        int opt;
        FILE *fp;
        char filename[50];
        char recipientFilename[50];
        char phone[50];
        char password[50];
        int cont = 1;
        float amount;
        struct sigaction sa;
        sa.sa_handler = &handle_sigint;
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, NULL);



        system("clear");
        printf("\nATM");
        printf("\n\n1. REGISTER");
        printf("\n2. LOGIN\n");
        scanf("%d", &opt);


        if (opt == 1)
        {

            system("clear");
            printf("\nPhone number:\t");
            scanf("%s", usr.phone);
            printf("\nPassword:\t");
            scanf("%s", usr.password);
            usr.balance = 0;
            strcpy(filename, usr.phone);


            fp = fopen(strcat(filename, ".dat"), "w");
            if (fp == NULL){
                return 4;
            }

            fwrite(&usr, sizeof(struct User), 1, fp);
            if (fwrite != 0)
            {
                printf("\nRegistration successful! Message bank to activate account.\n");
                break;

            }
            else
            {
                printf("\nSomething went wrong\n");
            }
            fclose(fp);
        }
        else if (opt == 2)
        {

            system("clear");
            printf("\nEnter phone number:\t");
            scanf("%s", phone);

            strcpy(filename, phone);

            bzero(buf, MAXDATASIZE);
            strcpy(buf, phone);

            //SEND1
            if (send(sockfd, buf, strlen(buf), 0) == -1) perror("send");

            //RECEIVE2
            receiveFile(sockfd, filename);

            printf("\nPassword:\t\t");
            scanf("%s", password);

            fp = fopen(filename, "r");
            if (fp == NULL){
                return 5;
            }

            fread(&usr, sizeof(struct User), 1, fp);
            fclose(fp);
            system("clear");

            if (!strcmp(password, usr.password))
            {
                printf("\nWelcome");

                while (cont != 0)
                {

                    printf("\n1. CHECK BALANCE");
                    printf("\n2. DEPOSIT");
                    printf("\n3. WITHDRAW");
                    printf("\n\n0 TO EXIT\n\n");
                    scanf("%d", &cont);

                    switch(cont)
                    {
                    case 0:
                        //SEND3
                        sendFile(sockfd, filename);
                        remove(filename);
                        return 0;
                    case 1:
                        system("clear");

                        printf("\nYour balance is %.2fEur\n", usr.balance);

                        break;
                    case 2:
                        system("clear");

                        printf("\nEnter the amount: ");
                        scanf("%f", &amount);

                        if (amount < 0.f){
                            printf("Why would you want to go into debt?\n");
                            break;
                        }
                        usr.balance += amount;
                        fp = fopen(filename, "w");
                        fwrite(&usr, sizeof(struct User), 1, fp);
                        fclose(fp);

                        break;
                    case 3:
                        system("clear");

                        printf("\nEnter the amount: ");
                        scanf("%f", &amount);

                        if (amount < 0.f){
                            printf("Deposit adds money, try that\n");
                            break;
                        }
                        usr.balance -= amount;
                        fp = fopen(filename, "w");
                        fwrite(&usr, sizeof(struct User), 1, fp);
                        fclose(fp);

                        break;
                        /*
                        I tried to implement a transfer function but idk how cause the current
                        protocol is send-receive-send for client and the opposite for server,
                        having this jumbles the order and it doesnt work

                        Hopefully in the future im smart enough to actually do that

                        A bandaid solution would be to bruteforce the transfer by forking it and
                        excelp mv *.dat ../server but that wouldnt really be an ATM<==>BANK system then,
                        more like ATM==BANK, where every atm has a bank storage server attached to it = not very safe!
                        (?does this make sense?)

                        The code below might not make any sense at parts its left in debugging state mostly

                        case 4:
                            system("clear");

                            printf("\nEnter the account number you wish to transfer to: ");
                            scanf("%s", &phone);

                            strcpy(filename,phone);
                            strcpy(recipientFilename, phone);
                                    bzero(buf, MAXDATASIZE);
                                    strcpy(buf, recipientFilename);
                                    if (send(sockfd, buf, strlen(buf), 0) == -1) perror("send");

                                    recv(sockfd, buffer, BUFSIZ, 0);
                                    file_size_receive = atoi(buffer);
                                    //fprintf(stdout, "\nFile size : %d\n", file_size);

                                    received_file = fopen(strcat(recipientFilename, ".dat"), "w");
                                    if (received_file == NULL)
                                    {
                                        fprintf(stderr, "Failed to open file foo --> %s\n", strerror(errno));

                                        exit(EXIT_FAILURE);
                                    }

                                    remain_data = file_size_receive;

                                    while ((remain_data > 0) && ((len = recv(sockfd, buffer, BUFSIZ, 0)) > 0))
                                    {
                                        fwrite(buffer, sizeof(char), len, received_file);
                                        remain_data -= len;

                                    }
                                    fclose(received_file);
                            fp = fopen(strcat(filename, ".dat"), "r");

                            if (fp == NULL)
                            {
                                printf("\nAccount not found.");
                                break;
                            }

                            printf("\nEnter the ammount to transfer to [%s]:", phone);
                            scanf("%f", &amount);

                            if (amount > usr.balance)
                            {
                                printf("\nInsufficient funds");
                                break;
                            }
                            else
                            {

                                fread(&recipient, sizeof(struct User), 1, fp);
                                fclose(fp);

                                fp = fopen(filename, "w");
                                recipient.balance += amount;
                                fwrite(&recipient, sizeof(struct User), 1, fp);
                                fclose(fp);

                                if (fwrite != NULL)
                                {
                                    printf("\nYouve transferred %.2f to %s", amount, phone);

                                    strcpy(filename, usr.phone);
                                    fp = fopen(strcat(filename, ".dat"), "w");
                                    usr.balance -= amount;
                                    fwrite(&usr, sizeof(struct User), 1, fp);
                                    fclose(fp);




                                    sendFile(sockfd, recipientFilename);
                                    break;

                                }
                            }*/

                    }
                }
            }
            else
            {
                printf("\nInvalid password\n");

                //SEND3
                sendFile(sockfd, filename);
                remove(filename);
                return 1;
            }

        }
    }


    close(sockfd);

    return 0;
}
