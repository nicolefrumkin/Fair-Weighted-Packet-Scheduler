#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.c"

#define MAX_LINE_LENGTH 1024
#define MAX_SADD_LENGTH 20
#define MAX_PACKETS 100000
#define MAX_CONNECTIONS 10000
#define MAX_QUEUE_SIZE 1000

typedef struct {
    Packet *items[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} Queue;

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
    struct Packet *next;
} Packet;

typedef struct Connection
{
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    double weight;
    double lastFinishTime;
    int printWeight;
    Queue queue;
} Connection;

int findOrCreateConnection(Packet packet, int *connectionCount, Connection *connections);
int calculateFinishTime(Packet *packet, Connection *connections, int *connectionCount);
int comparePackets(Packet *packet1, Packet *packet2, Connection *connections);
void printPacketToFile(Packet packet, Connection *connections, int *connectionCount);
void savePacketParameters(char *line, int *firstLine, int *packetCount, Packet *packets);
void sortConnectionOrder(Connection *connections);
void initQueue(Queue *q);
int enqueue(Queue *q, Packet *pkt);
int dequeue(Queue *q, Packet **pkt);
int isQueueEmpty(Queue *q);
int isQueueFull(Queue *q);