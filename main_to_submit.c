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
        int connIndex = findOrCreateConnection(p, &connectionCount, connections, packetCount);
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
    double conVirtualTime = 0.0;
    double packetSentTime = 0.0;
    int packetNum = 0;

    while (packetNum < remaining)
    {
        Packet *minPacket = NULL;
        int minConnId = -1;

        // sum weights of all active connections
        double activeLinksWeight = 0.0;
        for (int i = 0; i < connectionCount; i++)
        {
            Packet *candidate;
            conVirtualTime = connections[i].virtualFinishTime;

            if (peek(&connections[i].queue, &candidate))
            {
                if (conVirtualTime > globalFinishTime)
                {
                    activeLinksWeight += connections[i].weight;
                }
            }
        }

        // Find the best available packet (eligible and lowest virtual finish time)
        for (int i = 0; i < connectionCount; i++)
        {
            Packet *candidate;
            conVirtualTime = connections[i].virtualFinishTime;

            if (peek(&connections[i].queue, &candidate))
            {
                // take packet only if it arrived before the current time and its connection is available
                if ((candidate->time <= globalFinishTime) && (conVirtualTime <= globalFinishTime))
                {
                    if (!minPacket || (candidate->virtualFinishTime < minPacket->virtualFinishTime) || (fabs(candidate->virtualFinishTime - minPacket->virtualFinishTime) < 1e-9 && i < minConnId))
                    {
                        minPacket = candidate;
                        minConnId = i;
                    }
                }
            }
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
        globalFinishTime = packetSentTime + minPacket->packetLength / (activeLinksWeight + minPacket->weight);
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
            if (packet->hasWeight)
            {
                connections[i].weight = packet->weight;
            }
            else
            {
                packet->weight = connections[i].weight; // Inherit weight from the connection
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
    }
    else
    {
        printf("%d: %d %s %d %s %d %d\n",
               packetSentTime,
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
        packet->weight = 1.0; // fallback only if weight wasn't given
        packet->hasWeight = 0;
    }
    else
    {
        packet->hasWeight = 1; // weight specified
    }
    packet->virtualFinishTime = packet->time + packet->packetLength;
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

