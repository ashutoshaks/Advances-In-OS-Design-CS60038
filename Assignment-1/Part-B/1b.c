#include <linux/errno.h>    // Needed for error codes
#include <linux/init.h>     // Needed for the macros
#include <linux/kernel.h>   // Needed for KERN_ALERT
#include <linux/module.h>   // Needed by all modules
#include <linux/mutex.h>    // Needed for mutex
#include <linux/proc_fs.h>  // Needed for proc filesystem
#include <linux/sched.h>    // Needed for current
#include <linux/slab.h>     // Needed for kmalloc
#include <linux/string.h>   // Needed for strlen
#include <linux/uaccess.h>  // Needed for copy_from_user

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vanshita Garg and Ashutosh Kumar Singh");
MODULE_DESCRIPTION("LKM for a priority queue");
MODULE_VERSION("0.1");

#define PROCFS_NAME "partb_1_3"
#define PROCFS_MAX_SIZE 1024

/*
1 proc file (max buff size = 1024)
multiple processes possible (max is fixed = 128, no need to fix max if we use a dynamic linked list)
2 mutex locks for processes and proc file - read_write and open_close
*/

// Throughout the code, return negative of error codes to denote error conditions
// Also keep printing log and debug messages

struct element {
    int val;
    int priority;
    int insert_time;
};

struct priority_queue {
    struct element *heap;
    int size;
    int capacity;
    int last_value;
    int timer;
};

// Comparison first based on priority, then on insert_time
int compare(struct element *a, struct element *b) {
    if (a->priority < b->priority) {
        return 1;
    } else if (a->priority > b->priority) {
        return 0;
    } else {
        return a->insert_time < b->insert_time;
    }
}

enum proc_state {
    PROC_FILE_OPEN,
    PROC_READ_VALUE,
    PROC_READ_PRIORITY,
};

// Linked list of processes
struct process_node {
    pid_t pid;
    enum proc_state state;
    struct priority_queue *proc_pq;
    struct process_node *next;
};

// Global variables
static struct proc_dir_entry *proc_file;
static char procfs_buffer[PROCFS_MAX_SIZE];
static size_t procfs_buffer_size = 0;
static struct process_node *process_list = NULL;

DEFINE_MUTEX(mutex);

// Priority queue functions

static struct priority_queue *create_pq(int capacity) {
    struct priority_queue *pq = kmalloc(sizeof(struct priority_queue), GFP_KERNEL);
    if (pq == NULL) {
        printk(KERN_ALERT "Error: could not allocate memory for priority queue\n");
        return NULL;
    }
    pq->heap = kmalloc(capacity * sizeof(struct element), GFP_KERNEL);
    if (pq->heap == NULL) {
        printk(KERN_ALERT "Error: could not allocate memory for priority queue heap array\n");
        return NULL;
    }
    pq->size = 0;
    pq->capacity = capacity;
    pq->last_value = 0;
    pq->timer = 0;
    return pq;
}

// implement a min heap based on first priority, then insert_time

