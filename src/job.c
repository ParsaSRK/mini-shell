#include "job.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>

static job *head = NULL;
static int pool[MAX_JOBS];

int getId(void) {
    for (int i = 0; i < MAX_JOBS; ++i) {
        if (!pool[i]) {
            pool[i] = 1;
            return i;
        }
    }
    return -1;
}

void free_job(job *j) {
    if (!j) return;
    if (j->id >= 0) pool[j->id] = 0;
    free(j->procs);
    free(j);
}

int update_proc(pid_t pid, int status) {
    for (job *it = head; it; it = it->next) {
        for (int i = 0; i < it->nproc; ++i) {
            if (it->procs[i].pid != pid) continue;

            if (WIFEXITED(status)) {
                it->procs[i].exit_code = WEXITSTATUS(status);
                it->procs[i].term_sig = -1;
                it->procs[i].state = PROC_DONE;
            } else if (WIFSIGNALED(status)) {
                it->procs[i].term_sig = WTERMSIG(status);
                it->procs[i].exit_code = -1;
                it->procs[i].state = PROC_DONE;
            } else if (WIFSTOPPED(status)) {
                it->procs[i].state = PROC_STOP;
                it->procs[i].exit_code = -1;
                it->procs[i].term_sig = -1;
            } else if (WIFCONTINUED(status)) {
                it->procs[i].state = PROC_RUN;
                it->procs[i].exit_code = -1;
                it->procs[i].term_sig = -1;
            } else {
                fprintf(stderr, "update_proc: Unknown process status!\n");
                return -1;
            }
            it->isupd = 1;
            return 0;
        }
    }
    return 1; // Process not found
}

int update_job(job *j) {
    if (!j) return -1;

    if (!j->isupd) return 0;

    int stopped = 0;
    int running = 0;
    for (int i = 0; i < j->nproc; ++i) {
        switch (j->procs[i].state) {
            case PROC_STOP:
                ++stopped;
                break;
            case PROC_RUN:
                ++running;
                break;
            case PROC_DONE:
                break;
            default:
                fprintf(stderr, "job_update: Invalid process state!\n");
                return -1;
        }
    }

    if (running == 0 && stopped == 0)
        j->state = JOB_DONE;
    else if (stopped > 0)
        j->state = JOB_STOPPED;
    else if (running > 0)
        j->state = JOB_RUNNING;

    j->isupd = 0;
    return 0;
}

void update_jobs(void) {
    for (job *it = head; it; it = it->next)
        update_job(it);
}

int add_job(job *j) {
    if (!j) {
        fprintf(stderr, "add_job: Invalid job!\n");
        return -1;
    }
    j->next = head;
    head = j;
    return 0;
}

void remove_zombies(void) {
    job *cur = head;

    // Remove all done heads
    while (cur && cur->state == JOB_DONE) {
        head = cur->next;
        if (cur->isbg) printf("[%d] Done! %d\n", cur->id, cur->pgid);
        free_job(cur);
        cur = head;
    }

    if (!cur) return;

    // Set up the prev pointer
    job *prev = cur;
    cur = cur->next; // Advance cur by one

    // While cur exists, iterate and remove from mid using prev pointer
    while (cur) {
        if (cur->state == JOB_DONE) {
            prev->next = cur->next;
            if (cur->isbg) printf("[%d] Done! %d\n", cur->id, cur->pgid);
            free_job(cur);
            cur = prev->next;
        } else {
            cur = cur->next;
            prev = prev->next;
        }
    }
}

void kill_jobs(void) {
    // Send SIGTERM
    for (job *it = head; it; it = it->next)
        kill(-it->pgid, SIGTERM);

    pid_t pid;
    struct timespec ts;
    ts.tv_nsec = (long) 1e7;
    ts.tv_sec = 0;

    // Wait ~50ms for them to clean up
    for (int i = 0; i < 50; ++i) {
        int status;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) update_proc(pid, status);
        update_jobs();
        if (head == NULL) break;
        nanosleep(&ts, NULL);
    }

    // No more mercy, KILL them :)
    for (job *it = head; it; it = it->next)
        kill(-it->pgid, SIGKILL);

    int status;
    while ((pid = waitpid(-1, &status, 0)) > 0) update_proc(pid, status);
    update_jobs();
    remove_zombies();
}

void print_process_state(proc_state state) {
    switch (state) {
        case PROC_STOP:
            printf("PROC_STOP");
            break;
        case PROC_RUN:
            printf("PROC_RUN");
            break;
        case PROC_DONE:
            printf("PROC_DONE");
            break;
    }
}

void print_job_state(job_state state) {
    switch (state) {
        case JOB_RUNNING:
            printf("JOB_RUNNING");
            break;
        case JOB_STOPPED:
            printf("JOB_STOPPED");
            break;
        case JOB_DONE:
            printf("JOB_DONE");
            break;
    }
}

job *get_job(int id) {
    if (id == -1) return head;
    if (!pool[id]) return NULL;
    for (job *it = head; it != NULL; it = it->next)
        if (it->id == id) return it;
    return NULL;
}

void print_jobs(void) {
    for (job *it = head; it != NULL; it = it->next) {
        printf("[%d] {%d, ", it->id, it->pgid);
        print_job_state(it->state);
        printf("} : ");
        for (int i = 0; i < it->nproc; ++i) {
            printf("{%d, ", it->procs[i].pid);
            print_process_state(it->procs[i].state);
            printf("} ");
        }
        printf("\n");
    }
}
