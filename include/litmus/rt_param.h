/*
 * Definition of the scheduler plugin interface.
 *
 */
#ifndef _LINUX_RT_PARAM_H_
#define _LINUX_RT_PARAM_H_

/* Litmus time type. */
typedef unsigned long long lt_t;

static inline int lt_after(lt_t a, lt_t b)
{
	return ((long long) b) - ((long long) a) < 0;
}
#define lt_before(a, b) lt_after(b, a)

static inline int lt_after_eq(lt_t a, lt_t b)
{
	return ((long long) a) - ((long long) b) >= 0;
}
#define lt_before_eq(a, b) lt_after_eq(b, a)

/* different types of clients */
typedef enum {
	RT_CLASS_HARD,
	RT_CLASS_SOFT,
	RT_CLASS_BEST_EFFORT
} task_class_t;

typedef enum {
	NO_ENFORCEMENT,      /* job may overrun unhindered */
	QUANTUM_ENFORCEMENT, /* budgets are only checked on quantum boundaries */
	PRECISE_ENFORCEMENT  /* budgets are enforced with hrtimers */
} budget_policy_t;

/* budget draining policy (ignored if neither budget enforcement nor
   signalling are used). */
typedef enum {
	DRAIN_SIMPLE,	/* drains while task is linked */
	DRAIN_SIMPLE_IO, /* drains while task is linked or blocked
						(not waiting for Litmus lock) */
	DRAIN_SAWARE,	/* drains according to suspension-aware analysis */
	DRAIN_SOBLIV	/* drains according to suspension-obliv analysis */
} budget_drain_policy_t;

/* signal policy for budget exhaustion */
typedef enum {
	NO_SIGNALS,		/* job receives no signals when it exhausts its budget */
	QUANTUM_SIGNALS, /*budget signals are only sent on quantum boundaries */
	PRECISE_SIGNALS	/* budget signals are triggered with hrtimers */
} budget_signal_policy_t;

/* Release behaviors for jobs. PERIODIC and EARLY jobs
   must end by calling sys_complete_job() (or equivalent)
   to set up their next release and deadline. */
typedef enum {
	/* Jobs are released sporadically (provided job precedence
       constraints are met). */
	TASK_SPORADIC,

	/* Jobs are released periodically (provided job precedence
       constraints are met). */
	TASK_PERIODIC,

    /* Jobs are released immediately after meeting precedence
       constraints. Beware this can peg your CPUs if used in
       the wrong applications. Only supported by EDF schedulers. */
	TASK_EARLY
} release_policy_t;

/* Real-time behaviors of forked threads that are not explicitly real-time. */
typedef enum {
	AUX_ENABLE  = 0x1,	/* Priority of non-rt (aux) threads inherit from max
						   of suspended real-time thread within the process. */
	AUX_CURRENT = (AUX_ENABLE<<1),	/* All current non-rt threads become
									   aux threads. */
	AUX_FUTURE  = (AUX_CURRENT<<1)	/* Any non-rt threads forked in the future
									   automatically become aux threads. */
} auxiliary_thread_flags_t;

/* We use the common priority interpretation "lower index == higher priority",
 * which is commonly used in fixed-priority schedulability analysis papers.
 * So, a numerically lower priority value implies higher scheduling priority,
 * with priority 1 being the highest priority. Priority 0 is reserved for
 * priority boosting. LITMUS_MAX_PRIORITY denotes the maximum priority value
 * range.
 */

#define LITMUS_MAX_PRIORITY     512
#define LITMUS_HIGHEST_PRIORITY   1
#define LITMUS_LOWEST_PRIORITY    (LITMUS_MAX_PRIORITY - 1)

/* Provide generic comparison macros for userspace,
 * in case that we change this later. */
#define litmus_higher_fixed_prio(a, b)	(a < b)
#define litmus_lower_fixed_prio(a, b)	(a > b)
#define litmus_is_valid_fixed_prio(p)		\
	((p) >= LITMUS_HIGHEST_PRIORITY &&	\
	 (p) <= LITMUS_LOWEST_PRIORITY)

struct rt_task {
	lt_t 		exec_cost;
	lt_t 		period;
	lt_t		relative_deadline;
	lt_t		phase;
	unsigned int	cpu;
	unsigned int	priority;
	task_class_t	cls;
	budget_policy_t  budget_policy;  /* ignored by pfair */
	budget_drain_policy_t drain_policy;
	budget_signal_policy_t budget_signal_policy; /* ignored by pfair */
	release_policy_t release_policy;
};

