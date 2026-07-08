#ifndef MUTEX_MANAGER_H
#define MUTEX_MANAGER_H

#include <utils.h>
#include <commons/temporal.h>

extern t_log *logger;
extern t_scheduler_algorithm scheduler_algorithm;
extern pthread_mutex_t scheduler_mutex;
extern pthread_mutex_t list_mutex_mutex;
extern sem_t short_term_scheduler_sem;
extern t_list **ready_queue;
extern t_list *list_mutexes;
extern t_temporal *system_timer;
extern t_list *block_processes;
extern pthread_mutex_t block_processes_mutex;

typedef struct {
    char *name;
    bool isLocked;
    t_process*lockedBy;
    t_list *waitingProcesses;
    pthread_mutex_t internal_mutex;
} t_mutex;

t_mutex *mutex_create (char *name);
void mutex_destroy (void *arg);
void mutex_lock (t_mutex *mutex, t_process *process);
void mutex_unlock (t_mutex *mutex, t_process *process);
t_mutex* get_mutex_by_name (char *name);
void destroy_list_of_mutexes (t_list *list);

#endif
