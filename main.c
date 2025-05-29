#include "header.h"

int main()
{
    Packet *packets = malloc(sizeof(Packet) * MAX_PACKETS);
    Connection *connections = malloc(sizeof(Connection) * MAX_CONNECTIONS);
    char line[MAX_LINE_LENGTH] = {0};
    int packetCount = 0;
    int connectionCount = 0;

    // read packets from file
    while (fgets(line, sizeof(line), stdin))
    {
        Packet *p = &packets[packetCount];
        savePacketParameters(line, p);
        int connIndex = findOrCreateConnection(p, &connectionCount, connections);
        p->connectionID = connIndex;
        calculateFinishTime(p, connections);
        enqueue(&connections[connIndex].queue, p);
        drainReadyPackets(p->time, connections, connectionCount);
        packetCount++;
    }
    // Drain ALL remaining packets at the end
    drainReadyPackets(INT_MAX, connections, connectionCount);
    compareOutputWithExpected("out8.txt");
    free(packets);
    free(connections);
    return 0;
}

void compareOutputWithExpected(const char *expectedFilePath)
{
    FILE *expectedFile = fopen(expectedFilePath, "r");
    FILE *actualFile = fopen("actual_output.txt", "r");
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
            fprintf(mismatchesFile,"Mismatch at line %d:\nExpected: %s\nActual  : %s\n\n",
                   lineNumber, expectedLine, actualLine);
            match = false;
        }
        lineNumber++;
    }

    // Check if one file has extra lines
    if (fgets(expectedLine, sizeof(expectedLine), expectedFile) ||
        fgets(actualLine, sizeof(actualLine), actualFile))
    {
        fprintf(mismatchesFile,"File lengths differ after line %d.\n", lineNumber - 1);
        match = false;
    }

    if (match)
    {
        fprintf(mismatchesFile,"Output matches expected file.\n");
    }

    fclose(expectedFile);
    fclose(actualFile);
    fclose(mismatchesFile);
}


void drainReadyPackets(int currentTime, Connection *connections, int connectionCount)
{
    while (1)
    {
        Packet *next = NULL;
        int minConn = -1;
        double minStartTime = 0;

        // Find the packet with minimum virtual finish time across all connections
        for (int i = 0; i < connectionCount; i++)
        {
            Packet *candidate;
            if (!isEmpty(&connections[i].queue) && peek(&connections[i].queue, &candidate))
            {
                // Calculate when this packet can start transmission
                double candidateStart = fmax(connections[i].lastFinishTime, (double)candidate->time);

                if (!next || candidate->virtualFinishTime < next->virtualFinishTime ||
                    (candidate->virtualFinishTime == next->virtualFinishTime &&
                     connections[i].firstAppearIdx < connections[next->connectionID].firstAppearIdx))
                {
                    next = candidate;
                    minConn = i;
                    minStartTime = candidateStart;
                }
            }
        }

        // Stop if no packets or packet can't start yet
        if (!next || minStartTime > currentTime)
            break;

        printPacketToFile(next);
        Packet *removed;
        dequeue(&connections[minConn].queue, &removed);
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
            // Update connection weight if packet has a non-default weight and connection is still default
            if (packet->weight != 1.0 && connections[i].weight == 1.0)
            {
                connections[i].weight = packet->weight;

                // Recalculate virtual finish times for all packets in this connection's queue
                double lastFinish = 0.0;
                for (int j = 0; j < connections[i].queue.size; j++)
                {
                    int index = (connections[i].queue.front + j) % MAX_QUEUE_SIZE;
                    Packet *qpacket = connections[i].queue.items[index];

                    double start = fmax(lastFinish, (double)qpacket->time);
                    qpacket->weight = connections[i].weight; // Update the weight of the queued packet
                    qpacket->virtualFinishTime = start + (double)qpacket->packetLength / qpacket->weight;
                    lastFinish = qpacket->virtualFinishTime;
                }

                connections[i].lastFinishTime = lastFinish;
            }

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
    connections[idx].lastFinishTime = 0.0;
    connections[idx].firstAppearIdx = idx;
    initQueue(&connections[idx].queue);

    return idx;
}

int calculateFinishTime(Packet *packet, Connection *connections)
{
    int connID = packet->connectionID;
    Connection *conn = &connections[connID];

    // Use connection's weight if packet has default weight
    if (packet->weight == 1.0 && conn->weight != 1.0)
    {
        packet->weight = conn->weight;
    }

    // Calculate start time: max(arrival_time, last_finish_time_of_connection)
    double start = (conn->lastFinishTime > (double)packet->time)
                       ? conn->lastFinishTime
                       : (double)packet->time;

    // Virtual finish time = start + length/weight
    packet->virtualFinishTime = start + (double)packet->packetLength / packet->weight;

    // Update connection's last finish time
    conn->lastFinishTime = packet->virtualFinishTime;

    return 0;
}

void printPacketToFile(Packet *packet)
{
    // Calculate actual start time for output
    double start = packet->virtualFinishTime - (double)packet->packetLength / packet->weight;
    int startTime = (int)start;

    if (packet->weight == 1.0)
    {
        printf("%d: %d %s %d %s %d %d\n",
               startTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength);
    }
    else
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               startTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength, packet->weight);
    }
    fflush(stdout);
}

void savePacketParameters(char *line, Packet *packet)
{
    packet->weight = 1.0; // Set default weight
    sscanf(line, " %d %s %d %s %d %d %lf",
           &packet->time, packet->Sadd, &packet->Sport,
           packet->Dadd, &packet->Dport, &packet->packetLength, &packet->weight);
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