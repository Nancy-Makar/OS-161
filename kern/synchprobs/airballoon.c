/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 8
#define NROPES 16
static int ropes_left = NROPES;

int volatile done = 0;

/* Data structures for rope mappings */
typedef struct Rope
{
	//struct lock *rope_lock;
	struct semaphore *rope_sem;
	volatile int is_severed;
	volatile int stake_number;
} Rope;

typedef struct Stake
{
	volatile int rope_index;
	struct semaphore *stake_sem;
} Stake;




/* Implement this! */

static Rope ropes_arr[NROPES];
static Stake curr_stakes[NROPES];


/* Synchronization primitives */

/* Implement this! */

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */

/*
* The design consists of 2 structs, one representing the rope nad another the ground stakes
* Dandelion access the ropes via hooks as they have the same index
* Marigold access the ropes via stakes by having the rope number tied to it and the rope who's number is the number store by
* the stake structure, its structure has the index of the stake.
* ecah stake and each rope has a semaphore
* The semaphore of each rope blocks during severing or swapping that specific rope in all three thread dandelion, marigold and flowerkiller
* The semaphore of each stake blocks the 2 specific stakes to be swapped by flowerkiller
* Lord Flowerkiller swaps ropes by both changing the rope index in each stake and the stake number in each rope.
*/
static void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Dandelion thread starting\n");
	/* Implement this function */

	while (ropes_left > 0)
	{
		int hook_index = random() % NROPES;
		struct Rope *held_rope = &ropes_arr[hook_index];

		P(ropes_arr[hook_index].rope_sem);  //the sem block of the rope correspondind to the indexed hook
		if (!ropes_arr[hook_index].is_severed) //checks is the thread is severed, if not sever and yeild. If severed just increment sem count.
		{

			held_rope->is_severed = 1; //mark the thread as severed 

			ropes_left--;
			kprintf("Dandelion severed rope %d \n", hook_index);
			V(ropes_arr[hook_index].rope_sem);
			thread_yield(); //thread yeild after severing a rope
		}
		else
		{
			V(ropes_arr[hook_index].rope_sem);
		}
	}

	if (ropes_left <= 0) //thread will exit if no more unsevered ropes
	{
		kprintf("Dandelion thread done \n");
		thread_exit();
	}
}

static void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");
	/* Implement this function */


	while(ropes_left > 0){
		int stake_number = random() % NROPES;
		int n = curr_stakes[stake_number].rope_index;

		struct Rope *held_rope = &ropes_arr[n];
		P(ropes_arr[n].rope_sem); //the sem block of the rope correspondind to the indexed stake
		if (!ropes_arr[n].is_severed) //checks is the thread is severed, if not sever and yeild. If severed just increment sem count.
		{
			held_rope->is_severed = 1; //mark the thread as severed
			ropes_left--; //decrement the number of threads left
			kprintf("Marigold severed rope %d from stake %d\n", n, ropes_arr[n].stake_number);
			V(ropes_arr[n].rope_sem);
			thread_yield(); //thread yeild after severing a rope
		}
		else
		{
			V(ropes_arr[n].rope_sem);
		}
	}

		if (ropes_left <= 0) //thread will exit if no more unsevered ropes
	{
		kprintf("Marigold thread done \n");
		thread_exit();
	}
}

static void
flowerkiller(void *p, unsigned long arg) //not a big lock solution, just 4 semaphores used, 2 for ropes and 2 for stakes
{
	(void)p;
	(void)arg;
	

	kprintf("Lord FlowerKiller thread starting\n");
	/* Implement this function */

	while (ropes_left > 1)
	{

		if(ropes_left <= 1){
			kprintf("Lord FlowerKiller thread done\n");
			thread_exit();
		}
		int stake_1 = random() % 16;
		int stake_2 = random() % 16;

		P(curr_stakes[stake_1].stake_sem); //block the stake that has an index stake 1
		if (ropes_left <= 1)  //The thread will exit if not enough ropes to swap, means 1 thread remaining
		{
			kprintf("Lord FlowerKiller thread done\n");
			thread_exit();
		}
		P(curr_stakes[stake_2].stake_sem); //block the stake that has an index stake 2

		int n1 = curr_stakes[stake_1].rope_index; //get the rope number of the rope tied to the stake at index stake 1
		int n2 = curr_stakes[stake_2].rope_index; //get the rope number of the rope tied to the stake at index stake 2

		P(ropes_arr[n1].rope_sem); // block the rope to be swapped to check if it is severed and keep it block so that it does
									//not gte severed in the middle of swapping

		if ((!ropes_arr[n1].is_severed))
		{
			P(ropes_arr[n2].rope_sem); //block the other rope to be swapped to check if it is severed
			if ((!ropes_arr[n2].is_severed))
			{
				curr_stakes[stake_1].rope_index = n2; //for the stake the rope is to be tied to, change the stake number it holds
				ropes_arr[n1].stake_number = stake_2; //chnage the stake number of the first rope rope
				kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", n1, stake_1, stake_2);

				curr_stakes[stake_2].rope_index = n1; //for the stake the second rope is to be tied to, change the stake number it holds
				ropes_arr[n2].stake_number = stake_1; //swap the stake number of the second rope
				kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", n2, stake_2, stake_1);
				V(ropes_arr[n1].rope_sem);
				V(curr_stakes[stake_2].stake_sem); //stop blocking and yieldd if a successful swap happened
				V(curr_stakes[stake_1].stake_sem);
				V(ropes_arr[n2].rope_sem);
				thread_yield();
				
			}
			else{
				V(ropes_arr[n2].rope_sem);
				V(ropes_arr[n1].rope_sem);
				V(curr_stakes[stake_2].stake_sem); //stop blocking but do not yield
				V(curr_stakes[stake_1].stake_sem);
				
			}
		}
		else
		{
			V(ropes_arr[n1].rope_sem);
			V(curr_stakes[stake_2].stake_sem); //stop blocking but do not yield
			V(curr_stakes[stake_1].stake_sem);
		}
	}
		
		kprintf("Lord Flowerkiller thread done\n");
		thread_exit();  //exit the thread when out of the while loop , meaning only one rope left
}

static void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	while (ropes_left > 0)
	{
		thread_yield(); //thread will keep idling till no more rope remianig
	}

	kprintf("Balloon thread done\n");
	kprintf("All ropes are severed!\n");
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	done = 1;  //when there is no more ropes, balloon sets the variable doen to 1
	thread_exit();

	/* Implement this function */
}

// Change this function as necessary
int airballoon(int nargs, char **args)
{

	for (int i = 0; i < NROPES; i++)
	{
		ropes_arr[i].stake_number = i;
		ropes_arr[i].is_severed = 0;
		ropes_arr[i].rope_sem = sem_create("ropes sem", 1);
		curr_stakes[i].rope_index = i;
		curr_stakes[i].stake_sem = sem_create("stakes sem", 1);
	}

	int err = 0, i;

	(void)nargs;
	(void)args;
	(void)ropes_left;

	err = thread_fork("Marigold Thread",
					  NULL, marigold, NULL, 0);
	if (err)
		goto panic;

	err = thread_fork("Dandelion Thread",
					  NULL, dandelion, NULL, 0);
	if (err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++)
	{
		err = thread_fork("Lord FlowerKiller Thread",
						  NULL, flowerkiller, NULL, 0);
		if (err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
					  NULL, balloon, NULL, 0);
	if (err)
		goto panic;

	while(!done){
		thread_yield();
	}

	if(done) //When the balloon thread sets the variable done to 1, the main thread exits
		goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
		  strerror(err));

done:

	kprintf("Main thread done \n");
	return 0;
}