union np_flag {
	uint32_t raw;
	struct {
		/* Is the task currently in a non-preemptive section? */
		uint32_t flag:31;
		/* Should the task call into the scheduler? */
		uint32_t preempt:1;
	} np;
};

struct affinity_observer_args
{
	int lock_od;
};

struct gpu_affinity_observer_args
{
	struct affinity_observer_args obs;
	unsigned int replica_to_gpu_offset;
	unsigned int rho;
	int relaxed_rules;
};

#define R2DGLP_M_IN_FIFOS (0u)
#define R2DGLP_UNLIMITED_IN_FIFOS (~0u)
#define R2DGLP_OPTIMAL_FIFO_LEN (0u)
#define R2DGLP_UNLIMITED_FIFO_LEN (~0u)

struct r2dglp_args
{
	unsigned int nr_replicas;
	unsigned int max_in_fifos;
	unsigned int max_fifo_len;
};

/* The definition of the data that is shared between the kernel and real-time
 * tasks via a shared page (see litmus/ctrldev.c).
 *
 * WARNING: User space can write to this, so don't trust
 * the correctness of the fields!
 *
 * This servees two purposes: to enable efficient signaling
 * of non-preemptive sections (user->kernel) and
 * delayed preemptions (kernel->user), and to export
 * some real-time relevant statistics such as preemption and
 * migration data to user space. We can't use a device to export
 * statistics because we want to avoid system call overhead when
 * determining preemption/migration overheads).
 */
struct control_page {
	/* This flag is used by userspace to communicate non-preempive
	 * sections. */
	volatile __attribute__ ((aligned (8))) union np_flag sched;

	/* Incremented by the kernel each time an IRQ is handled. */
	volatile __attribute__ ((aligned (8))) uint64_t irq_count;

	/* Locking overhead tracing: userspace records here the time stamp
	 * and IRQ counter prior to starting the system call. */
	uint64_t ts_syscall_start;  /* Feather-Trace cycles */
	uint64_t irq_syscall_start; /* Snapshot of irq_count when the syscall
				     * started. */

	/* to be extended */
};

/* Expected offsets within the control page. */

#define LITMUS_CP_OFFSET_SCHED		0
#define LITMUS_CP_OFFSET_IRQ_COUNT	8
#define LITMUS_CP_OFFSET_TS_SC_START	16
#define LITMUS_CP_OFFSET_IRQ_SC_START	24


/* sched trace event injection */
/* mirror of st_event_record_type_t
 * Assume all are UNsupported, unless otherwise stated. */
typedef enum {
	ST_INJECT_NAME = 1,             /* supported */
	ST_INJECT_PARAM,                /* supported */
	ST_INJECT_RELEASE,              /* supported */
	ST_INJECT_ASSIGNED,
	ST_INJECT_SWITCH_TO,
	ST_INJECT_SWITCH_AWAY,
	ST_INJECT_COMPLETION,           /* supported */
	ST_INJECT_BLOCK,
	ST_INJECT_RESUME,
	ST_INJECT_ACTION,               /* supported */
	ST_INJECT_SYS_RELEASE,          /* supported */

	ST_INJECT_MIGRATION = 21,       /* supported */
} sched_trace_injection_events_t;

struct st_inject_args {
	union {
		/* ST_INJECT_RELEASE, ST_INJECT_COMPLETION */
		struct {
			lt_t release;
			lt_t deadline;
			unsigned int job_no;
		};

		/* ST_INJECT_ACTION */
		unsigned int action;

		/* ST_INJECT_MIGRATION */
		struct {
			unsigned int from;
			unsigned int to;
		};
	};
};

/* don't export internal data structures to user space (liblitmus) */
#ifdef __KERNEL__

#include <linux/semaphore.h>
#include <litmus/binheap.h>
#include <litmus/budget.h>

#ifdef CONFIG_LITMUS_SOFTIRQD
#include <linux/interrupt.h>
#endif

#if defined(CONFIG_LITMUS_NVIDIA) && defined(CONFIG_LITMUS_AFFINITY_LOCKING)
/*** GPU affinity tracking structures ***/
typedef enum gpu_migration_dist
{
	MIG_LOCAL	= 0,
	MIG_NEAR	= 1,
	MIG_MED		= 2,
	MIG_FAR		= 3, /* assumes 8 GPU binary tree hierarchy */
	MIG_NONE	= 4,

	MIG_LAST = MIG_NONE
} gpu_migration_dist_t;

#if 0
typedef struct feedback_est
{
	fp_t est;
	fp_t accum_err;
} feedback_est_t
#endif

