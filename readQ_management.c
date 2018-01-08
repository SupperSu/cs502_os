/**********************************************************
This ready_management is to serve for mainly ready queue, 
includes function enqueue or dequeue based on PRIORITY.

Lock Address is base + 12
***********************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"

extern LinkedPcb readyQ;
int countOfReadyQ = 0;
/************************************************************************
 ADD_TO_READY_QUEUE
 Go through the Ready queue find the place to insert into ready queue without
 consideration of priority
 ************************************************************************/
void addToReadyQueue(PcbNode *pcb){
	INT32 *LockResult1;
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	if (pcb->next != NULL || pcb->prev != NULL){
		pcb->next->prev = pcb->prev;
		pcb->prev->next = pcb->next;
		pcb->next = NULL;
		pcb->prev = NULL;
	}
	if (pcb->fromMem == TRUE){
		pcb->next = readyQ->next;
		readyQ->next->prev = pcb;
		readyQ->next = pcb;
		pcb->prev = readyQ;
		countOfReadyQ++;		
	} else {
		pcb->state = READY;
		// first entry of timer queue
		readyQ->prev->next = pcb;
		pcb->prev = readyQ->prev;
		readyQ->prev = pcb;
		pcb->next = readyQ;
		countOfReadyQ++;
	}
	
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	// while(temp!= NULL && pcb->priority < temp->priority){
	// 	temp = temp->next;
	// }
}
/************************************************************************
 DEQUEUE_FROM_READY_QUEUE
 dequeue the LOWEST PRIORITY pcb, and return it
 ************************************************************************/
PcbNode *dequeueFromReadyQueue(){
	INT32 *LockResult1;
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	// first entry of timer queue
	if (nothingInReadyQ() == TRUE){
		// printf("nothing in ready queue!");
		return NULL;
	}
	PcbNode *ans;
	ans = readyQ->next;
	readyQ->next = ans->next;
	ans->next->prev = readyQ;
	countOfReadyQ--;		
	ans->next = NULL;
	ans->prev = NULL;
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 12, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	return ans;
}

BOOL nothingInReadyQ(){	
	if (readyQ->next == readyQ && readyQ->prev == readyQ){
		return TRUE;
	}
	return FALSE;
}