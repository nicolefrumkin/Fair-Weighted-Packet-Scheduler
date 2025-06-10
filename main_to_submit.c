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
        enqueue(&connections[connIndex].queue, p);
        packetCount++;
    }
    drainPackets(connections, connectionCount, packetCount);
    free(packets);
    free(connections);
    return 0;
}

void drainPackets(Connection *connections, int connectionCount, int remaining)
{
    double virtualTime = 0.0;
    int lastRealTime = 0;
    double conVirtualTime = 0.0;
    while (remaining > 0)
    {
        // --- Update virtual time ---
        int elapsed = globalOutputFinishTime - lastRealTime;
        double totalWeight = 0.0;

        for (int i = 0; i < connectionCount; i++)
        {
            if (!isEmpty(&connections[i].queue))
            {
                Packet *candidate;
                peek(&connections[i].queue, &candidate);
                conVirtualTime = connections[candidate->connectionID].virtualFinishTime;
                if (candidate->hasWeight)
                {
                    connections[i].weight = candidate->weight;
                }
                else
                {
                    candidate->weight = connections[i].weight; // Inherit weight from the connection
                }
                if (candidate->time <= globalOutputFinishTime && conVirtualTime <= globalOutputFinishTime)
                {
                    totalWeight += connections[i].weight;
                }
            }
        }
        if (totalWeight > 0)
        {
            virtualTime += (double)elapsed / totalWeight;
        }
        lastRealTime = globalOutputFinishTime;
    
        Packet *bestPacket = NULL;
        int bestConn = -1;

        // Find the best available packet (eligible and lowest virtual finish time)
        for (int i = 0; i < connectionCount; i++)
        {
            if (!isEmpty(&connections[i].queue))
            {
                Packet *candidate;
                peek(&connections[i].queue, &candidate);

                // Calculate virtual start and finish time using current virtual time
                candidate->virtualFinishTime = globalOutputFinishTime + ((double)candidate->packetLength / candidate->weight);
                conVirtualTime = connections[candidate->connectionID].virtualFinishTime;

                if ((candidate->time <= globalOutputFinishTime) && (conVirtualTime <= globalOutputFinishTime))
                {
                    if (!bestPacket || (candidate->virtualFinishTime < bestPacket->virtualFinishTime) || (fabs(candidate->virtualFinishTime - bestPacket->virtualFinishTime)<1e-9 && i < bestConn))
                    {
                        bestPacket = candidate;
                        bestConn = i;
                    }
                }
            }
        }
        if (!bestPacket)
        {
            // Advance to the next earliest arrival if no packet is eligible now
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
        // Calculate real start time and update global finish time
        double start = fmax(bestPacket->time, globalOutputFinishTime);
        globalOutputFinishTime = start + bestPacket->packetLength;

        // Save last virtual finish time for this connection
        connections[bestConn].virtualFinishTime = globalOutputFinishTime;

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
    packet->weight = 1.0;
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
    packet->virtualFinishTime = 0.0;
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