#define AVG_EST_WINDOW_SIZE 20
typedef struct avg_est {
	lt_t history[AVG_EST_WINDOW_SIZE];
	uint16_t count;
	uint16_t idx;
	lt_t sum;
	lt_t avg;
	lt_t std;
} avg_est_t;
#endif /* end LITMUS_NVIDIA && LITMUS_AFFINITY_LOCKING */

#ifdef CONFIG_LITMUS_AFFINITY_LOCKING
typedef int (*notify_rsrc_exit_t)(struct task_struct* tsk);
#endif /* end LITMUS_AFFINITY_LOCKING */

#ifdef CONFIG_LITMUS_SOFTIRQD
/* klmirqd (real-time threaded interrupt) thread data */
struct klmirqd_info
{
	struct task_struct*	klmirqd;
	unsigned int	terminating:1;

	raw_spinlock_t	lock;
	u32			pending;
	atomic_t	num_hi_pending;
	atomic_t	num_low_pending;
	atomic_t	num_work_pending;

	struct tasklet_head	pending_tasklets_hi;
	struct tasklet_head	pending_tasklets;
	struct list_head	worklist;

	struct list_head	klmirqd_reg;

	struct completion*	exited;
};
#endif

struct _rt_domain;
struct bheap_node;
struct release_heap;

struct rt_job {
	/* Time instant the the job was or will be released.  */
	lt_t	release;

	/* What is the current deadline? */
	lt_t   	deadline;

	/* How much service has this job received so far? */
	lt_t	exec_time;

	/* By how much did the prior job miss its deadline by?
	 * Value differs from tardiness in that lateness may
	 * be negative (when job finishes before its deadline).
	 */
	long long	lateness;

	/* Which job is this. This is used to let user space
	 * specify which job to wait for, which is important if jobs
	 * overrun. If we just call sys_sleep_next_period() then we
	 * will unintentionally miss jobs after an overrun.
	 *
	 * Increase this sequence number when a job is released.
	 */
	unsigned int    job_no;

	/* Increments each time a job is forced to complete by budget exhaustion.
	 * If a job completes without remaining budget, the next ob will be early-
	 * released __without__ pushing back its deadline. job_backlog is
	 * decremented once per early release. This behavior continues until
	 * backlog == 0.
	 */
	unsigned int	backlog;

	/* Denotes if the current job is a backlogged job that was early released
	 * due to budget enforcement behaviors.
	 */
	unsigned int	is_backlogged_job:1;
};

struct pfair_param;

/*	RT task parameters for scheduling extensions
 *	These parameters are inherited during clone and therefore must
 *	be explicitly set up before the task set is launched.
 */
struct rt_param {
	/* Generic flags available for plugin-internal use. */
	unsigned int 		flags:8;

	/* do we need to check for srp blocking? */
	unsigned int		srp_non_recurse:1;

	/* is the task present? (true if it can be scheduled) */
	unsigned int		present:1;

	/* has the task completed? */
	unsigned int		completed:1;

#ifdef CONFIG_LITMUS_NVIDIA
	long unsigned int	held_gpus;		/* bitmap of held GPUs. */
	struct binheap_node	gpu_owner_node;	/* just one GPU for now... */
	unsigned int		hide_from_gpu:1;

#ifdef CONFIG_LITMUS_AFFINITY_LOCKING
	avg_est_t		gpu_migration_est[MIG_LAST+1];
	gpu_migration_dist_t	gpu_migration;
	int			last_gpu;
	lt_t			accum_gpu_time;
	lt_t			gpu_time_stamp;
	unsigned int		suspend_gpu_tracker_on_block:1;
#endif /* end LITMUS_AFFINITY_LOCKING */
#endif /* end LITMUS_NVIDIA */

#ifdef CONFIG_LITMUS_AFFINITY_LOCKING
	notify_rsrc_exit_t  rsrc_exit_cb;
	void* rsrc_exit_cb_args;
#endif

#ifdef CONFIG_LITMUS_LOCKING
	/* Is the task being priority-boosted by a locking protocol? */
	unsigned int		priority_boosted:1;
	/* If so, when did this start? */
	lt_t			boost_start_time;

	/* How many LITMUS^RT locks does the task currently hold/wait for? */
	unsigned int		num_locks_held;
	/* How many PCP/SRP locks does the task currently hold/wait for? */
	unsigned int		num_local_locks_held;
#endif

#ifdef CONFIG_LITMUS_NESTED_LOCKING
	raw_spinlock_t			hp_blocked_tasks_lock;
	struct binheap			hp_blocked_tasks;

