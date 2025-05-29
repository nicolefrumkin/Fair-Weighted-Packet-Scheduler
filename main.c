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
        Packet *p = &packets[packetCount];
        savePacketParameters(line, &firstLine, &packetCount, packets);
        drainReadyPackets(p->time, connections, connectionCount);
        calculateFinishTime(p, connections, &connectionCount);
        int connIndex = findOrCreateConnection(*p, &connectionCount, connections);
        enqueue(&connections[connIndex].queue, p);
        packetCount++;
    }
    return 0;
}

void drainReadyPackets(int currentTime, Connection *connections, int connectionCount) {
    while (1) {
        Packet *next = NULL;
        int minConn = -1;
        for (int i = 0; i < connectionCount; i++) {
            Packet *candidate;
            if (!isEmpty(&connections[i].queue) && peek(&connections[i].queue, &candidate)) {
                if (!next || candidate->virtualFinishTime < next->virtualFinishTime ||
                    (candidate->virtualFinishTime == next->virtualFinishTime &&
                     connections[i].firstAppearIdx < connections[next->connectionID].firstAppearIdx)) {
                    next = candidate;
                    minConn = i;
                }
            }
        }

        if (!next || next->virtualFinishTime > currentTime) break;
        printPacketToFile(*next);
        Packet *removed;
        dequeue(&connections[minConn].queue, &removed);
    }
}

int findOrCreateConnection(Packet packet, int *connectionCount, Connection *connections) {
    for (int i = 0; i < *connectionCount; i++) {
        if (strcmp(connections[i].Sadd, packet.Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet.Dadd) == 0 &&
            connections[i].Sport == packet.Sport &&
            connections[i].Dport == packet.Dport) {
            connections[i].printWeight = (packet.weight == 1.0) ? 0 : 1;
            return i;
        }
    }

    int idx = (*connectionCount)++;
    strcpy(connections[idx].Sadd, packet.Sadd);
    strcpy(connections[idx].Dadd, packet.Dadd);
    connections[idx].Sport = packet.Sport;
    connections[idx].Dport = packet.Dport;
    connections[idx].weight = packet.weight;
    connections[idx].lastFinishTime = 0.0;
    connections[idx].firstAppearIdx = idx;
    initQueue(&connections[idx].queue);
    connections[idx].printWeight = (packet.weight == 1.0) ? 0 : 1;
    return idx;
}

int calculateFinishTime(Packet *packet, Connection *connections, int *connectionCount) {
    int connID = findOrCreateConnection(*packet, connectionCount, connections);
    Connection *conn = &connections[connID];

    if (packet->weight == 1 && conn->weight != 1)
        packet->weight = conn->weight;
    else
        conn->weight = packet->weight;

    double start = (conn->lastFinishTime > (double)packet->time) ? conn->lastFinishTime : (double)packet->time;
    packet->virtualFinishTime = start + (double)packet->packetLength / conn->weight;
    conn->lastFinishTime = packet->virtualFinishTime;
    packet->connectionID = connID;
    return conn->printWeight;
}

int comparePackets(Packet *packet1, Packet *packet2, Connection *connections) {
    if (packet1->virtualFinishTime < packet2->virtualFinishTime) return -1;
    if (packet1->virtualFinishTime > packet2->virtualFinishTime) return 1;
    int conn1 = connections[packet1->connectionID].firstAppearIdx;
    int conn2 = connections[packet2->connectionID].firstAppearIdx;
    return conn1 - conn2;
}

void printPacketToFile(Packet packet) {
    if (packet.weight == 1.0) {
        printf("%d: %d %s %d %s %d %d\n",
               packet.time,
               packet.time, packet.Sadd, packet.Sport,
               packet.Dadd, packet.Dport, packet.packetLength);
    } else {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               packet.time,
               packet.time, packet.Sadd, packet.Sport,
               packet.Dadd, packet.Dport, packet.packetLength, packet.weight);
    }
    fflush(stdout);
}


void savePacketParameters(char *line, int *firstLine, int *packetCount, Packet *packets) {
    Packet *packet = &packets[*packetCount];
    int parsed = sscanf(line, " %d %s %d %s %d %d %lf",
                        &packet->time, packet->Sadd, &packet->Sport,
                        packet->Dadd, &packet->Dport, &packet->packetLength, &packet->weight);
    if (*firstLine) {
        packet->time = 0;
        *firstLine = 0;
    }
    if (parsed == 6) packet->weight = 1.0;
    packet->id = *packetCount;
}
