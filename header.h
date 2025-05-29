#ifndef HEADER_H
#define HEADER_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#define MAX_LINE_LENGTH 1024
#define MAX_SADD_LENGTH 20
#define MAX_PACKETS 100000
#define MAX_CONNECTIONS 10000
#define MAX_QUEUE_SIZE 1000

static double globalOutputFinishTime = 0.0;

// Define Packet FIRST
typedef struct Packet
{
    int time;
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    int packetLength;
    double weight;
    double virtualFinishTime;
    int connectionID;
    int id;
} Packet;

// Now Queue can use Packet*
typedef struct
{
    Packet *items[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} Queue;

typedef struct Connection
{
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    double weight;
    double lastFinishTime;
    int printWeight;
    int firstAppearIdx;
    double lastRealFinishTime;
    double virtualFinishTime;
    Queue queue;
} Connection;

// Function declarations
int findOrCreateConnection(Packet *packet, int *connectionCount, Connection *connections);
int calculateFinishTime(Packet *packet, Connection *connections);
void printPacketToFile(Packet *packet, int actualStartTime);
void savePacketParameters(char *line, Packet *packet);
void drainReadyPackets(int currentTime, Connection *connections, int connectionCount);
void compareOutputWithExpected(const char *expectedFilePath);

// Queue functions
void initQueue(Queue *q);
bool isEmpty(Queue *q);
bool isFull(Queue *q);
bool enqueue(Queue *q, Packet *packet);
bool dequeue(Queue *q, Packet **packet);
bool peek(Queue *q, Packet **packet);

#endif