	/* pointer to lock upon which is currently blocked */
	struct litmus_lock* blocked_lock;
	unsigned long	blocked_lock_data;

	struct litmus_lock* outermost_lock;
	unsigned int	virtually_unlocked:1;

	/* wait-queue entry for pending wakeups */
	wait_queue_t	wait;
#endif

	/* user controlled parameters */
	struct rt_task 		task_params;

	/* timing parameters */
	struct rt_job 		job_params;

	/* Should the next job be released at some time other than
	 * just period time units after the last release?
	 */
	unsigned int		sporadic_release:1;
	lt_t			sporadic_release_time;

	/* budget tracking/enforcement method and data assigned to this task */
	struct budget_tracker	budget;

	/* task representing the current "inherited" task
	 * priority, assigned by inherit_priority and
	 * return priority in the scheduler plugins.
	 * could point to self if PI does not result in
	 * an increased task priority.
	 */
	 struct task_struct*	inh_task;

	 /* budget enforcement methods may require knowledge of tasks that
	  * inherit this task's priority. There may be more than one such
	  * task w/ priority inheritance chains.
	  */
	 int	inh_task_linkback_idx;	/* idx in inh_task's
									   inh_task_linkbacks array. */

	 struct task_struct** inh_task_linkbacks; /* array w/ BITS_PER_LONG elm */
	 unsigned long	used_linkback_slots;  /* nr used slots
											 in inh_task_linkbacks */

#ifdef CONFIG_NP_SECTION
	/* For the FMLP under PSN-EDF, it is required to make the task
	 * non-preemptive from kernel space. In order not to interfere with
	 * user space, this counter indicates the kernel space np setting.
	 * kernel_np > 0 => task is non-preemptive
	 */
	unsigned int	kernel_np;
#endif

	/* This field can be used by plugins to store where the task
	 * is currently scheduled. It is the responsibility of the
	 * plugin to avoid race conditions.
	 *
	 * This used by GSN-EDF and PFAIR.
	 */
	volatile int		scheduled_on;

	/* Is the stack of the task currently in use? This is updated by
	 * the LITMUS core.
	 *
	 * Be careful to avoid deadlocks!
	 */
	volatile int		stack_in_use;

	/* This field can be used by plugins to store where the task
	 * is currently linked. It is the responsibility of the plugin
	 * to avoid race conditions.
	 *
	 * Used by GSN-EDF.
	 */
	volatile int		linked_on;

	/* PFAIR/PD^2 state. Allocated on demand. */
	struct pfair_param*	pfair;

	/* Fields saved before BE->RT transition.
	 */
	int old_policy;
	int old_prio;

	/* ready queue for this task */
	struct _rt_domain* domain;

	/* heap element for this task
	 *
	 * Warning: Don't statically allocate this node. The heap
	 *          implementation swaps these between tasks, thus after
	 *          dequeuing from a heap you may end up with a different node
	 *          then the one you had when enqueuing the task.  For the same
	 *          reason, don't obtain and store references to this node
	 *          other than this pointer (which is updated by the heap
	 *          implementation).
	 */
	struct bheap_node*	heap_node;
	struct release_heap*	rel_heap;

	/* Used by rt_domain to queue task in release list.
	 */
	struct list_head list;

	/* Pointer to the page shared between userspace and kernel. */
	struct control_page * ctrl_page;

#ifdef CONFIG_LITMUS_SOFTIRQD
	/* proxy threads have minimum priority by default */
	unsigned int	is_interrupt_thread:1;

	/* pointer to data used by klmirqd thread.
	   valid only if ptr only valid if is_interrupt_thread == 1 */
	struct klmirqd_info* klmirqd_info;
#endif /* end LITMUS_SOFTIRQD */

#ifdef CONFIG_REALTIME_AUX_TASKS
	/* Real-time data for auxiliary tasks */
	struct list_head	aux_task_node;
	struct binheap_node	aux_task_owner_node;

	unsigned int	is_aux_task:1;
	unsigned int	aux_ready:1;
	unsigned int	has_aux_tasks:1;
	unsigned int	hide_from_aux_tasks:1;
#endif
};

#ifdef CONFIG_REALTIME_AUX_TASKS
/* Auxiliary task data. Appears in task_struct, like rt_param */
struct aux_data {
	struct list_head	aux_tasks;
	struct binheap	aux_task_owners;
	unsigned int	initialized:1;
	unsigned int	aux_future:1;
};
#endif

#endif /* __KERNEL */

#endif
