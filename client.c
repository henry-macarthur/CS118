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

struct sockaddr_in serv_addr, client_addr;

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

int base_index = 0;
int end_of_window = 0;
struct packet window[10]; //will hold all of our data;
int packet_sizes[10];
char filled[10]; //tells us wether or not we have a packet here
int last_seq = -1;
int last_packet_length;

void check_num(int * num)
{
    *num = *num % 25601;
} 

void check_index(int * index)
{
    if(*index >= 10)
    {
        *index = 0;
    }
}
void sendpackets(int fd, int rdfd,  int num_packets, int * base)
{
    int base_seq = *base;
    for(int i = 0; i < num_packets; i++)
    {
        struct packet cur = {};
        check_num(base);
        cur.h.seq_num = *base;
        int rd = read(rdfd, &cur.data, DATA_SIZE); //read data and load into temp buffer, need to send packet now
        if(rd <= 0)
            return;
        //need to check whether or not rd is 0
        //printf("%s \n", cur.data);
        //sendto(socket_fd, &send_packet, 12 + rd, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr))
        //write(1, &cur.data, rd);
        // char * bff = ((char *)&cur);
        // char send_bf[524];
        // memcpy(send_bf, bff, 524);
        //printf("%d \n", rd);
        if(sendto(fd, &cur, 12 + rd, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("err! \n");
            exit(1);
        }
        end_of_window++;
        if(rd != 512)
        {
            last_seq = cur.h.seq_num;
            last_packet_length = rd + 12;
        
        }
        check_index(&end_of_window);
        window[end_of_window] = cur;
        //printf("WINDOW %d %d\n", end_of_window, base_index);
        packet_sizes[end_of_window] = 12 + rd;

        *base += (rd + 12);
        check_num(base);
        if(rd != DATA_SIZE)
            return;

    }
}
int main(int argc, char ** argv)
{    //create the structs for the current client server connection
    for(int i = 0; i < 10; i++)
        packet_sizes[i] = 0;

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
    }
    int fd;
    if(valid) //we are ready to start sending data!
    {
    //ready to send back 3rd part of the handshake
        int expected;
        fd = open(filename, O_RDONLY);
        bzero((char * ) &send_packet, sizeof(send_packet));
        int rd = read(fd, send_packet.data, DATA_SIZE);
        
        send_packet.h.ack = 1;
        send_packet.h.ack_num = rec_packet.h.seq_num + 1;
        send_packet.h.seq_num = rec_packet.h.ack_num;
        check_num(&send_packet.h.seq_num);
        check_num(&send_packet.h.ack_num);

        packet_sizes[base_index] = 12 + rd;
        printf("%d %d \n", base_index, packet_sizes[base_index]);
        window[base_index] = send_packet;
        //write(1, send_packet.data, rd);
        if(sendto(socket_fd, (char *)&send_packet, 12 + rd, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("err!");
            exit(1);
        }
        int next = send_packet.h.seq_num + (rd + 12);
        expected = send_packet.h.seq_num + (rd + 12);
        sendpackets(socket_fd, fd,  9, &next);

        int end = 1;
        while(end)
        {
            am_rd = recvfrom(socket_fd, &rec_packet, 12, 0, (struct sockaddr *) &serv_addr, &client_sz);
            printf("%d, %d, %d \n", rec_packet.h.ack_num, expected, am_rd);
            if(rec_packet.h.ack == 1 && rec_packet.h.ack_num == expected) // will need to expand this later when i add a window
            {
                printf("%d %d %d \n", base_index, end_of_window ,packet_sizes[base_index]);
                if(rec_packet.h.ack_num == last_seq)
                {
                    expected += last_packet_length;
                    end = 0;
                }
                else
                    expected += packet_sizes[base_index];
                base_index++;
                sendpackets(socket_fd, fd,  1, &next);
                check_index(&base_index);
                check_num(&expected);

                
            }
            if(am_rd != 12)
                break;
        }


        //basically we send data if we have space in our window, 
        //if am_rd return 0 then we start shutting down connection
        


        //close connection
        // bzero((char * ) &send_packet, sizeof(send_packet));
        // send_packet.h.fin = 1;
        // sendto(socket_fd, (char *)&send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

        // bzero((char * ) &rec_packet, sizeof(rec_packet));
        // recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *) &serv_addr, &client_sz);
        // if(rec_packet.h.ack == 1)
        // {
        //     //do we close server or no?
        //     //close connectione
        //     exit(0);
        // }

        //now have to send the remaining 9 packets
    }
    else
    {
        exit(1);
    }
    
}