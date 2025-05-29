#include "header.h"

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
        findOrCreateConnection(packets[packetCount], &connectionCount, connections);
        sortConnectionOrder(connections);
        printPacketToFile(packets[packetCount], connections, connectionCount);
        packetCount++;
    }
    return 0;
}


int findOrCreateConnection(Packet packet, int *connectionCount, Connection *connections)
{
    for (int i = 0; i < *connectionCount; i++)
    {
        if (strcmp(connections[i].Sadd, packet.Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet.Dadd) == 0 &&
            connections[i].Sport == packet.Sport &&
            connections[i].Dport == packet.Dport)
        { // Connection exists
            // used to check if we need to print the weight or not
            if (packet.weight == 1.0)
            {
                connections[i].printWeight = 0; // printWeight is 0 for packets with weight 1
            }
            else
            {
                connections[i].printWeight = 1; // printWeight is 1 for packets with explicit weight
            }
            enqueue(connections[i].queue, packet);
            return i;
        }
    }

    // New connection
    int idx = (*connectionCount)++;
    strcpy(connections[idx].Sadd, packet.Sadd);
    strcpy(connections[idx].Dadd, packet.Dadd);
    connections[idx].Sport = packet.Sport;
    connections[idx].Dport = packet.Dport;
    connections[idx].weight = packet.weight;
    connections[idx].lastFinishTime = 0.0;
    initPacketQueue(&connections[i]->queue);
    if (packet.weight == 1.0)
    {
        connections[idx].printWeight = 0; // printWeight is 0 for packets with weight 1
    }
    else
    {
        connections[idx].printWeight = 1; // printWeight is 1 for packets with explicit weight
    }
    return idx;
}

int calculateFinishTime(Packet *packet, Connection *connections, int *connectionCount)
{
    int connID = findOrCreateConnection(&packet, connectionCount, connections);
    Connection *conn = &connections[connID];

    if (packet->weight == 1 && conn->weight != 1)
        packet->weight = conn->weight;
    else
        conn->weight = packet->weight; // update with explicit value

    double start = conn->lastFinishTime > (double)packet->time ? conn->lastFinishTime : (double)packet->time;
    packet->virtualFinishTime = start + (double)packet->packetLength / conn->weight;
    conn->lastFinishTime = packet->virtualFinishTime;
    packet->connectionID = connID;

    return connections[connID].printWeight;
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

void printPacketToFile(Packet packet, Connection *connections, int *connectionCount)
{
    int printWeight = calculateFinishTime(&packet, connections, connectionCount);
    if (printWeight == 0)
    {
        printf("%d: %d %s %d %s %d %d\n",
               (int)packet.time,
               packet.time, packet.Sadd, packet.Sport,
               packet.Dadd, packet.Dport, packet.packetLength);
        fflush(stdout);
    }
    else
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               (int)packet.time,
               packet.time, packet.Sadd, packet.Sport,
               packet.Dadd, packet.Dport, packet.packetLength, packet.weight);

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
}

void sortConnectionOrder(Connection *connections)
{
}