#include <sys/queue.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <err.h>
#include "queue.h"
// creating semaphore:
// http://www.eventorient.com/2017/07/semaphores-in-mac-os-x-seminit-is.html
// tail queue:
// https://ofstack.com/C++/9343/c-language-tail-queue-tailq-is-shared-using-examples.html
TAILQ_HEAD(tailhead, connNode) head = TAILQ_HEAD_INITIALIZER(head);

struct connNode {
    int connfd;
    TAILQ_ENTRY(connNode) entries;
};

typedef sem_t Semaphore;

// Semaphores
Semaphore *sem;
Semaphore *lock;

static Semaphore *make_semaphore(int value) {
    Semaphore *semaphore = (Semaphore *) malloc(sizeof(Semaphore));
    semaphore = sem_open("/semaphore", O_CREAT, 0644, value);
    if (semaphore == SEM_FAILED) {
        errx(EXIT_FAILURE, "Failed to open semphore for empty");
    }
    sem_unlink("/semaphore");
    return semaphore;
}

static void semaphore_wait(Semaphore *semaphore) {
    sem_wait(semaphore);
}

static void semaphore_signal(Semaphore *semaphore) {
    sem_post(semaphore);
}

void createQueue(void) {
    sem = make_semaphore(0);
    lock = make_semaphore(1);
    TAILQ_INIT(&head);
}

void enqueue(int connfd) {

    semaphore_wait(lock);

    {
        struct connNode *node = (struct connNode *) malloc(sizeof(struct connNode));
        node->connfd = connfd;
        TAILQ_INSERT_TAIL(&head, node, entries);
    }

    semaphore_signal(lock);
    semaphore_signal(sem);
}

int dequeue(void) {
    int n = -1;
    semaphore_wait(sem);
    semaphore_wait(lock);

    {
        struct connNode *node = TAILQ_FIRST(&head);
        n = node->connfd;
        TAILQ_REMOVE(&head, node, entries); /* Deletion. */
        free(node);
    }

    semaphore_signal(lock);

    return n;
}
