#include "client.h"
#include "udp.h"
#include <stdlib.h> //rand()
#include <fcntl.h>  //fcntl()
#include <stdio.h>  //sprintf()
#include <unistd.h> //sleep()
#include <time.h>   //time()
#include <string.h> //strtok


int getPayloadResult(char *msg)
{
    char *token=malloc((BUFLEN));
    token = strtok(msg, "\n");
    int i = 0;
    int retVal;
    while (token != NULL)
    {
        retVal = atoi(token);
        token = strtok(NULL, "\n");
        i++;
    }
    free(token);
    return retVal;
}

// initializes the RPC connection to the server
struct rpc_connection RPC_init(int src_port, int dst_port, char dst_addr[])
{
    struct rpc_connection rpc; // new a rpc connection

    // init_socket(defined in upd.c) will new a udp socket, bind it to your src_port
    rpc.recv_socket = init_socket(src_port);


    struct sockaddr_storage addr; // this is a generic network address structure that can hold network addresses of any protocol family.
    socklen_t addrlen;

    populate_sockaddr(AF_INET, dst_port, dst_addr, &addr, &addrlen);
    // using populate_sockaddr, we transform the given argument (dst_port,dst_addr) into a struct that is approriate for our rpc.dst_addr

    rpc.dst_addr = *((struct sockaddr *)&addr); // store the correct dst_addr into rpc.dst_addr
    rpc.dst_len = sizeof(rpc.dst_addr);


    // sequence number is a monotonically increasing number, used to track if requests are duplicate.
    rpc.seq_number = 1;

    // The client id is uniquely randomly selected on startup (rand() is okay).
    //set seed for rand(), so that rand() will return different value each time
    srand(time(NULL));
    rpc.client_id = rand();

    return rpc;
}


// Sleeps the server thread for a few seconds
void RPC_idle(struct rpc_connection *rpc, int time)
{
    // set the socket to blocking mode so that other request from this client will be blocked
    int flags = fcntl(rpc->recv_socket.fd, F_GETFL, 0);       // get the current flags
    fcntl(rpc->recv_socket.fd, F_SETFL, flags & ~O_NONBLOCK); // set to non-blocking mode by ANDing flags & ~O_NONBLOCK


    char payload[BUFLEN]; 
    sprintf(payload, "%d\n%d\n1\n%d\n", rpc->client_id, rpc->seq_number, time);
    // the third argument is the function number, 1 means idle
    // separate the argument in payload with a newline

    int retryCnt = 0;
    struct packet_info response;
    while (retryCnt < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, rpc->dst_addr, rpc->dst_len, payload, BUFLEN);

        // get response from server
        response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);

        // check if the response is an ack(meaning the server has received the packet and is processing it)
        // if yes, wait for 1 second and check again
        int waitAck = 1;
        while (response.recv_len == 3 && response.buf[0] == 'A' && response.buf[1] == 'C' && response.buf[2] == 'K')
        {
            printf("%d time waiting\n", waitAck++);                                                                           // debug msg
            printf("RPC_idle: client %d received an ACK from server, wait for 1 second and check again\n\n", rpc->client_id); // debug msg
            sleep(1);
            response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);                                                                                                         // TODO: NOT SURE this is correct
        }

        // if response valid, break the loop
        if (response.recv_len>0){
            rpc->seq_number++;
            break;
        }

        // if response invalid, retry
        // WriteUp: if the socket times out, receive_packet_timeout will return a packet with a length of 0.
        if (response.recv_len < 0)
            ++retryCnt;
    }
    if (retryCnt == RETRY_COUNT)
    {
        printf("RPC_idle: no response after 5 attempts, exit with an error message\n");
        exit(1);
    }

    fcntl(rpc->recv_socket.fd, F_SETFL, flags | O_NONBLOCK); // set to non-blocking mode by ORing flags | O_NONBLOCK
    // return getPayloadResult(response.buf);
}

