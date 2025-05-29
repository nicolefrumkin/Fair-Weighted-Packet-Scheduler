#include <stdio.h>
#include <stdbool.h>
#include "header.h"

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

// Enqueue an element
bool enqueue(Queue *q, int value) {
    if (isFull(q))
        return false;

    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->items[q->rear] = value;
    q->size++;
    return true;
}

// Dequeue an element
bool dequeue(Queue *q, int *removedItem) {
    if (isEmpty(q))
        return false;

    *removedItem = q->items[q->front];
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    q->size--;
    return true;
}

// Peek at the front element
bool peek(Queue *q, int *frontItem) {
    if (isEmpty(q))
        return false;

    *frontItem = q->items[q->front];
    return true;
}
