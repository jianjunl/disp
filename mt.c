
#include <stdio.h>
#include <pthread.h>
#include <errno.h>

void init_example() {
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
 
    // Initialize attributes
    int ret = pthread_mutexattr_init(&attr);
    if (ret != 0) { /* Handle error */ }
 
    // Set mutex type to recursive
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (ret != 0) { /* Handle error (e.g., ENOTSUP if unsupported) */ }
 
    // Initialize mutex with attributes
    ret = pthread_mutex_init(&mutex, &attr);
    if (ret != 0) { /* Handle error */ }
 
    // Clean up attributes
    pthread_mutexattr_destroy(&attr);
}

/* Note: This is simpler but less safe than the struct-based approach
 * (no protection against concurrent lock_count modifications).
 * Use only in single-core, non-preemptive environments.
 */
pthread_mutex_t my_mutex;
int lock_count = 0;
pthread_t owner_thread = 0;
 
void recursive_lock() {
    if (owner_thread == pthread_self()) {
        lock_count++;
        return;
    }
    pthread_mutex_lock(&my_mutex);
    owner_thread = pthread_self();
    lock_count = 1;
}
 
void recursive_unlock() {
    if (owner_thread != pthread_self()) {
        return; // Error: not owner
    }
    lock_count--;
    if (lock_count == 0) {
        owner_thread = 0;
        pthread_mutex_unlock(&my_mutex);
    }
}
 
typedef struct {
    pthread_mutex_t mutex;       // Underlying non-recursive mutex
    pthread_t owner;             // Owning thread ID
    int lock_count;              // Recursive lock counter
} recursive_mutex_t;
 
// Initialize recursive mutex
int recursive_mutex_init(recursive_mutex_t *rm) {
    rm->owner = 0;
    rm->lock_count = 0;
    return pthread_mutex_init(&rm->mutex, NULL);
}
 
// Lock recursive mutex
int recursive_mutex_lock(recursive_mutex_t *rm) {
    pthread_t self = pthread_self();
 
    // Lock the underlying mutex to protect owner/lock_count
    if (pthread_mutex_lock(&rm->mutex) != 0) {
        return -1; // Handle error
    }
 
    if (rm->owner == self) {
        // Already owning; increment count
        rm->lock_count++;
    } else if (rm->owner == 0) {
        // Not owned; take ownership
        rm->owner = self;
        rm->lock_count = 1;
    } else {
        // Owned by another thread; wait (this will block on the underlying mutex)
        // Note: This is redundant since we already locked the underlying mutex,
        // but included for clarity. In practice, the underlying mutex will block here.
        pthread_mutex_unlock(&rm->mutex);
        return -1; // Should not reach here
    }
 
    pthread_mutex_unlock(&rm->mutex); // Release underlying mutex
    return 0;
}
 
// Unlock recursive mutex
int recursive_mutex_unlock(recursive_mutex_t *rm) {
    pthread_t self = pthread_self();
 
    if (pthread_mutex_lock(&rm->mutex) != 0) {
        return -1;
    }
 
    if (rm->owner != self) {
        pthread_mutex_unlock(&rm->mutex);
        return EPERM; // Not the owner
    }
 
    rm->lock_count--;
    if (rm->lock_count == 0) {
        rm->owner = 0; // Release ownership
    }
 
    pthread_mutex_unlock(&rm->mutex);
    return 0;
}
 
// Destroy recursive mutex
int recursive_mutex_destroy(recursive_mutex_t *rm) {
    return pthread_mutex_destroy(&rm->mutex);
}

void *test_thread(void *arg) {
    recursive_mutex_t *rm = (recursive_mutex_t *)arg;
 
    printf("Lock 1\n");
    recursive_mutex_lock(rm);
    printf("Lock 2\n");
    recursive_mutex_lock(rm); // Should succeed
    printf("Unlock 1\n");
    recursive_mutex_unlock(rm);
    printf("Unlock 2\n");
    recursive_mutex_unlock(rm); // Should release
 
    return NULL;
}
 
int main() {
    recursive_mutex_t rm;
    pthread_t thread;
 
    recursive_mutex_init(&rm);
    pthread_create(&thread, NULL, test_thread, &rm);
    pthread_join(thread, NULL);
    recursive_mutex_destroy(&rm);
 
    return 0;
}

