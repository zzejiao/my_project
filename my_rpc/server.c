
#include "server_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h> //strtok
#include "udp.h"



// Track up to 100 connected clients
#define MAX_CLIENTS 100

// call table, an array of structs that contains a unique identifier for you client,
// and a pointer to the function that you want to call.

//Receiving socket for server, set in the main function
struct socket server_socket;

typedef struct call_table_entry
{
    int clientId;
    int tracked_seq_number;
    int threadID;
    int argument[5];
    int retValue;//Use this to store the old call result
} call_table;

typedef struct thr_data
{
    int threadId;
    struct packet_info packet;
    int call_table_index;
    int clientID;
} thr_data;

call_table table[MAX_CLIENTS];
pthread_t threads[MAX_CLIENTS];
thr_data threadData[MAX_CLIENTS];
int threadUsed[MAX_CLIENTS]; // if a thread is used, set to 1, else 0
int clientNum=0; // number of clients already connected

int *parseArg(char *msg, int *argArray, int *argNum)
{
    char *token=malloc((BUFLEN));
    token = strtok(msg, "\n");
    int i = 0;
    while (token != NULL)
    {
        argArray[i] = atoi(token);
        //This had an error, changed "\n" to msg
        token = strtok(NULL, "\n");
        i++;
    }
    *argNum = i;
    return argArray;
}

int getFreeThreadID(void)
{
    int i = 0;
    while (threadUsed[i]) // while loop ends when threadUsed[i] is 0(not used)
    {
        i++;
    }
    //set the thread to inUse
    threadUsed[i]=1;
    return i;
}

int findClientIndex(int clientID)
{
    for (int i = 0; i < clientNum; i++)
    {
        if (table[i].clientId == clientID)
            return i;
    }
    return -1;
}

void *execute_request(void *arg)
{
    thr_data *data = (thr_data *)arg;
    struct packet_info packet = data->packet;

    // parse the message
    int argNum = 5;
    int argArray[5];
    parseArg(packet.buf, argArray, &argNum);

    // check if this client is new
    int index = findClientIndex(argArray[0]);
    char payload[BUFLEN];

    if (index == -1) // if threadId == -1, this client is new
    {
        // if it is new, assign a new entry in call table
        //werent checking if we exceeded client limit.  Added error handling
        index = clientNum;
        clientNum++;
        if(clientNum>100){
            printf("Error:Server at capacity.\n");
        }

        //initialize the call table entry
        table[index].clientId = argArray[0];
        //pssible error, we set the seq number when we have a new request but this might make our if statements think we already made this request
        //CHANGED THIS TO -1, I did this because we would treat this request like an old request since seqNum==seqNum
        table[index].tracked_seq_number = 0;
        table[index].threadID = data->threadId;
        for (int i = 0; i < argNum; i++)
        {
            table[index].argument[i] = argArray[i];
        }
        
    }
    // not new client, we use index to find the client and check its request
    int new_seq_number = argArray[1];
    // compare the tracked seq number with the seq number in the request
    if (new_seq_number > table[index].tracked_seq_number)
    {
        // if the new seq number is larger, update the tracked seq number and execute the request
        table[index].tracked_seq_number = new_seq_number;

        // execute the request
        switch (argArray[2])
        {
        case 1:
            idle(argArray[3]);
            //Set return value
            table[index].retValue=0;
            break;
        case 2:
            table[index].retValue=get(argArray[3]);
            break;
        case 3:
            table[index].retValue=put(argArray[3], argArray[4]);       
            break;
        default:
            printf("Invalid request No: %d\n", argArray[2]);
            break;
        }
        //end execution
        //Add client Id, seq number, and ret val to payload
        sprintf(payload, "%d\n%d\n%d\n", table[index].clientId,table[index].tracked_seq_number,table[index].retValue);
        //Thread id no longer in use so set to 0
        table[index].threadID=0;
        threadUsed[data->threadId]=0;
        //send packet back
        send_packet(server_socket, packet.sock, sizeof(packet.sock), payload, BUFLEN);
        //exit thread?
        pthread_exit(NULL);
    }
    else if (new_seq_number == table[index].tracked_seq_number)
    {
        // if the new seq number is the same, it is duplicate of last RPC or still in progress.
        // if duplicate, resend result
        // if in progress, send ACK back
        if(table[index].threadID==1){
            //thread is still being used, ACK
            //Fill buffer with ACK
            sprintf(payload,"ACK");
            printf("sending ack in server");
            send_packet(server_socket, packet.sock, sizeof(packet.sock), payload, 3);
            //Set the current threads in use to 0
            //Explaining above: If we are executing a request for a seq num its thread is in use.  When we request same seq again we create a new thread
            //We grab data from call table about the oriignal request and check if its thread is still in use
            //If it is, we send ack, and send the current threadID(The thread used to check status) to 0.
            threadUsed[data->threadId]=0;
            pthread_exit(NULL);
        }
        else{
            //thread is not being used, return old result
            //Fill payload same way it was filled in clinet.c
            sprintf(payload, "%d\n%d\n%d\n", table[index].clientId,table[index].tracked_seq_number,table[index].retValue);
            //send packet back
            send_packet(server_socket, packet.sock, sizeof(packet.sock), payload, BUFLEN);
            pthread_exit(NULL);
        }
    }
    else if (new_seq_number < table[index].tracked_seq_number)
    {
        // if the new seq number is smaller, discard message and ignore the request
        threadUsed[data->threadId]=0;
        pthread_exit(NULL);
    }

    return NULL;
}

int main(int argv, char **argc)
{
    if (argv != 2)
    {
        printf("Usage: %s <port>\n", argc[0]);
        exit(1);
    }
    int port = atoi(argc[1]);
    //-----------------take argument port from command line-----------------

    // initiate the socket
    server_socket = init_socket(port);
    struct packet_info request;
    int rc;
    // initiate the server loop
    while (1)
    {
        request = receive_packet(server_socket);
        // parse the request
        if (request.recv_len) // if the request's packet length is not 0, it means it  got something
        {
            int threadId = getFreeThreadID();
            threadData[threadId].packet = request;
            threadData[threadId].threadId = threadId;
            // create a pthread for this request
            if ((rc = pthread_create(&threads[threadId], NULL, execute_request, &threadData[threadId])))
                fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        }
    }

}