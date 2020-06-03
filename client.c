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
#include <inttypes.h>
#include <math.h>
#include <sys/time.h>

#define MSG_SIZE 524
#define DATA_SIZE 512
#define HEADER_SIZE 12

struct sockaddr_in serv_addr, client_addr;
struct timespec spec;
long mili;

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
//int current_time;
int end_window;
int base_index = 0;
int end_of_window = 0;
struct packet window[10]; //will hold all of our data;
int packet_sizes[10];
char filled[10]; //tells us wether or not we have a packet here
int last_seq = -1;
int last_packet_length;
struct timeval  tm;
double first, cur_time;
int last_packet = 0;
int final_expected;
int in_window = 0;
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
        if(in_window != 10)
            in_window++;
        if(sendto(fd, &cur, 12 + rd, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("err! \n");
            exit(1);
        }
        printf("SEND %d %d\n", cur.h.seq_num, 0);
        //printf("send pakcet %d \n", cur.h.seq_num);
        end_of_window++;
        if(rd != 512)
        {
            last_seq = cur.h.seq_num;
            last_packet_length = rd + 12;
            last_packet = 1;
            final_expected = last_seq += last_packet_length;
        
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

    //long cur_time;
    
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
    int valid = 0;
    if(sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("error!");
        exit(1);
    }
    printf("SEND %d 0 SYN\n", current_seq);
    gettimeofday(&tm, NULL);
    first = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
    int am_rd;
    while(1)
    {
        gettimeofday(&tm, NULL);
        cur_time = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
        if(cur_time - first > 500)
        {
            sendto(socket_fd, &send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
            first = cur_time;
            printf("TIMEOUT %d\n", current_seq);
            printf("RESEND %d 0 SYN\n", current_seq);
            //RESEND hSeqNumi hAckNumi [SYN] [FIN] [ACK] [DUP-ACK]
            //resend the packet
        }
        bzero((char * ) &rec_packet, sizeof(rec_packet));
        if((am_rd = recvfrom(socket_fd, &rec_packet, sizeof(rec_packet), MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &client_sz)) <= 0)
        {
        }
        else if(am_rd > 0)
        {
            if(rec_packet.h.syn == 1 && rec_packet.h.ack == 1 && rec_packet.h.ack_num == send_packet.h.seq_num + 1)
            {
                printf("RECV %d %d SYN ACK\n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                valid = 1;
                //exit(1);
                break;
            }
            //printf("%d %d %d %d \n", rec_packet.h.syn, rec_packet.h.ack, rec_packet.h.ack_num, send_packet.h.seq_num + 1);
            //printf("rec syn ack \n");
            //exit(1);
        }
        
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
        //printf("%d %d \n", base_index, packet_sizes[base_index]);
        window[base_index] = send_packet;
        //write(1, send_packet.data, rd);
        if(sendto(socket_fd, (char *)&send_packet, 12 + rd, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("err!");
            exit(1);
        }
        else
        {
            printf("SEND %d %d ACK\n", send_packet.h.seq_num, send_packet.h.ack_num); //if we have to resend this, i
            if(rd < DATA_SIZE)
            {
                last_seq = send_packet.h.seq_num + 12 + rd;
                last_packet = 1;
                in_window = 1;
            }
        }
        //exit(1);
        gettimeofday(&tm, NULL);
        first = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
        int next = send_packet.h.seq_num + (rd + 12);
        expected = send_packet.h.seq_num + (rd + 12);
        sendpackets(socket_fd, fd,  9, &next);

        int end = 1;
        int last_ack = 0;
        int num_lost = 0;
        while(end)
        {
            gettimeofday(&tm, NULL);
            cur_time = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
            //printf("%f\n", cur_time - first);
            if(cur_time - first > 500)
            {
                printf("TIMEOUT %d\n", next);
                num_lost++;
                //write(1, "lost packet \n", 13);
                //resend the entire window
                int i = base_index;
                int counter = 0;
                while(counter < in_window)
                {

                    sendto(socket_fd, &window[i], packet_sizes[i], 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                    if(window[i].h.ack)
                    {
                        printf("RESEND %d %d DUP-ACK\n", window[i].h.seq_num, window[i].h.ack_num);
                    }
                    else
                    {
                        printf("RESEND %d %d\n", window[i].h.seq_num, 0);
                    }
                    
                    //printf("resend %d \n", window[i].h.seq_num);
                    //se
                    i++;
                    if(i >= 10)
                        i = 0;
                    counter++;
                }
            
                first = cur_time;

                continue;
            }
            am_rd = recvfrom(socket_fd, &rec_packet, 12, MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &client_sz);
            if(am_rd == EWOULDBLOCK || am_rd <= 0)
            {
                continue;
            }
            //printf("%d, %d, %d \n", rec_packet.h.ack_num, expected, am_rd);
            int end = expected +  (524 *10);
            check_num(&end);
            //printf("recieved %d expected %d or %d\n", rec_packet.h.ack_num, expected, end);
            if(rec_packet.h.ack == 1) //&& (rec_packet.h.ack_num >= expected && rec_packet.h.ack_num <= end|| (rec_packet.h.ack_num <= end && rec_packet.h.ack_num <= expected))) // will need to expand this later when i add a window
            {
                printf("RECV %d %d ACK\n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                if(last_packet && rec_packet.h.ack_num == last_seq)
                    break;
                if(end > expected)
                {
                    if(rec_packet.h.ack_num >= expected && rec_packet.h.ack_num <= end)
                    {}
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    if(rec_packet.h.ack_num >= expected || (rec_packet.h.ack_num <= end && rec_packet.h.ack_num <= expected))
                    {}
                    else
                    {
                        continue;
                    }
                    
                }
                

                expected = rec_packet.h.ack_num;
                //printf("recieve ack for %d \n", expected);
                //printf("%d %d %d \n", base_index, end_of_window ,packet_sizes[base_index]);
                if(last_ack == 1)
                {
                    break;
                }
                if(last_packet && (rec_packet.h.ack_num >= last_seq))
                {
                    expected += last_packet_length;
                    last_ack = 1;
                }
                else
                    expected += packet_sizes[base_index];
                base_index++;
                sendpackets(socket_fd, fd,  1, &next);
                check_index(&base_index);
                check_num(&expected);

                
            }
        }

        int sent_fin_ack = 0;

        //close connection
        int fexpected;
        bzero((char * ) &send_packet, sizeof(send_packet));
        send_packet.h.fin = 1;
        send_packet.h.seq_num = last_seq;
        sendto(socket_fd, (char *)&send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        //fix ack and seq num later
        printf("SEND %d %d FIN\n", send_packet.h.seq_num, send_packet.h.ack_num);
        fexpected = send_packet.h.seq_num + 1;
        gettimeofday(&tm, NULL);
        double snd = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
        int ack_lost = 0;
        while(1)
        {
            gettimeofday(&tm, NULL);
            cur_time = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
            if(cur_time - snd > 500)
            {
                printf("TIMEOUT %d\n", last_seq); //fix
                printf("RESEND %d %d FIN\n", last_seq, 0);
                send_packet.h.seq_num = last_seq;
                if(ack_lost)
                    exit(1);
                sendto(socket_fd, (char *)&send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                snd = cur_time;
            }
            bzero((char * ) &rec_packet, sizeof(rec_packet));
            if(recvfrom(socket_fd, &rec_packet, 12, MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &client_sz) > 0)
            {
                if(rec_packet.h.ack == 1 && rec_packet.h.ack_num == fexpected)
                {
                    printf("RECV %d %d ACK\n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                    break;
                }
                else if(rec_packet.h.fin == 1)
                {
                    printf("RECV %d %d FIN\n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                    ack_lost = 1;
                    bzero((char * ) &send_packet, sizeof(send_packet));
                    send_packet.h.ack = 1;
                    send_packet.h.seq_num = last_seq + 1;
                    send_packet.h.ack_num = rec_packet.h.seq_num + 1;
                    sendto(socket_fd, (char *)&send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                    if(sent_fin_ack)
                    {
                        printf("SEND %d %d DUP-ACK\n", send_packet.h.seq_num, send_packet.h.ack_num);
                    }
                    else
                    {
                        /* code */
                        printf("SEND %d %d ACK\n", send_packet.h.seq_num, send_packet.h.ack_num);
                        sent_fin_ack = 1;
                    }
                    
                    //send ACK
                    //get ready to close connection
                }
            }
        }
        //do a blocking wait now for FIN
        int start_end = 0;
        while(1)
        {
            bzero((char * ) &rec_packet, sizeof(rec_packet));
            if(recvfrom(socket_fd, &rec_packet, 12, MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &client_sz) > 0)
            {
                if(rec_packet.h.fin == 1)
                {
                    printf("RECV %d %d FIN\n", rec_packet.h.seq_num, rec_packet.h.ack_num);
                    bzero((char * ) &send_packet, sizeof(send_packet));
                    send_packet.h.ack = 1;
                    send_packet.h.seq_num = last_seq + 1;
                    send_packet.h.ack_num = rec_packet.h.seq_num + 1;
                    sendto(socket_fd, (char *)&send_packet, 12, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                    //printf("SEND %d %d DUP-ACK\n", send_packet.h.seq_num, send_packet.h.ack_num);
                    if(sent_fin_ack)
                    {
                        printf("SEND %d %d DUP-ACK\n", send_packet.h.seq_num, send_packet.h.ack_num);
                    }
                    else
                    {
                        printf("SEND %d %d ACK\n", send_packet.h.seq_num, send_packet.h.ack_num);
                        sent_fin_ack = 1;
                    }
                    start_end = 1;
                    gettimeofday(&tm, NULL);
                    cur_time = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
                }
            }

            gettimeofday(&tm, NULL);
            double closing_time = (tm.tv_sec) * 1000 + (tm.tv_usec) / 1000;
            //double idkk = closing_time - cur_time;
            //printf("tm %lf \n", idkk);
            if(start_end && (closing_time - cur_time >= 2000))
            {
                exit(0);
            }
        }
    }
    else
    {
        exit(1);
    }
    
}