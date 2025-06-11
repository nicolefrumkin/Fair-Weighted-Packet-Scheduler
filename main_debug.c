#include "header2.h"

double prevGlobalFinishTime = 0.0;

int main()
{
    Packet *packets = malloc(sizeof(Packet) * MAX_PACKETS);
    Connection *connections = malloc(sizeof(Connection) * MAX_CONNECTIONS);
    char line[MAX_LINE_LENGTH] = {0};
    int packetCount = 0;
    int connectionCount = 0;
    fprintf(stderr, "\nPreviously Scheduled Packets:\nsent     arrived     s_IP        s_port     d_IP        d_port len  weight\n");
    while (fgets(line, sizeof(line), stdin))
    {
        Packet *p = &packets[packetCount];
        savePacketParameters(line, p);
        int connIndex = findOrCreateConnection(p, &connectionCount, connections, packetCount);
        p->connectionID = connIndex;
        enqueue(&connections[connIndex].queue, p);
        packetCount++;
    }
    drainPackets(connections, connectionCount, packetCount);

    const char *expected = NULL;
    const char *actual = NULL;
    if (packetCount > 0) // debug
    {
        if (strcmp(packets[0].Sadd, "70.246.64.70") == 0 &&
            strcmp(packets[0].Dadd, "4.71.70.4") == 0 &&
            packets[0].Sport == 14770 &&
            packets[0].Dport == 11970)
        {
            expected = "out8_correct.txt";
            actual = "out8.txt";
        }
        else if (strcmp(packets[0].Sadd, "104.248.96.104") == 0 &&
                 strcmp(packets[0].Dadd, "32.109.104.40") == 0 &&
                 packets[0].Sport == 64440 &&
                 packets[0].Dport == 39800)
        {
            expected = "out9+_correct.txt";
            actual = "out9+.txt";
        }
    }

    if (expected && actual) // debug
    {
        compareOutputWithExpected(expected, actual);
    }
    else
    {
        fprintf(stderr, "No comparison made (unknown input file).\n");
    }
    free(packets);
    free(connections);
    return 0;
}

void drainPackets(Connection *connections, int connectionCount, int remaining)
{
    double conVirtualTime = 0.0;
    double packetSentTime = 0.0;
    int packetNum = 1;

    while (packetNum <= remaining)
    {
        Packet *minPacket = NULL;
        int minConnId = -1;
        double activeLinksWeight = 0.0;

        // sum weights of all active connections
        if (packetNum == 208)
        {
            fprintf(stderr, "\n*********************************************************************\n\n");

            fprintf(stderr, "active connections indices: "); // debug}
        }
        for (int i = 0; i < connectionCount; i++)
        {
            Packet *candidate;
            conVirtualTime = connections[i].virtualFinishTime;

            if (peek(&connections[i].queue, &candidate))
            {
                if (candidate->hasWeight)
                {
                    connections[i].weight = candidate->weight;
                }
                else
                {
                    candidate->weight = connections[i].weight; // Inherit weight from the connection
                }
                candidate->virtualFinishTime = fmax(prevGlobalFinishTime, conVirtualTime) + candidate->packetLength / candidate->weight;
                if (conVirtualTime > globalFinishTime)
                {
                    activeLinksWeight += connections[i].weight;
                    if (packetNum == 208)
                        fprintf(stderr, "%d ", i); // debug
                }
            }
        }
        if (activeLinksWeight == 0.0)
        {
            activeLinksWeight = 1.0;
        }
        if (packetNum == 208) // debug
        {
            if (activeLinksWeight == 0.0) fprintf(stderr, "none");
            fprintf(stderr, "\nactiveLinksWeight: %.2f\n\nPackets Available to Transmit:\nconn    arrival  len   weight     VFT      con finish time\n", activeLinksWeight);
        }

        // Find the best available packet (eligible and lowest virtual finish time)
        for (int i = 0; i < connectionCount; i++)
        {
            Packet *candidate;
            conVirtualTime = connections[i].virtualFinishTime;

            if (peek(&connections[i].queue, &candidate))
            {
                if (packetNum == 208 && candidate->time <= globalFinishTime)
                {
                    fprintf(stderr, " %-2d    %7d    %-4d  %3.2f   %7.2f    %.2f",
                            i, candidate->time, candidate->packetLength, candidate->weight, candidate->virtualFinishTime, conVirtualTime);
                }
                // take packet only if it arrived before the current time and its connection is available
                if ((candidate->time <= globalFinishTime) && (conVirtualTime <= globalFinishTime))
                {
                    if (!minPacket || (candidate->virtualFinishTime < minPacket->virtualFinishTime) || (fabs(candidate->virtualFinishTime - minPacket->virtualFinishTime) < 1e-9 && i < minConnId))
                    {
                        if (packetNum == 208)
                            fprintf(stderr, "      <- curr min"); // debug
                        minPacket = candidate;
                        minConnId = i;
                    }
                }
                if (packetNum == 208 && candidate->time <= globalFinishTime)
                    fprintf(stderr, "\n"); // debug
            }
        }

        if (packetNum == 208) // debug
        {
            fprintf(stderr, "\n\n*********************************************************************\n\n");
        }
        // Advance to the next earliest arrival if no packet is eligible now
        if (!minPacket)
        {
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
            globalFinishTime = minArrival;
            continue;
        }
        packetSentTime = fmax(minPacket->time, globalFinishTime);
        prevGlobalFinishTime = globalFinishTime;
        globalFinishTime = packetSentTime + minPacket->packetLength / (activeLinksWeight);
        connections[minConnId].virtualFinishTime = globalFinishTime;
        printPacketToFile(minPacket, (int)packetSentTime);
        Packet *removed;
        dequeue(&connections[minConnId].queue, &removed);
        packetNum++;
    }
}

