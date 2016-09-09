/*********************************************************
* Module Name: UDP Echo client source 
*
* File Name:    UDPEchoClient2.c
*
* Summary:
*  This file contains the echo Client code.
*
* Revisions:
*
* $A0: 6-15-2008: misc. improvements
*
*********************************************************/
#include "UDPEcho.h"
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>


void clientCNTCCode();

void CatchAlarm(int ignored);

int numberOfTimeOuts = 0;
int numberOfTrials;
long totalPing;
int bStop;

char Version[] = "1.1";

int main(int argc, char *argv[]) {
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned short echoServPort;     /* Echo server port */
    unsigned int fromSize;           /* In-out of address size for recvfrom() */
    char *servIP;                    /* IP address of server */
    char *echoString;                /* String to send to echo server */
    char echoBuffer[ECHOMAX + 1];      /* Buffer for receiving echoed string */
    int echoStringLen;               /* Length of string to echo */
    int respStringLen;               /* Length of received response */
    struct hostent *thehost;         /* Hostent from gethostbyname() */
    double delay;                     /* Iteration delay in seconds */
    int packetSize;                  /* PacketSize*/
    struct timeval *theTime1;
    struct timeval *theTime2;
    struct timeval TV1, TV2;
    int i;
    struct sigaction myaction;
    long usec1, usec2, curPing;
    short *messageSizePtr;
    short *sessionModePtr;
    long int *timestampPtr;
    int *seqNumberPtr;
    unsigned int seqNumber = 1;
    unsigned int RxSeqNumber = 1;
    struct timespec reqDelay, remDelay;
    int nIterations;
    long avgPing, loss;

    /* Command line parameters and defaults */
    char *serverIP; /* Server's IP address */
    unsigned short serverPort;  /* Server's port number */
    unsigned int averageRate = 1000000; /* Avg application sending rate in bits per second */
    int tokenSize = 4 * 1472;  /* size in bytes of the token bucket */
    int bucketSize = 1 * tokenSize;    /* Number of tokens available in bytes */
    short messageSize = 1472;    /* Amount of data to be put into each UDP datagram in bytes */
    int mode = 0;   /* one-way = 1 and RTT = 0 */
    unsigned int numberIterations = 0;  /* Number of times the client sends. 0 implies forever */
    int debugFlag = 0;  /* 0 means no printf messages sent to stdout besides what is required */
    /* ------------------------------------ */

    theTime1 = &TV1;
    theTime2 = &TV2;

    //Initialize values
    numberOfTimeOuts = 0;
    numberOfTrials = 0;
    totalPing = 0;
    bStop = 0;

    /* get info from parameters , or default to defaults if they're not specified */
    if (argc > 10 || argc < 3) {
        fprintf(stderr, "Usage: %s <Server IP> [<Server Port>] [<Average Rate>] [<Bucket Size>] [<Token Size>] "
                "[<Message Size>] [<Mode>] [<Number of Iterations>] [<Debug Flag>]\n", argv[0]);
        exit(1);
    }

    serverIP = argv[1];           /* First arg: server IP address (dotted quad) */
    serverPort = (unsigned short) atoi(argv[2]);       /* Second arg: server port */

    if (argc >= 4) averageRate = (unsigned int) atoi(argv[3]);
    if (argc >= 5) bucketSize = atoi(argv[4]);
    if (argc >= 6) tokenSize = atoi(argv[5]);
    if (argc >= 7) messageSize = (short) atoi(argv[6]);
    if (argc >= 8) mode = atoi(argv[7]);
    if (argc >= 9) numberIterations = (unsigned int) atoi(argv[8]);
    if (argc == 10) debugFlag = atoi(argv[9]);

    if (messageSize < 16) messageSize = 17;

    myaction.sa_handler = CatchAlarm;
    if (sigfillset(&myaction.sa_mask) < 0)
        DieWithError("sigfillset() failed");

    myaction.sa_flags = 0;

    if (sigaction(SIGALRM, &myaction, 0) < 0)
        DieWithError("sigaction failed for sigalarm");

    /* Set up the echo string */

    echoStringLen = messageSize;
    echoString = (char *) echoBuffer;

    for (i = 0; i < messageSize; i++) {
        echoString[i] = 0;
    }

    messageSizePtr = (short *) echoString;
    sessionModePtr = (short *) (echoString + 2);
    timestampPtr = (long *) (echoString + 4);
    seqNumberPtr = (int *) (echoString + 12);
    echoString[messageSize - 1] = '\0';


    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));    /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                 /* Internet addr family */
    echoServAddr.sin_addr.s_addr = inet_addr(serverIP);  /* Server IP address */

    /* If user gave a dotted decimal address, we need to resolve it  */
    if (echoServAddr.sin_addr.s_addr == -1) {
        thehost = gethostbyname(serverIP);
        echoServAddr.sin_addr.s_addr = (in_addr_t) *((unsigned long *) thehost->h_addr_list[0]);
    }

    echoServAddr.sin_port = htons(serverPort);     /* Server port */

    /* DEBUG PRINTS ------------------------- */
    int len = 20;
    char buffer[len];
    inet_ntop(AF_INET, &(echoServAddr.sin_addr), buffer, len);
    if (debugFlag) printf("serverIP:%s\n", buffer);
    if (debugFlag) printf("serverPort:%d\n", ntohs(echoServAddr.sin_port));
    /* -------------------------------------- */

    /* Create a datagram/UDP socket – Use same socket for every iteration */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");
    do {
        if ((tokenSize - messageSize) >= 0) {
            *messageSizePtr = htons(messageSize);
            *sessionModePtr = htons(mode);
            *timestampPtr = (unsigned) time(NULL);
            if (debugFlag) printf("%X\n", (unsigned) time(NULL));
            *seqNumberPtr = htonl(seqNumber++);

            /* Send the string to the server */
            int i;
            for (i = 0; i < messageSize; i++) {
                if (i > 0) printf(":");
                printf("%02X", (unsigned) (unsigned char) echoString[i]);
            }
            printf("\n");
            //printf("UDPEchoClient: Send the string: %X to the server: %s \n", echoString, serverIP);
            gettimeofday(theTime1, NULL);

            if (sendto(sock, echoString, (size_t) echoStringLen, 0, (struct sockaddr *)
                    &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
                DieWithError("sendto() sent a different number of bytes than expected");

            /* Recv a response */
            fromSize = sizeof(fromAddr);
            alarm(2);            //set the timeout for 2 seconds

            if ((respStringLen = (int) recvfrom(sock, echoBuffer, ECHOMAX, 0,
                                                (struct sockaddr *) &fromAddr, &fromSize)) != echoStringLen) {
                if (errno == EINTR) {
                    printf("Received a  Timeout !!!!!\n");
                    numberOfTimeOuts++;
                    continue;
                }
            }

            RxSeqNumber = ntohl(*(int *) echoBuffer);

            alarm(0);            //clear the timeout
            gettimeofday(theTime2, NULL);

            usec2 = (theTime2->tv_sec) * 1000000 + (theTime2->tv_usec);
            usec1 = (theTime1->tv_sec) * 1000000 + (theTime1->tv_usec);

            curPing = (usec2 - usec1);
            printf("Ping(%d): %ld microseconds\n", RxSeqNumber, curPing);

            totalPing += curPing;
            numberOfTrials++;

            reqDelay.tv_sec = delay;
            remDelay.tv_nsec = 0;
            nanosleep((const struct timespec *) &reqDelay, &remDelay);
            if (debugFlag) printf("numberIterations:%d\n", numberIterations);
            numberIterations--;

            /* Remove token from the bucket */
            tokenSize -= messageSize;
        }
    } while (numberIterations != 0 && bStop != 1);

    /* Close the UDP socket after all of the iterations have run */
    close(sock);

    if (numberOfTrials != 0)
        avgPing = (totalPing / numberOfTrials);
    else
        avgPing = 0;
    if (numberOfTimeOuts != 0)
        loss = ((numberOfTimeOuts * 100) / numberOfTrials);
    else
        loss = 0;

    printf("\nAvg Ping: %ld microseconds Loss: %ld Percent\n", avgPing, loss);

    exit(0);
}

void CatchAlarm(int ignored) { }

void clientCNTCCode() {
    long avgPing, loss;

    bStop = 1;
    if (numberOfTrials != 0)
        avgPing = (totalPing / numberOfTrials);
    else
        avgPing = 0;
    if (numberOfTimeOuts != 0)
        loss = ((numberOfTimeOuts * 100) / numberOfTrials);
    else
        loss = 0;

    printf("\nAvg Ping: %ld microseconds Loss: %ld Percent\n", avgPing, loss);
}
