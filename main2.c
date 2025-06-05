#include "header.h"
int globalcount = 0; // debug
bool dequeueByPointer(Queue *q, Packet *target)
{
    int newRear = -1;
    Packet *newItems[MAX_QUEUE_SIZE];
    int newSize = 0;
    bool found = false;

    for (int i = 0; i < q->size; i++)
    {
        int idx = (q->front + i) % MAX_QUEUE_SIZE;
        if (q->items[idx] != target)
        {
            newItems[++newRear] = q->items[idx];
            newSize++;
        }
        else
        {
            found = true;
        }
    }

    if (!found)
        return false;

    for (int i = 0; i <= newRear; i++)
        q->items[i] = newItems[i];

    q->front = 0;
    q->rear = newRear;
    q->size = newSize;
    return true;
}

double calculateWeight(Queue *transmittingNow)
{
    double totalWeight = 0;
    for (int i = 0; i < transmittingNow->size; i++)
    {
        int idx = (transmittingNow->front + i) % MAX_QUEUE_SIZE;
        totalWeight += transmittingNow->items[idx]->weight;
    }
    return totalWeight;
}

void addPacketsToQueue(int *nextToEnqueue, int packetCount, Packet *packets,
                       double currentTime, Queue *transmittingNow)
{
    while (*nextToEnqueue < packetCount && packets[*nextToEnqueue].time <= currentTime)
    {
        for (int i = 0; i < transmittingNow->size; i++)
        {
            if (transmittingNow->items[(transmittingNow->front + i) % MAX_QUEUE_SIZE]->connectionID ==
                packets[*nextToEnqueue].connectionID)
            {
                continue; // Skip if packet already in the queue for this connection
            }
        }
        enqueue(transmittingNow, &packets[*nextToEnqueue]);
        (*nextToEnqueue)++;
    }
}

Packet *findPacketFinishingFirst(Queue *transmittingNow, double totalWeight, double currentTime)
{
    Packet *finishingPacket = NULL;
    double minFinishTime = DBL_MAX;

    for (int i = 0; i < transmittingNow->size; i++)
    {
        int idx = (transmittingNow->front + i) % MAX_QUEUE_SIZE;
        Packet *p = transmittingNow->items[idx];
        double rate = (double)p->weight / totalWeight;
        double timeToFinish = p->bytesLeft / rate;
        double finishTime = currentTime + timeToFinish;

        if (finishTime < minFinishTime ||
            (fabs(finishTime - minFinishTime) < 1e-9 &&
             (finishingPacket == NULL || p->connectionID < finishingPacket->connectionID)))
        {
            minFinishTime = finishTime;
            finishingPacket = p;
        }
    }

    return finishingPacket;
}

void updateBytesTransferred(Queue *transmittingNow, double totalWeight, double currentTime, double nextTime)
{
    for (int i = 0; i < transmittingNow->size; i++)
    {
        int idx = (transmittingNow->front + i) % MAX_QUEUE_SIZE;
        Packet *p = transmittingNow->items[idx];
        double rate = p->weight / totalWeight;
        p->endTime = fmin(currentTime + (p->bytesLeft) * rate, nextTime);
        p->bytesLeft2 = p->bytesLeft;
        p->bytesLeft -= (nextTime - currentTime) * rate;
        // Handle floating point precision
        if (p->bytesLeft < 1e-9)
        {
            p->bytesLeft = 0;
        }
    }
}

