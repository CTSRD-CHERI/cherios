#define MAX_MSG 16

typedef size_t msg_nb_t;

typedef struct
{
	register_t a0; /* GP arguments */
	register_t a1;
	register_t a2;

	void * c3; /* cap arguments */
	void * c4;
	void * c5;

	register_t v0;  /* method nb */
	void *     idc; /* identifier */
	void *     c1;  /* sync token */
}  msg_t;

typedef struct
{
	msg_nb_t start;
	msg_nb_t end;
	msg_nb_t len;
	msg_t msg[MAX_MSG];
}  queue_t;
