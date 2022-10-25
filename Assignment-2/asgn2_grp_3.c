#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vanshita Garg and Ashutosh Kumar Singh");
MODULE_DESCRIPTION("LKM for a priority queue");
MODULE_VERSION("0.1");

#define PROCFS_NAME "cs60038_a2_grp3"

#define PB2_SET_CAPACITY _IOW(0x10, 0x31, int32_t *)
#define PB2_INSERT_INT _IOW(0x10, 0x32, int32_t *)
#define PB2_INSERT_PRIO _IOW(0x10, 0x33, int32_t *)
#define PB2_GET_INFO _IOW(0x10, 0x34, int32_t *)
#define PB2_GET_MIN _IOW(0x10, 0x35, int32_t *)
#define PB2_GET_MAX _IOW(0x10, 0x36, int32_t *)

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

// Comparison first based on priority and then on insert time
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
static struct process_node *process_list = NULL;

DEFINE_MUTEX(mutex);

struct obj_info {
    int32_t prio_que_size;  // current number of elements in priority queue
    int32_t capacity;       // maximum capacity of priority queue
};

// Priority queue functions

// Initialize the priority queue
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

static void shift_up(struct priority_queue *pq, int i) {
    while (i > 0 && compare(&pq->heap[i], &pq->heap[(i - 1) / 2])) {
        struct element temp = pq->heap[i];
        pq->heap[i] = pq->heap[(i - 1) / 2];
        pq->heap[(i - 1) / 2] = temp;
        i = (i - 1) / 2;
    }
}

static void shift_down(struct priority_queue *pq, int i) {
    int left, right, min_child;
    while (i < pq->size) {
        left = 2 * i + 1;
        right = 2 * i + 2;
        if (left >= pq->size) {
            break;
        }
        min_child = left;
        if (right < pq->size && compare(&pq->heap[right], &pq->heap[left])) {
            min_child = right;
        }
        if (!compare(&pq->heap[i], &pq->heap[min_child])) {
            struct element temp = pq->heap[i];
            pq->heap[i] = pq->heap[min_child];
            pq->heap[min_child] = temp;
            i = min_child;
        } else {
            break;
        }
    }
}

// Insert an element into the priority queue
static int insert(struct priority_queue *pq, int val, int priority) {
    if (pq->size == pq->capacity) {
        printk(KERN_ALERT "Error: priority queue is full\n");
        return -EACCES;
    }
    pq->heap[pq->size].val = val;
    pq->heap[pq->size].priority = priority;
    pq->heap[pq->size].insert_time = pq->timer;
    pq->timer++;

    shift_up(pq, pq->size);
    pq->size++;
    return 0;
}

// Extract the minimum element from the priority queue
static int extract_min(struct priority_queue *pq, struct element *min_elem) {
    if (pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
    }
    min_elem->val = pq->heap[0].val;
    min_elem->priority = pq->heap[0].priority;
    min_elem->insert_time = pq->heap[0].insert_time;

    pq->heap[0] = pq->heap[pq->size - 1];
    pq->size--;
    shift_down(pq, 0);
    return 0;
}

static int extract_max(struct priority_queue *pq, struct element *max_elem) {
    struct element max_el, temp;
    int max_ind, i;

    if (pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
    }
    max_ind = 0;
    max_el = pq->heap[0];
    for (i = 1; i < pq->size; i++) {
        if (compare(&max_el, &pq->heap[i])) {
            max_el = pq->heap[i];
            max_ind = i;
        }
    }
    max_elem->val = max_el.val;
    max_elem->priority = max_el.priority;
    max_elem->insert_time = max_el.insert_time;

    pq->heap[max_ind].priority = -1;
    shift_up(pq, max_ind);
    extract_min(pq, &temp);
    return 0;
}

// Print priority queue
static void print_pq(struct priority_queue *pq) {
#ifdef DEBUG
    printk(KERN_INFO "Priority queue for process %d:\n", current->pid);
    if (pq != NULL && pq->heap != NULL) {
        int i;
        for (i = 0; i < pq->size; i++) {
            printk(KERN_INFO "%d  [%d, %d, %d]\n", i, pq->heap[i].val, pq->heap[i].priority, pq->heap[i].insert_time);
        }
    }
    printk("\n");
#endif
}

// Free the memory allocated to the priority queue
static void delete_pq(struct priority_queue *pq) {
    if (pq != NULL) {
        kfree(pq->heap);
        kfree(pq);
    }
}

// Find the process node with the given pid
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

