#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"
extern LinkedPcb susQ;
extern LinkedPcb wholePcb[25];
void addToSuspendQ(PcbNode *pcb){
    // INT32 *LockResult1;
    // READ_MODIFY(MEMORY_INTERLOCK_BASE + 19, DO_LOCK, SUSPEND_UNTIL_LOCKED,
	// 	&LockResult1);

	susQ->prev->next = pcb;
    pcb->prev = susQ->prev;
	susQ->prev = pcb;
    pcb->next = susQ;

    // READ_MODIFY(MEMORY_INTERLOCK_BASE + 19, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
    //     &LockResult1);
}
PcbNode *dequeueFromSuspendtQueueByPid(int pid){
    PcbNode *ans;
    ans = wholePcb[pid];
    ans->next->prev = ans->prev;
    ans->prev->next = ans->next;
    ans->next = NULL;
    ans->prev = NULL;

	return ans;
}