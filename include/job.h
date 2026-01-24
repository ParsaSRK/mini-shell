#pragma once

#include <unistd.h>

#define MAX_JOBS (1 << 15)

/**
 * @brief Current job state.
 */
typedef enum job_state {
    JOB_RUNNING, ///< Job is running (background or foreground)
    JOB_STOPPED, ///< Job is stopped (Ctrl+Z)
    JOB_DONE ///< Job is done (terminated or exited successfully)
} job_state;

/**
 * @brief Child process state change.
 * Used to update the parent job state.
 */
typedef enum proc_state {
    PROC_STOP, ///< Process stopped (e.g., SIGTSTP)
    PROC_RUN, ///< Process running (default/continued)
    PROC_DONE ///< Process terminated or exited
} proc_state;

/**
 * @brief Child process tracking entry.
 */
typedef struct process {
    pid_t pid; ///< process ID
    proc_state state; ///< process current state;
    int exit_code; ///< valid if state == PROC_DONE and exited normally
    int term_sig; ///< valid if state == PROC_DONE and signaled
} process;

// Declaration for recursive node
typedef struct job job;

/**
 * @brief Job node for tracking child processes.
 * Used to build a linked list of running processes for the jobs table.
 */
typedef struct job {
    int id; ///< Job identifier
    pid_t pgid; ///< Process group ID
    process *procs; ///< Process list of children
    int nproc; ///< Count of subprocesses in this group
    job_state state; ///< Current job state
    int isbg; ///< Is background? (1: true, 0: false)
    int isupd; ///< Is updated recently? (1: true, 0: false)
    job *next; ///< Next job in linked list
} job;

/**
 * @brief generates an ID for job being created
 * @return current job ID
 */
int getId(void);

/**
 * @brief Free a job struct.
 * @param j Pointer to a job.
 */
void free_job(job *j);

/**
 * @brief Update a child process state from a wait status.
 * @param pid PID of the child that changed state.
 * @param status Status returned by waitpid.
 * @return non-zero if failed (internal error).
 */
int update_proc(pid_t pid, int status);

/**
 * @brief Add a job to the jobs list.
 * @param j Job to add (prepended to the list).
 * @return non-zero if failed (internal error).
 */
int add_job(job *j);

/**
 * @brief Recompute job state from child process states.
 * @return non-zero if failed (internal error).
 */
int update_job(job* j);

/**
 * @brief Recompute job states from child process states.
 */
void update_jobs(void);

/**
 * @brief Remove completed jobs from the list and free their memory.
 */
void remove_zombies(void);

/**
 * @brief Used to gracefully terminate remaining jobs,
 * and kill them if they don't.
 */
void kill_jobs(void);

/**
 * @brief Used to print active jobs table. called by 'jobs' command
 */
void print_jobs(void);

/**
 * @brief get job description based on the id
 * @param id Job id
 * @return job pointer (NULL if not exist)
 */
job *get_job(int id);
