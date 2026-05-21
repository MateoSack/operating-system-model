#include "mutex_manager.h"

// Alway use first the mutex_manager_mutex, then internal_mutex and then the scheduler_mutex when locking both, to avoid deadlocks

t_mutex *mutex_create (char *name) { // Create a new mutex with the given name and add it to the list of mutexes
    t_mutex *mutex = malloc(sizeof(t_mutex));
    mutex->name = string_duplicate(name);
    mutex->isLocked = false;
    mutex->lockedBy = NULL;
    mutex->waitingProcesses = list_create();

    pthread_mutex_init(&mutex->internal_mutex, NULL);

    pthread_mutex_lock(&mutex_manager_mutex);
    list_add(list_mutexes, mutex);
    pthread_mutex_unlock(&mutex_manager_mutex);
    return mutex;
}

void mutex_lock (t_mutex *mutex, t_process *process) { // Lock a mutex for a process, if the mutex is already locked, block the process and add it to the waiting list of the mutex
    pthread_mutex_lock(&mutex->internal_mutex);

    if (!mutex->isLocked) {
        mutex->isLocked = true;
        mutex->lockedBy = process;

        pthread_mutex_unlock(&mutex->internal_mutex);

        log_info(logger, "## (%d) Mutex '%s' locked", process->pid, mutex->name);

    } else {
        if (mutex->lockedBy == process) { // If the process already owns the mutex, do nothing (avoid deadlock)
            pthread_mutex_unlock(&mutex->internal_mutex);
            log_warning(logger, "## (%d) Process already owns mutex '%s'", process->pid, mutex->name);
            return;
        }
        list_add(mutex->waitingProcesses, process);

        pthread_mutex_lock(&scheduler_mutex);
        
        process_set_state(process, BLOCK, logger);
        remove_process_from_list(exec_processes, process);
        
        pthread_mutex_unlock(&scheduler_mutex);
        
        pthread_mutex_unlock(&mutex->internal_mutex);

        evict_process(process);
    }
}

void mutex_unlock (t_mutex *mutex, t_process *process) { // Unlock a mutex, if there are processes waiting for the mutex, assign it to the next process in the waiting list and unblock it
    pthread_mutex_lock(&mutex->internal_mutex);

    if (mutex->lockedBy == NULL) {
        pthread_mutex_unlock(&mutex->internal_mutex);
        return;
    }

    if (mutex->lockedBy != process) {
        pthread_mutex_unlock(&mutex->internal_mutex);
        log_warning(logger, "## (%d) tried to unlock mutex '%s' without owning it", process->pid, mutex->name);
        return;
    }

    uint32_t previous_owner_pid = mutex->lockedBy->pid;

    if (!list_is_empty(mutex->waitingProcesses)) {
        t_process *next_process = list_remove(mutex->waitingProcesses, 0);
        mutex->isLocked = true;
        mutex->lockedBy = next_process;

        pthread_mutex_lock(&scheduler_mutex);
        process_set_state(next_process, READY, logger);
        add_process_to_list(ready_queue, next_process);
        
        pthread_mutex_unlock(&scheduler_mutex);
        
        pthread_mutex_unlock(&mutex->internal_mutex);

        sem_post(&short_term_scheduler_sem);

        log_info(logger, "## (%d) Mutex '%s' unlocked and assigned to process %d", previous_owner_pid, mutex->name, next_process->pid);

    } else {
        mutex->isLocked = false;
        mutex->lockedBy = NULL;

        pthread_mutex_unlock(&mutex->internal_mutex);

        log_info(logger, "## (%d) Mutex '%s' unlocked ", previous_owner_pid, mutex->name);
    }
}

t_mutex* get_mutex_by_name (char *name) { // Get a mutex from the list of mutexes based on its name, returns NULL if not found
    bool _mutex_name_coincides (void *ptr) {
        t_mutex *m = (t_mutex*)ptr;
        return string_equals_ignore_case(m->name, name);
    }

    pthread_mutex_lock(&mutex_manager_mutex);
    t_mutex *mutex = list_find(list_mutexes, _mutex_name_coincides);
    pthread_mutex_unlock(&mutex_manager_mutex);

    return mutex;
}
