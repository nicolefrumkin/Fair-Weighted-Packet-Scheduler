#include "header.h"

int main()
{
    Packet *packets = malloc(sizeof(Packet) * MAX_PACKETS);
    Connection *connections = malloc(sizeof(Connection) * MAX_CONNECTIONS);
    char line[MAX_LINE_LENGTH] = {0};
    int packetCount = 0;
    int connectionCount = 0;

    while (fgets(line, sizeof(line), stdin))
    {
        Packet *p = &packets[packetCount];
        savePacketParameters(line, p);
        int connIndex = findOrCreateConnection(p, &connectionCount, connections);
        p->connectionID = connIndex;
        if (p->hasWeight)
        {
            // Packet has explicit weight — update connection's weight
            connections[connIndex].weight = p->weight;
        }
        else
        {
            // Inherit weight from the connection
            p->weight = connections[connIndex].weight;
        }

        enqueue(&connections[connIndex].queue, p);
        packetCount++;
    }
    for (int i = 0; i < packetCount; i++)
    {
        calculateFinishTime(&packets[i], connections);
    }
    drainPackets(connections, connectionCount, packetCount);

    compareOutputWithExpected("out8_correct.txt");
    free(packets);
    free(connections);
    return 0;
}

void compareOutputWithExpected(const char *expectedFilePath)
{
    FILE *expectedFile = fopen(expectedFilePath, "r");
    FILE *actualFile = fopen("out8.txt", "r");
    FILE *mismatchesFile = fopen("mismatches.txt", "w");

    if (!expectedFile || !actualFile || !mismatchesFile)
    {
        fprintf(stderr, "Error opening files for comparison.\n");
        return;
    }

    char expectedLine[MAX_LINE_LENGTH];
    char actualLine[MAX_LINE_LENGTH];
    int lineNumber = 1;
    bool match = true;

    while (fgets(expectedLine, sizeof(expectedLine), expectedFile) &&
           fgets(actualLine, sizeof(actualLine), actualFile))
    {
        // Remove trailing newline characters
        expectedLine[strcspn(expectedLine, "\r\n")] = 0;
        actualLine[strcspn(actualLine, "\r\n")] = 0;

        if (strcmp(expectedLine, actualLine) != 0)
        {
            fprintf(mismatchesFile, "Mismatch at line %d:\nExpected: %s\nActual  : %s\n\n",
                    lineNumber, expectedLine, actualLine);
            match = false;
        }
        lineNumber++;
    }

    // Check if one file has extra lines
    if (fgets(expectedLine, sizeof(expectedLine), expectedFile) ||
        fgets(actualLine, sizeof(actualLine), actualFile))
    {
        fprintf(mismatchesFile, "File lengths differ after line %d.\n", lineNumber - 1);
        match = false;
    }

    if (match)
    {
        fprintf(mismatchesFile, "Output matches expected file.\n");
    }

    fclose(expectedFile);
    fclose(actualFile);
    fclose(mismatchesFile);
}