// Free the memory allocated to a process node
static void delete_process_node(struct process_node *node) {
    if (node != NULL) {
        delete_pq(node->proc_pq);
        kfree(node);
    }
}

// Delete a process node with the given pid
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

// Free the memory allocated to all process nodes
static void delete_process_list(void) {
    struct process_node *curr = process_list;
    while (curr != NULL) {
        struct process_node *temp = curr;
        curr = curr->next;
        delete_process_node(temp);
    }
}

// Open, close, read and write handlers for proc file

// Open handler for proc file
static int procfile_open(struct inode *inode, struct file *file) {
    pid_t pid;
    int ret;
    struct process_node *curr;

    mutex_lock(&mutex);

    pid = current->pid;
    printk(KERN_INFO "procfile_open() invoked by process %d\n", pid);
    ret = 0;

    curr = find_process(pid);
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

// Close handler for proc file
static int procfile_close(struct inode *inode, struct file *file) {
    pid_t pid;
    int ret;
    struct process_node *curr;

    mutex_lock(&mutex);

    pid = current->pid;
    printk(KERN_INFO "procfile_close() invoked by process %d\n", pid);
    ret = 0;

    curr = find_process(pid);
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

static long pb2_set_capacity(unsigned long arg, struct process_node *curr) {
    int32_t capacity;

    printk(KERN_INFO "PB2_SET_CAPACITY invoked by process %d\n", curr->pid);
    if (copy_from_user(&capacity, (int32_t *)arg, sizeof(int32_t)) != 0) {
        printk(KERN_ALERT "Error: could not copy capacity from user\n");
        return -EINVAL;
    }
    if (curr->state != PROC_FILE_OPEN) {
        delete_pq(curr->proc_pq);
    }
    if (capacity < 1 || capacity > 100) {
        printk(KERN_ALERT "Error: Capacity must be between 1 and 100\n");
        return -EINVAL;
    }
    curr->proc_pq = create_pq(capacity);
    if (curr->proc_pq == NULL) {
        printk(KERN_ALERT "Error: priority queue initialization failed\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "Priority queue with capacity %d has been intialized for process %d\n", capacity, curr->pid);
    curr->state = PROC_READ_VALUE;
    return 0;
}

static long pb2_insert_int(unsigned long arg, struct process_node *curr) {
    int32_t value;

    printk(KERN_INFO "PB2_INSERT_INT invoked by process %d\n", curr->pid);
    if (copy_from_user(&value, (int32_t *)arg, sizeof(int32_t)) != 0) {
        printk(KERN_ALERT "Error: could not copy value from user\n");
        return -EINVAL;
    }
    if (curr->state == PROC_READ_VALUE) {
        curr->proc_pq->last_value = value;
        printk(KERN_INFO "Value %d has been written to the proc file for process %d\n", value, curr->pid);
        curr->state = PROC_READ_PRIORITY;
    } else if (curr->state == PROC_FILE_OPEN) {
        printk(KERN_ALERT "Error: process %d has not set the capacity of the priority queue\n", curr->pid);
        return -EACCES;
    } else if (curr->state == PROC_READ_PRIORITY) {
        printk(KERN_ALERT "Error: process %d is supposed to enter a value, not priority\n", curr->pid);
        return -EACCES;
    }
    return 0;
}

static long pb2_insert_prio(unsigned long arg, struct process_node *curr) {
    int32_t prio;

    printk(KERN_INFO "PB2_INSERT_PRIO invoked by process %d\n", curr->pid);
    if (copy_from_user(&prio, (int32_t *)arg, sizeof(int32_t)) != 0) {
        printk(KERN_ALERT "Error: could not copy priority from user\n");
        return -EINVAL;
    }
    if (curr->state == PROC_READ_PRIORITY) {
        if (curr->proc_pq->size == curr->proc_pq->capacity) {
            printk(KERN_ALERT "Error: priority queue is full\n");
            return -EACCES;
        }
        if (prio < 1) {
            printk(KERN_ALERT "Error: Priority must be a positive integer\n");
            return -EINVAL;
        }
        printk(KERN_INFO "Priority %d has been written to the proc file for process %d\n", prio, curr->pid);
        insert(curr->proc_pq, curr->proc_pq->last_value, prio);
        printk(KERN_INFO "(%d, %d) value-priority element has been inserted into the priority queue for process %d\n", curr->proc_pq->last_value, prio, curr->pid);
        curr->state = PROC_READ_VALUE;
    } else if (curr->state == PROC_FILE_OPEN) {
        printk(KERN_ALERT "Error: process %d has not set the capacity of the priority queue\n", curr->pid);
        return -EACCES;
    } else if (curr->state == PROC_READ_PRIORITY) {
        printk(KERN_ALERT "Error: process %d is supposed to enter priority, not value\n", curr->pid);
        return -EACCES;
    }
    return 0;
}

static long pb2_get_info(unsigned long arg, struct process_node *curr) {
    struct obj_info info;
    printk(KERN_INFO "PB2_GET_INFO invoked by process %d\n", curr->pid);
    info.prio_que_size = curr->proc_pq->size;
    info.capacity = curr->proc_pq->capacity;
    if (copy_to_user((struct obj_info *)arg, &info, sizeof(struct obj_info))) {
        printk(KERN_ALERT "Error: could not copy info to user\n");
        return -EINVAL;
    }
    return 0;
}

static long pb2_get_min(unsigned long arg, struct process_node *curr) {
    int min_val;
    struct element min_elem;
    printk(KERN_INFO "PB2_GET_MIN invoked by process %d\n", curr->pid);
    if (curr->state == PROC_FILE_OPEN) {
        printk(KERN_ALERT "Error: process %d has not yet written anything to the proc file\n", curr->pid);
        return -EACCES;
    }
    // curr->proc_pq cannot be NULL if the control comes here
    if (curr->proc_pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
    }
    extract_min(curr->proc_pq, &min_elem);
    min_val = min_elem.val;
    if (copy_to_user((int32_t *)arg, &min_val, sizeof(int32_t))) {
        printk(KERN_ALERT "Error: could not copy min value to user\n");
        return -EINVAL;
    }
    return 0;
}

static long pb2_get_max(unsigned long arg, struct process_node *curr) {
    int max_val;
    struct element max_elem;
    printk(KERN_INFO "PB2_GET_MAX invoked by process %d\n", curr->pid);
    if (curr->state == PROC_FILE_OPEN) {
        printk(KERN_ALERT "Error: process %d has not yet written anything to the proc file\n", curr->pid);
        return -EACCES;
    }
    // curr->proc_pq cannot be NULL if the control comes here
    if (curr->proc_pq->size == 0) {
        printk(KERN_ALERT "Error: priority queue is empty\n");
        return -EACCES;
    }
    extract_max(curr->proc_pq, &max_elem);
    max_val = max_elem.val;
    if (copy_to_user((int32_t *)arg, &max_val, sizeof(int32_t))) {
        printk(KERN_ALERT "Error: could not copy max value to user\n");
        return -EINVAL;
    }
    return 0;
}

static long proc_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    int ret;
    pid_t pid;
    struct process_node *curr;

    mutex_lock(&mutex);

    pid = current->pid;
    curr = find_process(pid);
    if (curr == NULL) {
        printk(KERN_ALERT "Error: process %d does not have the proc file open\n", pid);
        mutex_unlock(&mutex);
        return -EACCES;
    }

    if (cmd == PB2_SET_CAPACITY) {
        ret = pb2_set_capacity(arg, curr);
    } else if (cmd == PB2_INSERT_INT) {
        ret = pb2_insert_int(arg, curr);
    } else if (cmd == PB2_INSERT_PRIO) {
        ret = pb2_insert_prio(arg, curr);
    } else if (cmd == PB2_GET_INFO) {
        ret = pb2_get_info(arg, curr);
    } else if (cmd == PB2_GET_MIN) {
        ret = pb2_get_min(arg, curr);
    } else if (cmd == PB2_GET_MAX) {
        ret = pb2_get_max(arg, curr);
    } else {
        printk(KERN_ALERT "Error: invalid ioctl command\n");
        ret = -EINVAL;
    }

    print_pq(curr->proc_pq);
    mutex_unlock(&mutex);
    return ret;
}

static const struct proc_ops proc_fops = {
    .proc_open = procfile_open,
    .proc_release = procfile_close,
    .proc_ioctl = proc_ioctl};

// Module initialization
static int __init lkm_init(void) {
    printk(KERN_INFO "LKM for partb_1_3 loaded\n");

    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_fops);
    if (proc_file == NULL) {
        printk(KERN_ALERT "Error: could not create proc file\n");
        return -ENOENT;
    }
    printk(KERN_INFO "/proc/%s created\n", PROCFS_NAME);
    return 0;
}

// Module cleanup
static void __exit lkm_exit(void) {
    delete_process_list();
    remove_proc_entry(PROCFS_NAME, NULL);
    printk(KERN_INFO "/proc/%s removed\n", PROCFS_NAME);
    printk(KERN_INFO "LKM for partb_1_3 unloaded\n");
}

module_init(lkm_init);
module_exit(lkm_exit);
