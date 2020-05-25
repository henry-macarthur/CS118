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

struct packet
{
    struct header h;
    char data[DATA_SIZE];
};

int base_seq_num;
int expected_seq_num; //what we look for whith acks
int base = 0;
int end_of_wndow = 9;
struct packet window[10]; //will hold all of our data;
char filled[10]; //tells us wether or not we have a packet here
int file_num = 1;
int file;
struct sockaddr_in serv_addr, client_addr; //create the structs for the current client server connection
socklen_t client_length; 

void check_num(int * num)
{
    *num = *num % 25601;
}

void update_index(int * indx)
{
    if(*indx >= 10)
    {
        *indx = (*indx) - 10;
    }
}

//indexing will be base += (seq_num - expected_seq_num)
//have to deal with edge case that seq_num < expected_seq_num
//calculate how many packets to end, and how many packets past 0 it is i guess

void sendAck(int fd, int seq_num, int am_rd, struct packet rec)
{
    struct packet cur = {};
    //printf("%d, %d, %d \n", expected_seq_num, seq_num, am_rd);
    if(seq_num == expected_seq_num)
    {
        //printf("%d %d\n", seq_num, am_rd);
        //write out the data, 
        write(file, rec.data, am_rd - 12);
        expected_seq_num += (am_rd);
        check_num(&expected_seq_num);
    }
    cur.h.ack = 1;
    cur.h.ack_num  = expected_seq_num; 
    //printf("%d \n", expected_seq_num);
    sendto(fd, &cur, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf("sendAck %d \n", cur.h.ack_num);

}
// void sendAck(int fd, int seq_num, int am_rd, struct packet rec)
// {
//     struct packet cur = {};
//     int index;
//     if(seq_num == expected_seq_num)
//     {
//         //can shift base or whatever
//         expected_seq_num += am_rd;
//         window[base] = rec;
//         //should be able to write the data
//         printf("%d, %d \n", base, end_of_wndow);
//         int nxt = base + 1;
//         end_of_wndow += 1;
//         update_index(&end_of_wndow);
//         update_index(&nxt);
//         while(filled[nxt])
//         {
//             //write the data
//             nxt += 1;
//             end_of_wndow += 1;
//             printf("%d, %d \n", nxt, end_of_wndow);
//             update_index(&nxt);
//             update_index(&end_of_wndow);
//         }
//         base = nxt;
//     }   
//     else
//     {
//         //calculate index
//         if(seq_num > expected_seq_num)
//             index = (base + (seq_num - expected_seq_num)/MSG_SIZE); 
//         else
//         {
//             index = ((25600 - expected_seq_num) / MSG_SIZE) + seq_num/MSG_SIZE;
//         }
//         update_index(&index);
//         window[index] = rec;
//         filled[index] = 1;
//     }
    
//     check_num(&expected_seq_num);
//     cur.h.ack = 1;
//     cur.h.ack_num = expected_seq_num;

//     //sendto(fd, &cur, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
// }


int main(int argc, char ** argv)
{
    for(int i = 0; i < 10; i++)
        filled[i] = 0;
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
        if(open_for_connection == 1)
        {
            if((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *) &serv_addr, &client_sz)) < 0)
            {
                printf("error! \n");
                exit(1);
            }
        }

        if(rec_packet.h.syn == 1 && rec_packet.h.ack == 0 && am_rd == 12) //meas init connection
        {
            open_for_connection = 0;

            char file_name[100];
            bzero(file_name, 100);
            char * ending = ".file";
            sprintf(file_name, "%d%s", file_num, ending);
            file_num++;
            file = open(file_name, O_CREAT | O_RDWR, S_IRWXU);
            if(file > 0)
            {
                open("%d \n", file);
            }
            if(0 == 0) //child process
            {
                //we need to first send back the 2nd part of the 3 way handshake
                int seq_num = rec_packet.h.seq_num;
                //struct packet pack = {};
                send_packet.h.ack = 1;
                send_packet.h.syn = 1;

                expected_seq_num = seq_num + 1;
                send_packet.h.seq_num = rand()%(25600 + 1) + 0;
                check_num(&expected_seq_num);
                send_packet.h.ack_num = expected_seq_num;

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
                   // write(1, rec_packet.data, am_rd);
                }
                

                if(rec_packet.h.ack == 1 && rec_packet.h.seq_num == expected_seq_num) //check to make sure the ack and seq, num are as expected
                {
                    base_seq_num = rec_packet.h.seq_num;
                    sendAck(socket_fd, rec_packet.h.seq_num, am_rd, rec_packet);
                }
                else
                {
                    printf("invalid 3rd part of handshake!! \n");
                    exit(1);
                }
                
                //int lasts = rec_packet.h.seq_num;
                while((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), 0, (struct sockaddr *)& serv_addr, &client_sz)) > 0)
                {
                    //write(1, rec_packet.data, am_rd);
                    //struct packet * pkt = ((struct packet *) rec_packet);
                    //printf("%d %d \n", expected_seq_num, rec_packet.h.seq_num);


                    //close connection, this needs to be updated to close server side!
                    if(rec_packet.h.fin == 1)
                    {
                        bzero((char * ) &send_packet, sizeof(send_packet));
                        send_packet.h.ack = 1;
                        sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        bzero((char * ) &send_packet, sizeof(send_packet));
                        send_packet.h.fin = 1;
                        sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        //open_for_connection = 1;
                        break;
                    }
                    else
                    {
                        sendAck(socket_fd, rec_packet.h.seq_num, am_rd, rec_packet);

                    }
                    
                    bzero((char * ) &rec_packet, sizeof(rec_packet));
                }
                
                bzero((char * ) &rec_packet, sizeof(send_packet));
                recvfrom(socket_fd, &rec_packet, 12, 0, (struct sockaddr *)& serv_addr, &client_sz);
                if(rec_packet.h.ack == 1)
                {
                    open_for_connection = 1;
                    close(file);
                }
                else
                {
                    exit(1);
                }
                

            }
            
        }
    }
    //can just send data over
}