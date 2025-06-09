#include "header.h"
int globalcount = 0;  // debug
double currentTime2 = 0; // debug
int prevConn = -1;
int prevPrinted = 1;

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
        bool alreadyInQueue = false;
        for (int i = 0; i < transmittingNow->size; i++)
        {
            int idx = (transmittingNow->front + i) % MAX_QUEUE_SIZE;
            if (transmittingNow->items[idx]->connectionID == packets[*nextToEnqueue].connectionID)
            {
                alreadyInQueue = true;
                break;
            }
        }

        if (!alreadyInQueue)
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

void updateBytesTransferred(Queue *transmittingNow, double totalWeight, double currentTime, double nextTime, Connection *connections)
{
    for (int i = 0; i < transmittingNow->size; i++)
    {
        int idx = (transmittingNow->front + i) % MAX_QUEUE_SIZE;
        Packet *p = transmittingNow->items[idx];
        double rate = p->weight / totalWeight;
        p->endTime = fmin(currentTime + (p->bytesLeft) * rate, nextTime);
        connections[p->connectionID].virtualFinishTime = currentTime2 + p->packetLength;
        p->bytesLeft2 = p->bytesLeft;
        p->bytesLeft -= (nextTime - currentTime) * rate;
        // Handle floating point precision
        if (p->bytesLeft < 1e-9)
        {
            p->bytesLeft = 0;
        }
    }
}

