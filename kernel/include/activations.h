#include "queue.h"

typedef size_t aid_t;

typedef struct
{
	uint32_t expected_reply;
}  sync_t;

/*
 * Possible status for an activation
 */
typedef enum status_e
{
	status_waiting,
	status_schedulable,
	status_runnable,
	status_sync_block,
	status_wip
} status_e;

/*
 * Kernel structure for an activation
 */
typedef  struct
{
	aid_t parent;		/* activation that created it */
	aid_t aid;		/* activation id -- redundant with array index */
	queue_t queue;		/* message queue */
	msg_nb_t queue_len;	/* queue len (cannot trust userspace
	                           which has write access to queue) */
	status_e status;	/* Current status */
	sync_t sync_token;	/* Helper for the synchronous CCall mecanism */
	void * act_reference;	/* Sealed reference for the activation */
	void * act_default_id;  /* Default object identifier */
}  proc_t;

extern reg_frame_t	kernel_exception_framep[];
extern reg_frame_t *	kernel_exception_framep_ptr;
extern proc_t		kernel_acts[];
extern aid_t 		kernel_curr_proc;

