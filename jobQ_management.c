/**********************************************************
Serve to wholePcb array, offer some basic operation of 
finding a pcb, checking existing pcb by name, kill a pcb e.t.c
***********************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"

extern LinkedPcb wholePcb[25];

// insert on tail of queue
void addToJobQueue(PcbNode *pcb){
    // first entry of job queue
    wholePcb[pcb->processId] = pcb;
}
// determine any more processes in machine which is not terminated
BOOL anyMore(){
    for (int i = 0; i < WHOLE_PCB_LEN; i++){
        if (wholePcb[i] == NULL)continue;
        if (wholePcb[i]->state != TERMINATED)return TRUE;
    }
    return FALSE;
}
// check existing process by its' name
BOOL checkExistByName(char *name){
    for (int i = 0; i < WHOLE_PCB_LEN; i++){
        if (wholePcb[i] == NULL)continue;
        if (strcmp(wholePcb[i]->processName, name) == 0){
            return TRUE;
        }
    }
    return FALSE;
}
// find the pcb which is input process id
PcbNode *findPCB(long pId, long *errorCode, PcbNode *output){
    if (pId >= WHOLE_PCB_LEN){
        // printf("pId is larger than WHOLE_PCB_LEN");
        return NULL;
    }
    if (wholePcb[pId] == NULL){
        printf("no this PCB");
        *errorCode = ERR_PROCESS_NOT_EXIST;
        return NULL;
    }
    output = (PcbNode *)wholePcb[pId];
    return wholePcb[pId];
}
// terminate a pcb
void killPCB(long pId, long *errorCode){
    INT32 *LockResult1;
    READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
    PcbNode *temp;
    temp = findPCB(pId, errorCode, temp);
    if (temp == NULL){
        // printf("the kill require of %d PID is not valid");
        return;
    }
    temp->state = TERMINATED;
    temp->next->prev = temp->prev;
    temp->prev->next = temp->next;
    temp->next = NULL;
    temp->prev = NULL;
    *errorCode = ERR_SUCCESS;
    READ_MODIFY(MEMORY_INTERLOCK_BASE + 1, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
}
