/**********************************************************
This timer_management is to serve for mainly timer queue, 
includes function enqueue or dequeue based on WAKE TIME.

Lock address is base + 13
***********************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include 			"my_global.h"

extern LinkedPcb timerQ;
int countOfTimerQ = 0;
/************************************************************************
 ADD_TO_TIMER_QUEUE
 Go through the timer queue find the place to insert into timer queue by comparing
 wake time of process
 ************************************************************************/
void addToTimerQueue(PcbNode *pcb){
	// first entry of timer queue
	INT32 *LockResult1;
	READ_MODIFY(MEMORY_INTERLOCK_BASE+ 13, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	MEMORY_MAPPED_IO mmio;
	pcb->state = WAITING_TIMER;
	
	PcbNode *temp;
	temp = timerQ->next;
	pcb->state = WAITING_TIMER;
	while(temp!= timerQ && pcb->wakeTime > temp->wakeTime){
		temp = temp->next;
	}
	temp->prev->next = pcb;
	pcb->next = temp;
	pcb->prev = temp->prev;
	temp->prev = pcb;
	countOfTimerQ++;
	
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 13, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
}
/************************************************************************
 DEQUEUE_FROM_TIMER_QUEUE
 dequeue the earliest wake up pcb, and return it
 ************************************************************************/
PcbNode *dequeueFromTimerQueue(){
	INT32 *LockResult1;
	READ_MODIFY(MEMORY_INTERLOCK_BASE+ 13, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	// first entry of timer queue
	if (nothingInTimerQ() == TRUE){
		printf("nothing in ready queue!");
		return NULL;
	}
	PcbNode *ans;
	ans = timerQ->next;
	timerQ->next = ans->next;
	ans->next->prev = timerQ;
	countOfTimerQ--;
	ans->next = NULL;
	ans->prev = NULL;
	READ_MODIFY(MEMORY_INTERLOCK_BASE + 13, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	return ans;
}
BOOL nothingInTimerQ(){
	if (timerQ->next == timerQ && timerQ->prev == timerQ){
		return TRUE;
	}
	return FALSE;
}
PcbNode *peekTimerQ(){
	if (nothingInTimerQ() == TRUE){
		printf("nothing in timerQ! while peek it");
		return NULL;
	}
	return timerQ->next;
}
// start the timer for machine with input timeunits
void startTimer(PcbNode **pcb, long timeUnits){
	MEMORY_MAPPED_IO mmio;
	long CurTime;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);
	CurTime = mmio.Field1;
	(*pcb)->wakeTime = timeUnits + CurTime;
	addToTimerQueue(*pcb);
	if (peekTimerQ()->processId == (*pcb)->processId){
		mmio.Mode = Z502Start;
		mmio.Field1 = timeUnits;   
		mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Timer, &mmio);
	}
	*pcb = NULL;
	dispatcher();
}
/************************************************************************
resetTimer

After a pcb dequeued, should resetTimer for TimerQ.
 ************************************************************************/
void resetTimer(){
	if (nothingInTimerQ() == TRUE)return;
	long CurTime;
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);
	CurTime = mmio.Field1;
	PcbNode *temp = peekTimerQ();
	mmio.Mode = Z502Start;
	long sleepUnits;
	if (temp->wakeTime - CurTime < 0){
		sleepUnits = 0;
	} else {
		sleepUnits = temp->wakeTime - CurTime;
	}
	mmio.Field1 = sleepUnits;   // You pick the time units
	mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502Timer, &mmio);
}
void TimerInterruptHandler(){
	long CurTime;
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502Clock, &mmio);
	CurTime = mmio.Field1;
	PcbNode *temp;
	temp = dequeueFromTimerQueue();
	addToReadyQueue(temp);
	// MEM_WRITE(Z502Context, &mmio);  
	resetTimer();
	// mmio.Mode = Z502StartContext;
	// mmio.Field1 =temp->contextId;
	// mmio.Field2 = START_NEW_CONTEXT_ONLY;
	// mmio.Field3 = mmio.Field4 = 0;
	// MEM_WRITE(Z502Context, &mmio);  
}