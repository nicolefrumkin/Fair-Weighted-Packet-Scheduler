#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_LINE_LENGTH 1024
#define MAX_SADD_LENGTH 20
#define MAX_PACKETS 100000
#define MAX_CONNECTIONS 10000

typedef struct Packet
{
    int id;
    int connectionID;
    int time;
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    int packetLength;
    double weight;
    double virtualFinishTime;
} Packet;

typedef struct Connection
{
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    double weight;
    double lastFinishTime;
    int firstAppearIdx;
    int flagweight;
    bool isActive;
} Connection;

int findOrCreateConnection(Packet *packet, int *connectionCount, Connection *connections)
{
    // printf("Im here3\n");
    // fflush(stdout);
    for (int i = 0; i < connectionCount; i++)
    {
        if (connections[i].isActive &&
            strcmp(connections[i].Sadd, packet->Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet->Dadd) == 0 &&
            connections[i].Sport == packet->Sport &&
            connections[i].Dport == packet->Dport)
        { // Connection exists
            if (packet->weight == 1.0)
            {
                connections[i].flagweight = 0; // flagweight is 0 for packets with weight 1
            }
            else
            {
                connections[i].flagweight = 1; // flagweight is 1 for packets with explicit weight
            }
            return i;
        }
    }

    // New connection
    int idx = connectionCount++;
    strcpy(connections[idx].Sadd, packet->Sadd);
    strcpy(connections[idx].Dadd, packet->Dadd);
    connections[idx].Sport = packet->Sport;
    connections[idx].Dport = packet->Dport;
    connections[idx].weight = packet->weight;
    connections[idx].lastFinishTime = 0.0;
    connections[idx].firstAppearIdx = idx;
    connections[idx].isActive = true;
    if (packet->weight == 1.0)
    {
        connections[idx].flagweight = 0; // flagweight is 0 for packets with weight 1
    }
    else
    {
        connections[idx].flagweight = 1; // flagweight is 1 for packets with explicit weight
    }
    return idx;
}

int processPacket(Packet *packet, Connection *connections)
{
    int connID = findOrCreateConnection(packet);
    Connection *conn = &connections[connID];

    if (packet->weight == 1 && conn->weight != 1)
        packet->weight = conn->weight;
    else
        conn->weight = packet->weight; // update with explicit value

    double start = conn->lastFinishTime > (double)packet->time ? conn->lastFinishTime : (double)packet->time;
    packet->virtualFinishTime = start + (double)packet->packetLength / conn->weight;
    conn->lastFinishTime = packet->virtualFinishTime;
    packet->connectionID = connID;

    return connections[connID].flagweight;
}

int comparePackets(Packet *packet1, Packet *packet2, Connection *connections)
{
    if (packet1->virtualFinishTime < packet2->virtualFinishTime)
        return -1;
    if (packet1->virtualFinishTime > packet2->virtualFinishTime)
        return 1;
    int conn1 = connections[packet1->connectionID].firstAppearIdx;
    int conn2 = connections[packet2->connectionID].firstAppearIdx;
    return conn1 - conn2;
}

void printPacketToFile(Packet *packet)
{
    int flagWeight = processPacket(packet);
    if (flagWeight == 0)
    {
        printf("%d: %d %s %d %s %d %d\n",
               (int)packet->time,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength);
        fflush(stdout);
    }
    else
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               (int)packet->time,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength, packet->weight);

        fflush(stdout);
    }
}

void savePacketParameters(char *line, int *firstLine, int *packetCount, Packet *packets)
{
    Packet *packet = &packets[*packetCount];
    int parsedElements = sscanf(line, " %d %s %d %s %d %d %lf",
                                 &packet->time, packet->Sadd, &packet->Sport,
                                 packet->Dadd, &packet->Dport, &packet->packetLength, &packet->weight);

    if (*firstLine)
    {
        packet->time = 0;
        *firstLine = 0;
    }

    if (parsedElements == 6)
        packet->weight = 1.0;

    packet->id = *packetCount;
    (*packetCount)++;
}

int main()
{
    Packet packets[MAX_PACKETS];
    Connection connections[MAX_CONNECTIONS];
    char line[MAX_LINE_LENGTH];
    int packetCount = 0;
    int connectionCount = 0;
    int firstLine = 1;

    // read packets from file
    while (fgets(line, sizeof(line), stdin))
    {
        savePacketParameters(line, firstLine, &packetCount, packets);

    }
    return 0;
}