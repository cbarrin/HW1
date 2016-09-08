/*********************************************************
*
* Module Name: UDP Echo server 
*
* File Name:    UDPEchoServer.c	
*
* Summary:
*  This file contains the echo server code
*
* Revisions:
*  $A0:  6/15/2008:  don't exit on error
*
*********************************************************/
#include "UDPEcho.h"
#define MAX_CLIENT 100

void DieWithError(char *errorMessage);  /* External error handling function */


char Version[] = "1.1";   
struct client {                      /* Struct for storing client information */
    char ip_addr[20];
    unsigned short port;
    time_t start_time;
    time_t last_time;
    unsigned long recv_bytes;
    unsigned long avg_thrput;
    unsigned short avg_loss;
};

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[ECHOMAX];        /* Buffer for echo string */
    unsigned short echoServPort;     /* Server port */
    int recvMsgSize;                 /* Size of received message */
    double avgLossRate = 0.0;        /* Average Artificial Loss Rate */
    int debugFlag = 0;               /* Flag for debug level */
    int totalMsg = 0;                /* Total messages received by server */
    int totalSessions = 0;           /* Total number of client sessions */
    struct client clients[MAX_CLIENT]; 
    int current_session = 0;
    int matched = 0;                 

    if (argc < 2)         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> [<Average Loss Rate>] [<Debug Level>]\n", argv[0]);
        exit(1);
    }
    if (argc == 3){
        avgLossRate = atof(argv[2]);
    } else if (argc == 4) {
        avgLossRate = atof(argv[2]);
        debugFlag = atoi(argv[3]);
    } 

    echoServPort = atoi(argv[1]);  /* First arg:  local port */
//    printf("Values: PORT :%d\tLoss: %f\tLevel: %d\n",echoServPort,avgLossRate,debugFlag);
//#if debugFlag > 0
if (debugFlag > 0) {
    printf("UDPEchoServer(version:%s): Port:%d Loss:%f Debug:%d\n",(char *)Version,echoServPort,avgLossRate,debugFlag);    
}
//#endif
    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
//        DieWithError("socket() failed");
if (debugFlag > 0) {
      printf("Failure on socket call , errno:%d\n",errno);
}
    }

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
//        DieWithError("bind() failed");
if (debugFlag > 0) {
          printf("Failure on bind, errno:%d\n",errno);
}
    }
  
    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        cliAddrLen = sizeof(echoClntAddr);

        /* Block until receive message from a client */
        if ((recvMsgSize = recvfrom(sock, echoBuffer, ECHOMAX, 0,
            (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
        {
//$A0
//            DieWithError("recvfrom() failed");
if (debugFlag > 0) {
          printf("Failure on recvfrom, client: %s, errno:%d\n", inet_ntoa(echoClntAddr.sin_addr),errno);
}
        }
    

if (debugFlag > 0) {
        printf("Handling client %s:%d\n", inet_ntoa(echoClntAddr.sin_addr),ntohs(echoClntAddr.sin_port));
}

        matched = 0;
        int i;
        for(i=0;i<totalSessions;i++) {
            if(!strcmp(clients[i].ip_addr,inet_ntoa(echoClntAddr.sin_addr))) {
                 if(clients[i].port == ntohs(echoClntAddr.sin_port)) {
                     current_session = i;
                     matched = 1;
                     clients[current_session].last_time = time(NULL);
                     break; 
                 }
            }
        }
        if(!matched) {
            current_session = totalSessions;
            totalSessions++;
            strcpy(clients[current_session].ip_addr,inet_ntoa(echoClntAddr.sin_addr));
            clients[current_session].port = ntohs(echoClntAddr.sin_port);
            clients[current_session].start_time = time(NULL);
            clients[current_session].last_time = clients[current_session].start_time; 
            clients[current_session].recv_bytes = 0;
            clients[current_session].avg_thrput = 0;
            clients[current_session].avg_loss = 0;
        }

        clients[current_session].recv_bytes += recvMsgSize; 
        totalMsg += recvMsgSize;

        double r = rand()%100;
if (debugFlag > 0) {
        printf("Saved IP addres: %s\n", clients[current_session].ip_addr);
        printf("bytes for this session: %d\n",clients[current_session].recv_bytes);
        printf("Total bytes received by server: %d\n",totalMsg);
        printf("random number generated for loss: %f\n",r);
}
        if (r < avgLossRate*100) {
             continue;
        }

        /* Send received datagram back to the client */
        if (sendto(sock, echoBuffer, recvMsgSize, 0,  
             (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != recvMsgSize) {
//            DieWithError("sendto() sent a different number of bytes than expected");
          printf("Failure on sendTo, client: %s, errno:%d\n", inet_ntoa(echoClntAddr.sin_addr),errno);
        }
    }
    /* NOT REACHED */
}
