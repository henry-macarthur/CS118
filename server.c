#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
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

void check_num(int * num)
{
    if(*num > 25600)
    {
        *num = 0; //or do i need to offset this 
    }
}

struct packet
{
    struct header h;
    char data[DATA_SIZE];
};

int main(int argc, char ** argv)
{
    struct sockaddr_in serv_addr, client_addr; //create the structs for the current client server connection
    socklen_t client_length; 
    char host_name[10] = "localhost";
    int port = atoi(argv[1]); //grab the port from input for now
    int client_sz;
    //setup the server
    bzero((char * ) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); //create the socket 
    if(socket_fd < 0)
    {
        fprintf(stdout, "error creating socket! \n");
        exit(1);
    }
    //bind the server now
    if(bind(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "error binding socket! \n");
        exit(1);
    }
    printf("SERVER RUNNING on port %d! \n", port);
    struct packet send_packet = {};
    struct packet rec_packet = {};
    client_sz = sizeof(serv_addr);
    //char buff[50];
    int open_for_connection = 1;
    int wait_for_client = 1;
    //INIT CLIENT CONNECTION
    int am_rd;
    while(1)
    {
        if(open_for_connection)
            if((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *) &serv_addr, &client_sz)) < 0)
            {
                printf("error! \n");
                exit(1);
            }

        if(rec_packet.h.syn == 1 && rec_packet.h.ack == 0 && am_rd == 12) //meas init connection
        {
            open_for_connection = 0;
            if(0 == 0) //child process
            {

                printf("seq: %d, ack:%d \n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                //we need to first send back the 2nd part of the 3 way handshake
                int seq_num = rec_packet.h.seq_num;
                //struct packet pack = {};
                send_packet.h.ack = 1;
                send_packet.h.syn = 1;

                int expected_seq_num = seq_num + 1;
                send_packet.h.seq_num = rand()%(25600 + 1) + 0;
                check_num(&expected_seq_num);
                send_packet.h.ack_num = expected_seq_num;
                printf("seq: %d, ack:%d \n", send_packet.h.seq_num, send_packet.h.ack_num);
                if(sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) //send back handshake
                {
                    printf("error!");
                    exit(1);
                }

                //am expecting ACK with data, followe by packets
                //do a single read, then consec reads till input buffer is empty 
                bzero((char * ) &rec_packet, sizeof(rec_packet));
                
                if((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *)& serv_addr, &client_sz)) < 0)
                {
                    printf("error! \n");
                    exit(1);
                }
                else
                {
                    write(1, rec_packet.data, am_rd);
                }
                

                if(rec_packet.h.ack == 1 && rec_packet.h.seq_num == expected_seq_num) //check to make sure the ack and seq, num are as expected
                {
                    printf("ok connected! \n");
                    //send ack back
                }
                else
                {
                    printf("invalid 3rd part of handshake!! \n");
                    exit(1);
                }
                
                //int lasts = rec_packet.h.seq_num;
                while((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *)& serv_addr, &client_sz)) > 0)
                {
                    write(1, rec_packet.data, am_rd);
                    bzero((char * ) &rec_packet, sizeof(rec_packet));
                }
                exit(0);

            }
            
        }
    }
    //can just send data over
}