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
    double weight;
    double virtual_finish_time;
} Packet;

typedef struct Connection
{
    char Sadd[MAX_SADD_LENGTH];
    int Sport;
    char Dadd[MAX_SADD_LENGTH];
    int Dport;
    double weight;
    double last_finish_time;
    int first_appear_idx;
    int flagweight;
    bool is_active;
} Connection;

Packet packets[MAX_PACKETS];
Connection connections[MAX_CONNECTIONS];
int packet_count = 0;
int connection_count = 0;

int find_or_create_connection(Packet *packet)
{
    // printf("Im here3\n");
    // fflush(stdout);
    for (int i = 0; i < connection_count; i++)
    {
        if (connections[i].is_active &&
            strcmp(connections[i].Sadd, packet->Sadd) == 0 &&
            strcmp(connections[i].Dadd, packet->Dadd) == 0 &&
            connections[i].Sport == packet->Sport &&
            connections[i].Dport == packet->Dport)
        { // Connection exists
            if (packet->weight == 1.0)
            {
                connections[i].flagweight = 0; // flagweight is 0 for packets with weight 1
            }
            else
            {
                connections[i].flagweight = 1; // flagweight is 1 for packets with explicit weight
            }
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
    if (packet->weight == 1.0)
    {
        connections[idx].flagweight = 0; // flagweight is 0 for packets with weight 1
    }
    else
    {
        connections[idx].flagweight = 1; // flagweight is 1 for packets with explicit weight
    }
    return idx;
}

int process_packet(Packet *packet)
{
    int conn_id = find_or_create_connection(packet);
    Connection *conn = &connections[conn_id];

    if (packet->weight == 1 && conn->weight != 1)
        packet->weight = conn->weight;
    else
        conn->weight = packet->weight; // update with explicit value

    double start = conn->last_finish_time > (double)packet->time ? conn->last_finish_time : (double)packet->time;
    packet->virtual_finish_time = start + (double)packet->length_packet / conn->weight;
    conn->last_finish_time = packet->virtual_finish_time;
    packet->connection_id = conn_id;

    return connections[conn_id].flagweight;
}

int compare_packets(const void *a, const void *b)
{

    Packet *packet_a = (Packet *)a;
    Packet *packet_b = (Packet *)b;
    fflush(stdout);

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
    // printf("Im here1\n");
    // fflush(stdout);
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), stdin))
    {
        Packet *packet = &packets[packet_count];
        int parsed_elements = sscanf(line, " %d %s %d %s %d %d %lf",
                                     &packet->time, packet->Sadd, &packet->Sport,
                                     packet->Dadd, &packet->Dport, &packet->length_packet, &packet->weight);
        if (parsed_elements == 6)
            packet->weight = 1.0;
        packet->id = packet_count;
        int flagWeight = process_packet(packet);
        if (flagWeight == 0)
        {
            printf("%d: %d %s %d %s %d %d\n",
                   (int)packet->virtual_finish_time,
                   packet->time, packet->Sadd, packet->Sport,
                   packet->Dadd, packet->Dport, packet->length_packet);
            fflush(stdout);
        }
        else
        {
            printf("%d: %d %s %d %s %d %d %.2f\n",
                   (int)packet->virtual_finish_time,
                   packet->time, packet->Sadd, packet->Sport,
                   packet->Dadd, packet->Dport, packet->length_packet, packet->weight);

            fflush(stdout);
        }
        packet_count++;
    }
    return 0;
}