static int insert_pq(struct priority_queue *pq, int val, int priority) {
    if (pq->size == pq->capacity) {
        printk(KERN_ALERT "Error: priority queue is full\n");
        return -EACCES;
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

static int extract_min(struct priority_queue *pq) {
    if (pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
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

static void delete_pq(struct priority_queue *pq) {
    if (pq != NULL) {
        kfree(pq->heap);
        kfree(pq);
    }
}

// Fnd the process node with the given pid
static struct process_node *find_process(pid_t pid) {
    struct process_node *curr = process_list;
    while (curr != NULL) {
        if (curr->pid == pid) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

// Insert a process node with the given pid
static struct process_node *insert_process(pid_t pid) {
    struct process_node *node = kmalloc(sizeof(struct process_node), GFP_KERNEL);
    if (node == NULL) {
        return NULL;
    }
    node->pid = pid;
    node->state = PROC_FILE_OPEN;
    node->proc_pq = NULL;
    node->next = process_list;
    process_list = node;
    return node;
}

static void delete_process_node(struct process_node *node) {
    if (node != NULL) {
        delete_pq(node->proc_pq);
        kfree(node);
    }
}

static int delete_process(pid_t pid) {
    struct process_node *prev = NULL;
    struct process_node *curr = process_list;
    while (curr != NULL) {
        if (curr->pid == pid) {
            if (prev == NULL) {
                process_list = curr->next;
            } else {
                prev->next = curr->next;
            }
            delete_process_node(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -EACCES;
}

static void delete_process_list(void) {
    struct process_node *curr = process_list;
    while (curr != NULL) {
        struct process_node *temp = curr;
        curr = curr->next;
        delete_process_node(temp);
    }
}

// Open, close, read and write handlers for proc file

static int procfile_open(struct inode *inode, struct file *file) {
    mutex_lock(&mutex);

    pid_t pid = current->pid;
    printk(KERN_INFO "procfile_open() invoked by process %d\n", pid);
    int ret = 0;

    struct process_node *curr = find_process(pid);
    if (curr == NULL) {
        curr = insert_process(pid);
        if (curr == NULL) {
            printk(KERN_ALERT "Error: could not allocate memory for process node\n");
            ret = -ENOMEM;
        } else {
            printk(KERN_INFO "Process %d has been added to the process list\n", pid);
        }
    } else {
        printk(KERN_ALERT "Error: process %d has the proc file already open\n", pid);
        ret = -EACCES;
    }

    mutex_unlock(&mutex);
    return ret;
}

static int procfile_close(struct inode *inode, struct file *file) {
    mutex_lock(&mutex);

    pid_t pid = current->pid;
    printk(KERN_INFO "procfile_close() invoked by process %d\n", pid);
    int ret = 0;

    struct process_node *curr = find_process(pid);
    if (curr == NULL) {
        printk(KERN_ALERT "Error: process %d does not have the proc file open\n", pid);
        ret = -EACCES;
    } else {
        delete_process(pid);
        printk(KERN_INFO "Process %d has been removed from the process list\n", pid);
    }

    mutex_unlock(&mutex);
    return ret;
}

static ssize_t handle_read(struct process_node *curr) {
    if (curr->state == PROC_FILE_OPEN) {
        printk(KERN_ALERT "Error: process %d has not yet written anything to the proc file\n", curr->pid);
        return -EACCES;
    }
    // curr->proc_pq cannot be NULL if the control comes here
    if (curr->proc_pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
    }
    int min_val = extract_min(curr->proc_pq);
    strncpy(procfs_buffer, (const char *)&min_val, sizeof(int));
    procfs_buffer[sizeof(int)] = '\0';
    procfs_buffer_size = sizeof(int);
    return procfs_buffer_size;
}

static ssize_t procfile_read(struct file *filep, char __user *buffer, size_t length, loff_t *offset) {
    mutex_lock(&mutex);

    pid_t pid = current->pid;
    printk(KERN_INFO "procfile_read() invoked by process %d\n", pid);
    int ret = 0;

    struct process_node *curr = find_process(pid);
    if (curr == NULL) {
        printk(KERN_ALERT "Error: process %d does not have the proc file open\n", pid);
        ret = -EACCES;
    } else {
        procfs_buffer_size = min(length, (size_t)PROCFS_MAX_SIZE);
        ret = handle_read(curr);
        if (ret >= 0) {
            if (copy_to_user(buffer, procfs_buffer, procfs_buffer_size) != 0) {
                printk(KERN_ALERT "Error: could not copy data to user space\n");
                ret = -EACCES;
            } else {
                ret = procfs_buffer_size;
            }
        }
    }
    mutex_unlock(&mutex);
    return ret;
}

static ssize_t handle_write(struct process_node *curr) {
    if (curr->state == PROC_FILE_OPEN) {
        if (procfs_buffer_size > 1ul) {
            printk(KERN_ALERT "Error: Buffer size for capacity must be 1 byte\n");
            return -EINVAL;
        }
        size_t capacity = (size_t)procfs_buffer[0];
        if (capacity < 1 || capacity > 100) {
            printk(KERN_ALERT "Error: Capacity must be between 1 and 100\n");
            return -EINVAL;
        }
        curr->proc_pq = create_pq(capacity);
        if (curr->proc_pq == NULL) {
            printk(KERN_ALERT "Error: priority queue initialization failed\n");
            return -ENOMEM;
        }
        printk(KERN_INFO "Priority queue with capacity %zu has been intialized for process %d\n", capacity, curr->pid);
        curr->state = PROC_READ_VALUE;
    } else if (curr->state == PROC_READ_VALUE) {
        if (procfs_buffer_size > 4ul) {  // sizeof(int)
            printk(KERN_ALERT "Error: Buffer size for value must be 4 bytes\n");
            return -EINVAL;
        }
        if (curr->proc_pq->size == curr->proc_pq->capacity) {
            printk(KERN_ALERT "Error: priority queue is full\n");
            return -EACCES;
        }
        int value = *((int *)procfs_buffer);
        curr->proc_pq->last_value = value;
        printk(KERN_INFO "Value %d has been written to the proc file for process %d\n", value, curr->pid);
        curr->state = PROC_READ_PRIORITY;
    } else if (curr->state == PROC_READ_PRIORITY) {
        if (procfs_buffer_size > 4ul) {  // sizeof(int)
            printk(KERN_ALERT "Error: Buffer size for priority must be 4 bytes\n");
            return -EINVAL;
        }
        if (curr->proc_pq->size == curr->proc_pq->capacity) {
            printk(KERN_ALERT "Error: priority queue is full\n");
            return -EACCES;
        }
        int priority = *((int *)procfs_buffer);
        if (priority < 1) {
            printk(KERN_ALERT "Error: Priority must be a positive integer\n");
            return -EINVAL;
        }
        printk(KERN_INFO "Priority %d has been written to the proc file for process %d\n", priority, curr->pid);
        int ret = insert_pq(curr->proc_pq, curr->proc_pq->last_value, priority);
        if (ret < 0) {
            printk(KERN_ALERT "Error: priority queue insertion failed\n");
            return -EACCES;
        }
        printk(KERN_INFO "(%d, %d) value-priority element has been inserted into the priority queue for process %d\n", curr->proc_pq->last_value, priority, curr->pid);
        curr->state = PROC_READ_VALUE;
    }
    return procfs_buffer_size;
}

static ssize_t procfile_write(struct file *filep, const char __user *buffer, size_t length, loff_t *offset) {
    mutex_lock(&mutex);

    pid_t pid = current->pid;
    printk(KERN_INFO "procfile_write() invoked by process %d\n", pid);
    int ret = 0;

    struct process_node *curr = find_process(pid);
    if (curr == NULL) {
        printk(KERN_ALERT "Error: process %d does not have the proc file open\n", pid);
        ret = -EACCES;
    } else {
        if (buffer == NULL || length == 0) {
            printk(KERN_ALERT "Error: empty write\n");
            ret = -EINVAL;
        } else {
            procfs_buffer_size = min(length, (size_t)PROCFS_MAX_SIZE);
            if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size)) {
                printk(KERN_ALERT "Error: could not copy from user\n");
                ret = -EFAULT;
            } else {
                ret = handle_write(curr);
            }
        }
    }
    mutex_unlock(&mutex);
    return ret;
}

static const struct proc_ops proc_fops = {
    .proc_open = procfile_open,
    .proc_read = procfile_read,
    .proc_write = procfile_write,
    .proc_release = procfile_close,
};

static int __init lkm_init(void) {
    printk(KERN_INFO "LKM for partb_1_3 loaded\n");

    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_fops);
    if (proc_file == NULL) {
        printk(KERN_ALERT "Error: could not create proc file\n");
        return -ENOENT;
    }
    printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);
    // Buffer for proc file is statically allocated
    return 0;
}

static void __exit lkm_exit(void) {
    delete_process_list();
    remove_proc_entry(PROCFS_NAME, NULL);
    printk(KERN_INFO "/proc/%s removed\n", PROCFS_NAME);
    printk(KERN_INFO "LKM for partb_1_3 unloaded\n");
}

module_init(lkm_init);
module_exit(lkm_exit);

/*
what to do with error codes?
how is the proc file read/write with priority values working?
(need to add one more state to indicate value/priority is being written)
can values be negative
limit on max processes?
*/