// gets the value of a key on the server store
int RPC_get(struct rpc_connection *rpc, int key)
{
    int flags = fcntl(rpc->recv_socket.fd, F_GETFL, 0);       // get the current flags
    fcntl(rpc->recv_socket.fd, F_SETFL, flags & ~O_NONBLOCK); // set to non-blocking mode by ANDing flags & ~O_NONBLOCK

    char payload[BUFLEN];
    sprintf(payload, "%d\n%d\n2\n%d\n", rpc->client_id,rpc->seq_number,key);
    // the first argument is the function number, 2 means get()


    int retryCnt = 0;
    struct packet_info response;
    while (retryCnt < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, rpc->dst_addr, rpc->dst_len, payload, BUFLEN);

        response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);


        int ackCnt = 1; // if server send back ACK
        while (response.recv_len == 3 && response.buf[0] == 'A' && response.buf[1] == 'C' && response.buf[2] == 'K')
        {
            printf("%d time waiting\n", ackCnt++);                                                                            // debug msg
            printf("RPC_get: client %d received an ACK from server, wait for 1 second and check again\n\n", rpc->client_id); // debug msg
            sleep(1);  
            response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);                                                                                           
        }

        // if response valid, break the loop
        if (response.recv_len>0){
            rpc->seq_number++;
            break;
        }

        // if response invalid, retry
        if (response.recv_len < 0)
            ++retryCnt;
    }
    if (retryCnt == RETRY_COUNT)
    {
        printf("RPC_get: no response after 5 attempts, exit with an error message\n");
        exit(1);
    }

    fcntl(rpc->recv_socket.fd, F_SETFL, flags | O_NONBLOCK); // set to non-blocking mode by ORing flags | O_NONBLOCK
    
    return getPayloadResult(response.buf);

}

// sets the value of a key on the server store
// TODO:what does it return?
int RPC_put(struct rpc_connection *rpc, int key, int value)
{
    int flags = fcntl(rpc->recv_socket.fd, F_GETFL, 0);       // get the current flags
    fcntl(rpc->recv_socket.fd, F_SETFL, flags & ~O_NONBLOCK); // set to non-blocking mode by ANDing flags & ~O_NONBLOCK

    char payload[BUFLEN]; 
    sprintf(payload, "%d\n%d\n3\n%d\n%d\n", rpc->client_id, rpc->seq_number, key, value);
    // the third argument is the function number, 3 means put()

    int retryCnt = 0;
    struct packet_info response;
    
    while (retryCnt < RETRY_COUNT)
    {
        send_packet(rpc->recv_socket, rpc->dst_addr, rpc->dst_len, payload, BUFLEN);

        response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);
        
        int ackCnt = 1; // if server send back ACK
        while (response.recv_len == 3 && response.buf[0] == 'A' && response.buf[1] == 'C' && response.buf[2] == 'K')
        {
            printf("%d time waiting\n", ackCnt++);                                                                            // debug msg
            printf("rpc_put: client %d received an ACK from server, wait for 1 second and check again\n\n", rpc->client_id); // debug msg
            sleep(1);                
            response = receive_packet_timeout(rpc->recv_socket, TIMEOUT_TIME);                                                                                              // TODO: NOT SURE this is correct
        }

        // if response valid, break the loop
        if (response.recv_len>0){
            rpc->seq_number++;
            break;
        }

        // if response invalid, retry
        if (response.recv_len < 0)
            ++retryCnt;
    }
    if (retryCnt == RETRY_COUNT)
    {
        printf("rpc_put: no response after 5 attempts, exit with an error message\n");
        exit(1);
    }

    fcntl(rpc->recv_socket.fd, F_SETFL, flags | O_NONBLOCK); // set to non-blocking mode by ORing flags | O_NONBLOCK
    return getPayloadResult(response.buf);
}

// closes the RPC connection to the server
// cleanup that you need to do for your RPC variables
void RPC_close(struct rpc_connection *rpc)
{
    if (rpc->recv_socket.fd != -1) // TODO: not sure if checking fd is correct
        close(rpc->recv_socket.fd);
}