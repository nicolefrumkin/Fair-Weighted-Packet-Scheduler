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

double globalFinishTime = 0.0;

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
    int hasWeight;
    double endTime;
} Packet;

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
    double virtualFinishTime;
    Queue queue;
} Connection;

// Function declarations
int findOrCreateConnection(Packet *packet, int *connectionCount, Connection *connections, int packetCount);
void printPacketToFile(Packet *packet, int actualStartTime);
void savePacketParameters(char *line, Packet *packet);
void drainPackets(Connection *connections, int connectionCount, int remaining);

// Queue functions
void initQueue(Queue *q);
bool isEmpty(Queue *q);
bool isFull(Queue *q);
bool enqueue(Queue *q, Packet *packet);
bool dequeue(Queue *q, Packet **packet);
bool peek(Queue *q, Packet **packet);

#endif
