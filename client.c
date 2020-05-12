#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define MSG_SIZE 524
#define DATA_SIZE 512
#define HEADER_SIZE 12

struct header
{
    int seq_num;
    int ack_num;
    char ack;
    char syn;
    char fin;
    char pad;
};

struct packet
{
    struct header h;
    char data[DATA_SIZE];
};


int main(int argc, char ** argv)
{
    struct sockaddr_in serv_addr, client_addr; //create the structs for the current client server connection
    socklen_t client_length; 
    char * host_name = argv[1];
    int port = atoi(argv[2]); //grab the port from input for now
    char * filename = argv[3];

    bzero((char * ) &serv_addr, sizeof(serv_addr));

    //check if the input is a valid ip
    struct hostent * server;
    if(inet_pton(AF_INET, host_name, &(serv_addr.sin_addr.s_addr)) == 0)
    {
        server = gethostbyname(host_name);
        if(server == NULL)
        {
            fprintf(stdout, "invlaid hostname! \n");
            exit(1);
        }
        else
        {
            bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);   
        }
    }

    
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); //create the socket 
    if(socket_fd < 0)
    {
        fprintf(stdout, "error creating socket! \n");
        exit(1);
    }
    int client_sz = sizeof(serv_addr);
    //init connection
    //==========================================
    srand(time(0)); 
    struct header cur_header = {};
    int current_seq = rand()%(25600 + 1) + 0;
    cur_header.syn = 1;
    cur_header.ack_num = 0;
    cur_header.seq_num = current_seq;
    struct packet send_packet = {};
    struct packet rec_packet = {};
    send_packet.h = cur_header;
    
    printf("seq: %d, ack: %d\n", current_seq, cur_header.ack_num);

    //
    if(sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("error!");
        exit(1);
    }
    int am_rd;
    if((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *) &serv_addr, &client_sz)) < 0)
    {
        printf("error! \n");
        exit(1);
    }

    int valid = 0;
    if(rec_packet.h.syn == 1 && rec_packet.h.ack == 1 && rec_packet.h.ack_num == send_packet.h.seq_num + 1)
    {
        valid = 1;
        printf("seq: %d, ack: %d \n", rec_packet.h.seq_num, rec_packet.h.ack_num);
    }
    int fd;
    if(valid) //we are ready to start sending data!
    {
    //ready to send back 3rd part of the handshake
        fd = open(filename, O_RDONLY);
        bzero((char * ) &send_packet, sizeof(send_packet));
        int rd = read(fd, send_packet.data, DATA_SIZE);
        
        send_packet.ack = 1;
        send_packet.h.ack_num = rec_packet.h.seq_num + 1;
    }
    else
    {
        exit(1);
    }
    
}