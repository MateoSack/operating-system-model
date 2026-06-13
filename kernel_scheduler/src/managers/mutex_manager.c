#include "mutex_manager.h"

// Alway use first the list_mutex_mutex, then internal_mutex and then the scheduler_mutex when locking both, to avoid deadlocks

t_mutex *mutex_create (char *name) { // Create a new mutex with the given name and add it to the list of mutexes
    t_mutex *mutex = malloc(sizeof(t_mutex));
    mutex->name = string_duplicate(name);
    mutex->isLocked = false;
    mutex->lockedBy = NULL;
    mutex->waitingProcesses = list_create();

    pthread_mutex_init(&mutex->internal_mutex, NULL);

    pthread_mutex_lock(&list_mutex_mutex);
    list_add(list_mutexes, mutex);
    pthread_mutex_unlock(&list_mutex_mutex);
    return mutex;
}

void mutex_lock (t_mutex *mutex, t_process *process) { // Lock a mutex for a process, if the mutex is already locked, block the process and add it to the waiting list of the mutex
    pthread_mutex_lock(&mutex->internal_mutex);

    if (!mutex->isLocked) {
        mutex->isLocked = true;
        mutex->lockedBy = process;

        pthread_mutex_unlock(&mutex->internal_mutex);

        log_info(logger, "## (%d) Toma el mutex '%s'", process->pid, mutex->name);

    } else {
        if (mutex->lockedBy == process) { // If the process already owns the mutex, do nothing (avoid deadlock)
            pthread_mutex_unlock(&mutex->internal_mutex);
            log_warning(logger, "## (%d) El proceso %d ya posee el mutex '%s'", process->pid, process->pid, mutex->name);
            return;
        }
        list_add(mutex->waitingProcesses, process);

        pthread_mutex_lock(&scheduler_mutex);
        
        process_set_state(process, BLOCK, logger);
        remove_process_from_list(exec_processes, process);

        t_client_info *cpu = process->cpu;
        
        pthread_mutex_unlock(&scheduler_mutex);
        
        pthread_mutex_unlock(&mutex->internal_mutex);

        evict_process(cpu, MUTEX_LOCKED);
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
        log_warning(logger, "## (%d) intentó liberar el mutex '%s' sin poseerlo", process->pid, mutex->name);
        return;
    }

    uint32_t previous_owner_pid = mutex->lockedBy->pid;

    if (!list_is_empty(mutex->waitingProcesses)) {
        t_process *next_process = list_remove(mutex->waitingProcesses, 0);
        mutex->isLocked = true;
        mutex->lockedBy = next_process;

        pthread_mutex_lock(&scheduler_mutex);
        process_set_state(next_process, READY, logger);
        add_process_to_ready_queue(next_process);
        
        pthread_mutex_unlock(&scheduler_mutex);
        
        pthread_mutex_unlock(&mutex->internal_mutex);

        sem_post(&short_term_scheduler_sem);

        log_info(logger, "## (%d) Libera el Mutex '%s' y se asigna al proceso %d", previous_owner_pid, mutex->name, next_process->pid);

    } else {
        mutex->isLocked = false;
        mutex->lockedBy = NULL;

        pthread_mutex_unlock(&mutex->internal_mutex);

        log_info(logger, "## (%d) Libera el Mutex '%s' ", previous_owner_pid, mutex->name);
    }
}

t_mutex* get_mutex_by_name (char *name) { // Get a mutex from the list of mutexes based on its name, returns NULL if not found
    bool _mutex_name_coincides (void *ptr) {
        t_mutex *m = (t_mutex*)ptr;
        return string_equals_ignore_case(m->name, name);
    }

    pthread_mutex_lock(&list_mutex_mutex);
    t_mutex *mutex = list_find(list_mutexes, _mutex_name_coincides);
    pthread_mutex_unlock(&list_mutex_mutex);

    return mutex;
}

void destroy_list_of_mutexes (t_list *list) { // Destroys a list of mutexes, freeing their memory and destroying their internal mutexes
    void _destroy_mutex(void *ptr) {
        t_mutex *mutex = (t_mutex*)ptr;
        pthread_mutex_destroy(&mutex->internal_mutex);
        list_destroy(mutex->waitingProcesses);
        free(mutex->name);
        free(mutex);
    }

    list_destroy_and_destroy_elements(list, _destroy_mutex);
}
