#include "header.h"

typedef struct {
    Packet *items[MAX_QUEUE_SIZE];
    int front;
    int rear;
    int size;
} Queue;

// Initialize queue
void initQueue(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

// Check if queue is empty
bool isEmpty(Queue *q) {
    return q->size == 0;
}

// Check if queue is full
bool isFull(Queue *q) {
    return q->size == MAX_QUEUE_SIZE;
}

// Enqueue a Packet*
bool enqueue(Queue *q, Packet *packet) {
    if (isFull(q)) return false;

    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->items[q->rear] = packet;
    q->size++;
    return true;
}

// Dequeue a Packet*
bool dequeue(Queue *q, Packet **packet) {
    if (isEmpty(q)) return false;

    *packet = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return true;
}

// Peek at the front Packet*
bool peek(Queue *q, Packet **packet) {
    if (isEmpty(q)) return false;

    *packet = q->items[q->front];
    return true;
}