Packet **GPS(int packetCount, Packet *packets, Connection *connections)
{
    int nextToEnqueue = 0;
    Queue transmittingNow;
    initQueue(&transmittingNow);
    Packet **sortedPackets = malloc(sizeof(Packet *) * packetCount);
    int j = 0;
    double nextTime = 0.0;
    double currentTime = 0.0;

    // Initialize bytesLeft for all packets
    for (int i = 0; i < packetCount; i++)
    {
        packets[i].bytesLeft = packets[i].packetLength;
    }

    while (j < packetCount)
    {
        // Add all packets that arrive at or before currentTime to queue
        addPacketsToQueue(&nextToEnqueue, packetCount, packets, currentTime2, &transmittingNow);

        // If no packet is transmitting, jump to next packet arrival time
        if (isEmpty(&transmittingNow))
        {   prevPrinted = 0;
            if (nextToEnqueue < packetCount)
            {
                currentTime = packets[nextToEnqueue].time;
            }
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
        if (nextToEnqueue < packetCount || prevPrinted == 1)
        {
            nextArrivalTime = packets[nextToEnqueue].time;
        }

        // Determine next event time
        double packetFinishTime = finishingPacket->endTime;
        // double nextTime2 = (packetFinishTime < nextArrivalTime) ? packetFinishTime : nextArrivalTime;
        if (prevPrinted) {
            nextTime = nextArrivalTime;
        }
        else {
            nextTime = packetFinishTime;
        }
        if (nextTime < 48000) fprintf(stderr, "j = %d, nextTime = %.2f\n", j+1,nextTime);
        double timeElapsed = nextTime - currentTime;
        // Update bytes for all packets

        updateBytesTransferred(&transmittingNow, totalWeight, currentTime, nextTime, connections);
        // Debug: Print current queue
        if (j > 33 && j < 38)
        {
            fprintf(stderr, "\nTime %-4.0f, j = %d, queue size = %d:\n", currentTime, j + 1, transmittingNow.size);
            for (int i = 0; i < transmittingNow.size; i++)
            {
                int idx = (transmittingNow.front + i) % MAX_QUEUE_SIZE;
                int ID = transmittingNow.items[idx]->connectionID;

                fprintf(stderr, "Queue[%d]: arrival=%-4d, length=%-3d, bytesLeft=%-4.2f, weight=%-3.2f, rate=%-3.2f,endTime=%-7.2f\n",
                        i, transmittingNow.items[idx]->time,
                        transmittingNow.items[idx]->packetLength,
                        transmittingNow.items[idx]->bytesLeft2,
                        transmittingNow.items[idx]->weight,
                        transmittingNow.items[idx]->weight / totalWeight,
                        transmittingNow.items[idx]->endTime,
                        transmittingNow.items[idx]->printed);
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
            if (p->endTime < minimalTime)
            {
                minimalTime = p->endTime;
                minimalPacket = p;
            }
        }
        // After updateBytesTransferred and before printing
        double t = fmax(currentTime2 + minimalPacket->packetLength, nextTime);
        if (j > 33 && j < 38)
            fprintf(stderr, "t = %.2f, packetLength = %.2f, nextTime = %.2f, prevPrinted = %d\n",
                    t, currentTime2+minimalPacket->packetLength, nextTime, prevPrinted);
        if ((!minimalPacket->printed || transmittingNow.size == 1))
        {
            // If the packet is not printed yet and its connection's virtual finish time is less than or equal to t
            // and it is the only packet in the queue, print it
            minimalPacket->endTime = t; // Set end time to t
            minimalPacket->printed = 1; // Mark as printed
            sortedPackets[j] = minimalPacket;
            printPacketToFile(sortedPackets[j], currentTime2);
            currentTime2 = fmax(currentTime2 + minimalPacket->packetLength, nextTime);
            j++;
            prevPrinted = 1;
        }

        else if (!minimalPacket->printed && (connections[minimalPacket->connectionID].virtualFinishTime - t) <=1e-6)
        {
            minimalPacket->endTime = t; // Set end time to t
            minimalPacket->printed = 1;
            sortedPackets[j] = minimalPacket;
            connections[minimalPacket->connectionID].virtualFinishTime = currentTime2 + minimalPacket->packetLength;
            printPacketToFile(sortedPackets[j], currentTime2);
            prevPrinted = 1;
            currentTime2 = fmax(currentTime2 + minimalPacket->packetLength, nextTime);
            j++;
        }
        else {
            prevPrinted = 0; 
        }

        // Check for finished packets AFTER updating all bytes
        for (int i = transmittingNow.size - 1; i >= 0; i--) // Iterate backwards to safely remove
        {
            int idx = (transmittingNow.front + i) % MAX_QUEUE_SIZE;
            Packet *p = transmittingNow.items[idx];
            if (p->bytesLeft == 0)
            {
                p->endTime = nextTime;
                dequeueByPointer(&transmittingNow, p);
            }
        }
        // fprintf(stderr, "queue size = %d\n", transmittingNow.size);
        // Drain remaining packets in the queue
        while (!isEmpty(&transmittingNow))
        {
            Packet *p;
            dequeue(&transmittingNow, &p);
            if (!p->printed)
            {
                p->printed = 1;
                sortedPackets[j] = p;
                printPacketToFile(sortedPackets[j], currentTime2);
                currentTime2 = fmax(currentTime2 + sortedPackets[j]->packetLength, nextTime);
                j++;
                prevPrinted = 1;
            }
        }
    }

    return sortedPackets;
}

int main(int argc, char *argv[])
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
    Packet **sortedPackets = GPS(packetCount, packets, connections);
    // fprintf(stderr, "-----------------------------------------------------------------------------\n");
    // fprintf(stderr, "\n\nsent     arrived     s_IP        s_port     d_IP        d_port len  weight end_time\n");

    // for (int i = 0; i < packetCount; i++)
    // {
    //     printPacketToFile(sortedPackets[i], currentTime2);
    //     currentTime2 = fmax(currentTime2 + sortedPackets[i]->packetLength,
    //                         (i + 1 < packetCount ? sortedPackets[i + 1]->time : 0));
    // }

    // drainPackets(connections, connectionCount, packetCount);
    // fprintf(stderr, "-----------------------------------------------------------------------------\n");
    // Detect input file based on first packet timestamp
    const char *expected = NULL;
    const char *actual = NULL;

    if (packetCount > 0)
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

    if (expected && actual)
    {
        compareOutputWithExpected(expected, actual);
    }
    else
    {
        fprintf(stderr, "No comparison made (unknown input file).\n");
    }

    free(packets);
    free(connections);
    free(sortedPackets);
    return 0;
}

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
    fprintf(stderr, "\n----------------------------------------------------------------------------\n\n");

    while (fgets(expectedLine, sizeof(expectedLine), expectedFile) &&
           fgets(actualLine, sizeof(actualLine), actualFile))
    {
        // Remove trailing newline characters
        expectedLine[strcspn(expectedLine, "\r\n")] = 0;
        actualLine[strcspn(actualLine, "\r\n")] = 0;
        if (lineNumber > 31 && lineNumber < 37)
        {
            fprintf(stderr, "line %d: %s\n",
                    lineNumber, actualLine);
        }
        if (strcmp(expectedLine, actualLine) != 0)
        {
            fprintf(mismatchesFile, "Mismatch at line %d:\nExpected: %s\nActual  : %s\n\n",
                    lineNumber, expectedLine, actualLine);
            if (count < 5)
            {
                fprintf(stderr, "\nMismatch at line %d:\nExpected: %s\nActual  : %s\n",
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
    fprintf(stderr, "----------------------------------------------------------------------------\n");

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