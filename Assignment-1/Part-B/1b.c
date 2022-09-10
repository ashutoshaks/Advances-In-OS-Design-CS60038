#include <linux/errno.h>    // Needed for error codes
#include <linux/init.h>     // Needed for the macros
#include <linux/kernel.h>   // Needed for KERN_ALERT
#include <linux/module.h>   // Needed by all modules
#include <linux/mutex.h>    // Needed for mutex
#include <linux/proc_fs.h>  // Needed for proc filesystem
#include <linux/sched.h>    // Needed for current
#include <linux/slab.h>     // Needed for kmalloc

/*
1 proc file (max buff size = 256)
multiple processes possible (max is fixed = 128, no need to fix max if we use a dynamic linked list)
2 mutex locks for processes and proc file - read_write and open_close
*/

struct element {
    int val;
    int priority;
    int insert_time;
};

// comparison first based on priority, then on insert_time
int compare(struct element *a, struct element *b) {
    if (a->priority < b->priority) {
        return 1;
    } else if (a->priority > b->priority) {
        return 0;
    } else {
        return a->insert_time < b->insert_time;
    }
}

struct priority_queue {
    struct element *heap;
    int size;
    int capacity;
    int timer;
};

enum proc_state {
    PROC_FILE_OPEN,
    PROC_DEQUE_CREATE,
    PROC_DEQUE_WRITE,
    PROC_FILE_CLOSE
};

struct process {
    pid_t pid;
    enum proc_state state;
    struct priority_queue *proc_pq;
};

/*
Depending on max number of processes,
make either a linked list or a fixed array
*/

static struct priority_queue *create_pq(int capacity) {
    struct priority_queue *pq = kmalloc(sizeof(struct priority_queue), GFP_KERNEL);
    if (pq == NULL) {
        printk(KERN_ALERT "Error: could not allocate memory for priority queue");
        return NULL;
    }
    pq->heap = kmalloc(capacity * sizeof(struct element), GFP_KERNEL);
    if (pq->heap == NULL) {
        printk(KERN_ALERT "Error: could not allocate memory for priority queue heap array");
        return NULL;
    }
    pq->size = 0;
    pq->capacity = capacity;
    pq->timer = 0;
    return pq;
}

// implement a min heap based on first priority, then insert_time

static int insert(struct priority_queue *pq, int val, int priority) {
    if (pq->size == pq->capacity) {
        printk(KERN_ALERT "Error: priority queue is full");
        return -1;
    }
    pq->heap[pq->size].val = val;
    pq->heap[pq->size].priority = priority;
    pq->heap[pq->size].insert_time = pq->timer;
    pq->timer++;

    int i = pq->size;
    while (i > 0 && compare(&pq->heap[i], &pq->heap[(i - 1) / 2])) {
        struct element temp = pq->heap[i];
        pq->heap[i] = pq->heap[(i - 1) / 2];
        pq->heap[(i - 1) / 2] = temp;
        i = (i - 1) / 2;
    }
    pq->size++;
    return 0;
}

static int *extract_min(struct priority_queue *pq) {
    if (pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty");
        return -1;
    }
    int min_val = pq->heap[0].val;
    pq->heap[0] = pq->heap[pq->size - 1];
    pq->size--;
    int i = 0;
    while (i < pq->size) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        if (left >= pq->size) {
            break;
        }
        int min_child = left;
        if (right < pq->size && compare(&pq->heap[right], &pq->heap[left])) {
            min_child = right;
        }
        if (compare(&pq->heap[i], &pq->heap[min_child])) {
            struct element temp = pq->heap[i];
            pq->heap[i] = pq->heap[min_child];
            pq->heap[min_child] = temp;
            i = min_child;
        } else {
            break;
        }
    }
    return min_val;
}

/*
what to do with error codes?
how is the proc file read/write with priority values working?
(need to add one more state to indicate value/priority is being written)
can values be negative
limit on max processes?
*/