void drainPackets(Connection *connections, int connectionCount, int remaining)
{
    while (remaining > 0)
    {
        // find best available packet
        Packet *bestPacket = NULL;
        int bestConn = -1;
        // printf("-------------------------------------------------------------------------\n");
        for (int i = 0; i < connectionCount; i++)
        {
            if (!isEmpty(&connections[i].queue))
            {
                Packet *candidate;
                peek(&connections[i].queue, &candidate);
                // printf("Checking packet from connection %d: %s:%d -> %s:%d at time %d ",
                //        i, candidate->Sadd, candidate->Sport, candidate->Dadd, candidate->Dport, candidate->time);
                // printf("Virtual finish time: %.2f\n",
                //        candidate->virtualFinishTime);
                if (candidate->time <= globalOutputFinishTime)
                {
                    if (!bestPacket ||
                        candidate->time < bestPacket->time ||                  // prefer earliest-arriving eligible packet
                        (candidate->time == bestPacket->time && i < bestConn)) // tie-breaker: earlier connection

                    {
                        bestPacket = candidate;
                        bestConn = i;
                    }
                }
            }
        }
        // printf("-------------------------------------------------------------------------\n");
        //   if no packet was found, meaning all packets are in the future
        if (!bestPacket)
        {
            // Find next earliest arrival time across all queues
            int minArrival = INT_MAX;
            for (int i = 0; i < connectionCount; i++)
            {
                if (!isEmpty(&connections[i].queue))
                {
                    Packet *candidate;
                    peek(&connections[i].queue, &candidate);
                    if (candidate->time < minArrival)
                    {
                        minArrival = candidate->time;
                    }
                }
            }
            globalOutputFinishTime = minArrival;
            continue;
        }

        double start = fmax(bestPacket->time, globalOutputFinishTime);
        globalOutputFinishTime = start + bestPacket->packetLength;
        printPacketToFile(bestPacket, (int)start);

        Packet *removed;
        dequeue(&connections[bestConn].queue, &removed);
        remaining--;
    }
}

int findOrCreateConnection(Packet *packet, int *connectionCount, Connection *connections)
{

    for (int i = 0; i < *connectionCount; i++)
    {
        if (strcmp(connections[i].Sadd, packet->Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet->Dadd) == 0 &&
            connections[i].Sport == packet->Sport &&
            connections[i].Dport == packet->Dport)
        {
            // Found existing connection
            // If this packet has a weight specification and connection doesn't have one yet
            if (packet->weight != 1.0 && connections[i].weight == 1.0)
            {
                connections[i].weight = packet->weight;
            }
            // Set packet weight to connection weight
            // packet->weight = connections[i].weight;
            return i;
        }
    }

    // No existing connection found — create new one
    int idx = (*connectionCount)++;

    strcpy(connections[idx].Sadd, packet->Sadd);
    strcpy(connections[idx].Dadd, packet->Dadd);
    connections[idx].Sport = packet->Sport;
    connections[idx].Dport = packet->Dport;
    connections[idx].weight = packet->weight;
    initQueue(&connections[idx].queue);

    return idx;
}

void calculateFinishTime(Packet *packet, Connection *connections)
{
    int connID = packet->connectionID;
    Connection *conn = &connections[connID];
    double virtualStart = fmax(packet->time, globalOutputFinishTime);
    packet->virtualFinishTime = virtualStart + (double)packet->packetLength / packet->weight;
}

void printPacketToFile(Packet *packet, int actualStartTime)
{
    if (packet->hasWeight)
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               actualStartTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength, packet->weight);
    }
    else
    {
        printf("%d: %d %s %d %s %d %d\n",
               actualStartTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength);
    }
    fflush(stdout);
}

void savePacketParameters(char *line, Packet *packet)
{
    int parsed = sscanf(line, " %d %s %d %s %d %d %lf",
                        &packet->time, packet->Sadd, &packet->Sport,
                        packet->Dadd, &packet->Dport, &packet->packetLength, &packet->weight);
    if (parsed < 7)
    {
        packet->weight = 1.0;  // fallback only if weight wasn't given
        packet->hasWeight = 0; // no weight specified
    }
    else
    {
        packet->hasWeight = 1; // weight specified
    }
}

// Queue functions remain unchanged
void initQueue(Queue *q)
{
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

bool isEmpty(Queue *q)
{
    return q->size == 0;
}

bool isFull(Queue *q)
{
    return q->size == MAX_QUEUE_SIZE;
}

bool enqueue(Queue *q, Packet *packet)
{
    if (isFull(q))
        return false;

    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->items[q->rear] = packet;
    q->size++;
    return true;
}

bool dequeue(Queue *q, Packet **packet)
{
    if (isEmpty(q))
        return false;

    *packet = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return true;
}

bool peek(Queue *q, Packet **packet)
{
    if (isEmpty(q))
        return false;

    *packet = q->items[q->front];
    return true;
}