int findOrCreateConnection(Packet *packet, int *connectionCount, Connection *connections, int packetCount)
{

    for (int i = 0; i < *connectionCount; i++)
    {
        if (strcmp(connections[i].Sadd, packet->Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet->Dadd) == 0 &&
            connections[i].Sport == packet->Sport &&
            connections[i].Dport == packet->Dport)
        {
            // Found existing connection
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
    connections[idx].virtualFinishTime = 0.0;
    initQueue(&connections[idx].queue);

    return idx;
}

void printPacketToFile(Packet *packet, int packetSentTime)
{
    if (packet->hasWeight)
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               packetSentTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength, packet->weight);
        // if (packetSentTime > 309558 && packetSentTime < 312351) // debug
        // {
        //     fprintf(stderr, "%-7d: %-7d %-15s %-6d %-15s %-6d %-3d %-3.2f\n",
        //             packetSentTime,
        //             packet->time, packet->Sadd, packet->Sport,
        //             packet->Dadd, packet->Dport, packet->packetLength, packet->weight);
        // }
    }
    else
    {
        printf("%d: %d %s %d %s %d %d\n",
               packetSentTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength);
        // if (packetSentTime > 309558 && packetSentTime < 312351) // debug
        // {
        //     fprintf(stderr, "%-7d: %-7d %-15s %-6d %-15s %-6d %-7d %d\n",
        //             packetSentTime,
        //             packet->time, packet->Sadd, packet->Sport,
        //             packet->Dadd, packet->Dport, packet->packetLength, 1);
        // }
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
        packet->weight = 1.0; // fallback only if weight wasn't given
        packet->hasWeight = 0;
    }
    else
    {
        packet->hasWeight = 1; // weight specified
    }
    packet->virtualFinishTime = 0.0;
}

// Queue functions
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

// debug function
void compareOutputWithExpected(const char *expectedFilePath, const char *actualFilePath)
{
    FILE *expectedFile = fopen(expectedFilePath, "r");
    FILE *actualFile = fopen(actualFilePath, "r");
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
    int count = 0;
    // fprintf(stderr, "-----------------------------------------------------------------------------\n\n");

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
            if (count < 5)
            {
                fprintf(stderr, "Mismatch at line %d:\nExpected: %s\nActual  : %s\n\n",
                        lineNumber, expectedLine, actualLine);
                count++;
            }
            match = false;
        }
        lineNumber++;
    }

    // Check if one file has extra lines
    if (fgets(expectedLine, sizeof(expectedLine), expectedFile) ||
        fgets(actualLine, sizeof(actualLine), actualFile))
    {
        fprintf(mismatchesFile, "File lengths differ after line %d.\n", lineNumber - 1);
        fprintf(stderr, "File lengths differ after line %d.\n", lineNumber - 1);
        match = false;
    }

    if (match)
    {
        fprintf(mismatchesFile, "Output matches expected file.\n");
        fprintf(stderr, "Output matches expected file.\n");
    }
    fprintf(stderr, "-----------------------------------------------------------------------------\n");

    fclose(expectedFile);
    fclose(actualFile);
    fclose(mismatchesFile);
}