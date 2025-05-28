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
    int connection_id;
    int time;
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    int length_packet;
    int weight;
    int virtual_finish_time;
} Packet;

typedef struct Connection
{
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    int weight;
    int last_finish_time;
    int first_appear_idx;
    bool is_active;
} Connection;

Packet packets[MAX_PACKETS];
Connection connections[MAX_CONNECTIONS];
int packet_count = 0;
int connection_count = 0;

int maxi(int a, int b)
{
    return a > b ? a : b;
}

int find_or_create_connection(Packet *packet)
{
    for (int i = 0; i < connection_count; i++)
    {
        if (connections[i].is_active &&
            strcmp(connections[i].Sadd, packet->Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet->Dadd) == 0 &&
            connections[i].Sport == packet->Sport &&
            connections[i].Dport == packet->Dport)
        { // Connection exists
            return i;
        }
    }

    // New connection
    int idx = connection_count++;
    strcpy(connections[idx].Sadd, packet->Sadd);
    strcpy(connections[idx].Dadd, packet->Dadd);
    connections[idx].Sport = packet->Sport;
    connections[idx].Dport = packet->Dport;
    connections[idx].weight = packet->weight;
    connections[idx].last_finish_time = 0.0;
    connections[idx].first_appear_idx = idx;
    connections[idx].is_active = true;
    return idx;
}

void process_packet(Packet *packet)
{
    int conn_id = find_or_create_connection(packet);
    Connection *conn = &connections[conn_id];

    if (packet->weight == 1 && conn->weight != 1)
        packet->weight = conn->weight;
    else
        conn->weight = packet->weight; // update with explicit value

    double start = maxi(conn->last_finish_time, packet->time);
    packet->virtual_finish_time = start + packet->length_packet / packet->weight;
    conn->last_finish_time = packet->virtual_finish_time;
    packet->connection_id = conn_id;
}

int compare_packets(const void *a, const void *b)
{
    Packet *packet_a = (Packet *)a;
    Packet *packet_b = (Packet *)b;

    if (packet_a->virtual_finish_time < packet_b->virtual_finish_time)
        return -1;
    if (packet_a->virtual_finish_time > packet_b->virtual_finish_time)
        return 1;
    int conn1 = connections[packet_a->connection_id].first_appear_idx;
    int conn2 = connections[packet_b->connection_id].first_appear_idx;
    return conn1 - conn2;
}

int main()
{
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), stdin))
    {
        Packet *packet = &packets[packet_count++];
        int parsed_elementes = sscanf(line, " %d %s %d %s %d %d %d",
                                      &packet->time, packet->Sadd, &packet->Sport,
                                      packet->Dadd, &packet->Dport, &packet->length_packet, &packet->weight);
        if (parsed_elementes == 6)
            packet->weight = 1;
        process_packet(packet);
        packet->id = packet_count;
    }
    qsort(packets, packet_count, sizeof(Packet), compare_packets);

    for (int i = 0; i < packet_count; i++)
    {
        Packet *p = &packets[i];
        printf("%d: %d %s %d %s %d %d\n",
               (int)p->virtual_finish_time,
               p->time, p->Sadd, p->Sport, p->Dadd, p->Dport, p->length_packet);
    }

    return 0;
}