Packet **GPS(int packetCount, Packet *packets)
{
    int nextToEnqueue = 0;
    Queue transmittingNow;
    initQueue(&transmittingNow);
    Packet **sortedPackets = malloc(sizeof(Packet *) * packetCount);
    int j = 0;

    double currentTime = 0.0;

    // Initialize bytesLeft for all packets
    for (int i = 0; i < packetCount; i++)
    {
        packets[i].bytesLeft = packets[i].packetLength;
    }

    while (j < packetCount)
    {
        // Add all packets that arrive at or before currentTime to queue
        addPacketsToQueue(&nextToEnqueue, packetCount, packets, currentTime, &transmittingNow);

        // If no packet is transmitting, jump to next packet arrival time
        if (isEmpty(&transmittingNow))
        {
            if (nextToEnqueue < packetCount)
            {
                currentTime = packets[nextToEnqueue].time;
            }
            continue;
        }

        double totalWeight = calculateWeight(&transmittingNow);
        if (totalWeight == 0)
            break;

        // Find which packet will finish first
        Packet *finishingPacket = findPacketFinishingFirst(&transmittingNow, totalWeight, currentTime);
        if (finishingPacket == NULL)
            break;

        // Find when the next packet arrives
        double nextArrivalTime = DBL_MAX;
        if (nextToEnqueue < packetCount)
        {
            nextArrivalTime = packets[nextToEnqueue].time;
        }

        // Determine next event time
        double packetFinishTime = finishingPacket->endTime;
        // double nextTime2 = (packetFinishTime < nextArrivalTime) ? packetFinishTime : nextArrivalTime;
        double nextTime = nextArrivalTime;
        double timeElapsed = nextTime - currentTime;
        // Update bytes for all packets

        updateBytesTransferred(&transmittingNow, totalWeight, currentTime, nextTime);
        // Debug: Print current queue
        if (currentTime < 43000 && currentTime > 30000)
        {
            fprintf(stderr, "\nTime %-4.0f:\n", currentTime, transmittingNow.size);
            for (int i = 0; i < transmittingNow.size; i++)
            {
                int idx = (transmittingNow.front + i) % MAX_QUEUE_SIZE;
                fprintf(stderr, "Queue[%d]: arrival=%-4d, length=%-3d, bytesLeft=%-4.2f, weight=%-3.2f, rate=%-3.2f,endTime=%-7.2f\n",
                        i, transmittingNow.items[idx]->time,
                        transmittingNow.items[idx]->packetLength,
                        transmittingNow.items[idx]->bytesLeft2,
                        transmittingNow.items[idx]->weight,
                        transmittingNow.items[idx]->weight / totalWeight,
                        transmittingNow.items[idx]->endTime);
            }
        }
        currentTime = nextTime;
        double minimalTime = DBL_MAX;
        Packet *minimalPacket = NULL;

        // If we reached the packet finish time, complete and re
        // Check for finished packets AFTER updating all bytes
        for (int i = 0; i < transmittingNow.size; i++) // Iterate backwards to safely remove
        {
            int idx = (transmittingNow.front + i) % MAX_QUEUE_SIZE;
            Packet *p = transmittingNow.items[idx];
            if (p->endTime < minimalTime && !p->printed)
            {
                minimalTime = p->endTime;
                minimalPacket = p;
            }
        }
        sortedPackets[j++] = minimalPacket;

        // Check for finished packets AFTER updating all bytes
        for (int i = transmittingNow.size - 1; i >= 0; i--) // Iterate backwards to safely remove
        {
            int idx = (transmittingNow.front + i) % MAX_QUEUE_SIZE;
            Packet *p = transmittingNow.items[idx];
            if (p->bytesLeft == 0)
            {
                p->endTime = nextTime;
                p->printed = 1;
                dequeueByPointer(&transmittingNow, p);
            }
        }
    }

    return sortedPackets;
}

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
        int connIndex = findOrCreateConnection(p, &connectionCount, connections, packetCount);
        p->connectionID = connIndex;
        if (p->hasWeight)
        {
            connections[connIndex].weight = p->weight; // Set connection weight if specified
        }
        else
        {
            p->weight = connections[connIndex].weight; // Inherit weight from the connection
        }
        enqueue(&connections[connIndex].queue, p);
        packetCount++;
    }
    Packet **sortedPackets = GPS(packetCount, packets);
    int currentTime2 = 0;
    // fprintf(stderr, "-----------------------------------------------------------------------------\n");
    // fprintf(stderr, "\n\nsent     arrived     s_IP        s_port     d_IP        d_port len  weight end_time\n");

    for (int i = 0; i < packetCount; i++)
    {
        printPacketToFile(sortedPackets[i], currentTime2);
        currentTime2 = fmax(currentTime2 + sortedPackets[i]->packetLength,
                            (i + 1 < packetCount ? sortedPackets[i + 1]->time : 0));
    }
    // drainPackets(connections, connectionCount, packetCount);
    // fprintf(stderr, "-----------------------------------------------------------------------------\n");

    compareOutputWithExpected("out8_correct.txt");
    free(packets);
    free(connections);
    free(sortedPackets);
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
    int count = 0;
    fprintf(stderr, "\n-----------------------------------------------------------------------------\n\n");

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
            // If this packet has a weight specification and connection doesn't have one yet
            if (packet->hasWeight)
            {
                connections[i].weight = packet->weight;
            }
            else
            {
                packet->weight = connections[i].weight; // Inherit weight from the connection
            }
            // Set packet weight to connection weight
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

void printPacketToFile(Packet *packet, int actualStartTime)
{
    if (packet->hasWeight)
    {
        printf("%d: %d %s %d %s %d %d %.2f\n",
               actualStartTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength, packet->weight);
        // if (packet->time <= 46714)
        // {
        //     fprintf(stderr, "%-7d: %-7d %-15s %-6d %-15s %-6d %-3d  %-3.2f   %-3.2f\n",
        //             actualStartTime,
        //             packet->time, packet->Sadd, packet->Sport,
        //             packet->Dadd, packet->Dport, packet->packetLength, packet->weight, packet->endTime);
        //     globalcount++;
        // }
    }
    else
    {
        printf("%d: %d %s %d %s %d %d\n",
               actualStartTime,
               packet->time, packet->Sadd, packet->Sport,
               packet->Dadd, packet->Dport, packet->packetLength);
        // if (packet->time <= 46714)
        // {
        //     fprintf(stderr, "%-7d: %-7d %-15s %-6d %-15s %-6d %-3d  %-3.2f   %-3.2f\n",
        //             actualStartTime,
        //             packet->time, packet->Sadd, packet->Sport,
        //             packet->Dadd, packet->Dport, packet->packetLength, packet->weight, packet->endTime);
        //     globalcount++;
        // }
    }
    fflush(stdout);
}

void savePacketParameters(char *line, Packet *packet)
{
    packet->weight = 1.0;
    int parsed = sscanf(line, "%d %s %d %s %d %d %lf",
                        &packet->time, packet->Sadd, &packet->Sport,
                        packet->Dadd, &packet->Dport, &packet->packetLength, &packet->weight);
    packet->bytesLeft = packet->packetLength; // Initialize bytes left to total length
    if (parsed < 7)
    {
        packet->weight = 1.0;  // fallback only if weight wasn't given
        packet->hasWeight = 0; // no weight specified
    }
    else
    {
        packet->hasWeight = 1; // weight specified
    }
    packet->virtualFinishTime = 0.0;
    packet->printed = 0;
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