/**********************************************************
This disk management designed to serve the disk queue. the 
member of queue is pcb. 

The first entry of queue is currently processing Disk operation one
FIFO priority

Lock address is base + diskId to different disk.
***********************************************************/
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"

extern LinkedPcb diskArray[8];

/************************************************************************
DISK_IS_EMPTY
The requested disk is empty or not, determine it by checking whether the its'
disk queue is emtpy or not.

 ************************************************************************/
 BOOL diskIsEmpty(long diskId){
     LinkedPcb diskQ;
     diskQ = diskArray[diskId];
    if (diskQ->next == diskQ && diskQ->prev == diskQ){
        return TRUE;
    }
    return FALSE;
}

// add pcb to the tail of disk queue
void addToDiskQueue(PcbNode *pcb, BOOL writeRequest){
    // first entry of disk queue
    INT32 *LockResult1;
    READ_MODIFY(MEMORY_INTERLOCK_BASE + pcb->diskId, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
    LinkedPcb diskQ = diskArray[pcb->diskId];
    if (pcb->fromMem == TRUE){
        pcb->next = diskQ->next;
        diskQ->next->prev = pcb;
        diskQ->next = pcb;
        pcb->prev = diskQ;
        beginDisk(pcb->diskId);
    } else {
        diskQ->prev->next = pcb;
        pcb->prev = diskQ->prev;
        diskQ->prev = pcb;
        pcb->next = diskQ;
        if (writeRequest == TRUE){
            pcb->state = WAITING_DISK_WRITE;
        } else {
            pcb->state = WAITING_DISK_READ;
        }
        beginDisk(pcb->diskId);
    }
	// first entry of timer queue
	
    READ_MODIFY(MEMORY_INTERLOCK_BASE + pcb->diskId, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
        &LockResult1);
}
// dequeue from disk queue
PcbNode *dequeueFromDiskQueue(long diskId){
    INT32 *LockResult1;
    READ_MODIFY(MEMORY_INTERLOCK_BASE + diskId, DO_LOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
    LinkedPcb diskQ = diskArray[diskId];    
    if (diskIsEmpty(diskId) == TRUE){
		// printf("nothing in Disk queue!");
		return NULL;
	}
    PcbNode *ans;
	ans = diskQ->next;
    diskQ->next = ans->next;
    ans->next->prev = diskQ;
    ans->next = NULL;
    ans->prev = NULL;
    ans->fromMem = FALSE;
    beginDisk(diskId);
    READ_MODIFY(MEMORY_INTERLOCK_BASE + diskId, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
		&LockResult1);
	return ans;
}
// check the first item of disk queue but not pop it up
PcbNode *peekDiskQ(long diskId){
    LinkedPcb diskQ;
    diskQ = diskArray[diskId];
    return diskQ->next;
}
// make disk to work
void beginDisk(long diskId){
    LinkedPcb diskQ = diskArray[diskId];
    if (diskIsEmpty(diskId) == TRUE){
        return;
    }
    MEMORY_MAPPED_IO mmio;
    PcbNode *temp;
    temp = peekDiskQ(diskId);
    if (temp->state == WAITING_DISK_READ){
        temp->state = DISK_IS_READING;
        mmio.Mode = Z502DiskRead;
        mmio.Field1 = temp->diskId;
        mmio.Field2 = temp->diskSector;
        mmio.Field3 = (long)temp->container;
        MEM_WRITE(Z502Disk, &mmio);
    } else if(temp->state == WAITING_DISK_WRITE){
        temp->state = DISK_IS_WRITING;
        mmio.Mode = Z502DiskWrite;
        mmio.Field1 = temp->diskId;
        mmio.Field2 = temp->diskSector;
        mmio.Field3 = (long) temp->container;
        MEM_WRITE(Z502Disk, &mmio);
    }
}
// system_call function 
void readDisk(PcbNode **pcb, long sector, long diskId, byte *output){
    (*pcb)->diskId = diskId;
    (*pcb)->diskSector = sector;
    (*pcb)->container = output;
    addToDiskQueue(*pcb, FALSE);
    (*pcb) = NULL;
    dispatcher();
}
// system_call function 
void writeDisk(PcbNode **pcb, long sector, long diskId, byte *input){
    (*pcb)->diskId = diskId;
    (*pcb)->diskSector = sector;
    (*pcb)->container = input;
    addToDiskQueue(*pcb, TRUE);
    (*pcb) = NULL;
    dispatcher();
}

void memReadDisk(PcbNode **pcb, long sector, long diskId, byte *output){
    (*pcb)->fromMem = TRUE;
    (*pcb)->diskId = diskId;
    (*pcb)->diskSector = sector;
    (*pcb)->container = output;
    addToDiskQueue(*pcb, FALSE);
    (*pcb) = NULL;
    dispatcher();
}
void memWriteDisk(PcbNode **pcb, long sector, long diskId, byte *input){
    (*pcb)->fromMem = TRUE;    
    (*pcb)->diskId = diskId;
    (*pcb)->diskSector = sector;
    (*pcb)->container = input;
    addToDiskQueue(*pcb, TRUE);
    (*pcb) = NULL;
    dispatcher();
}
void dispatcherForMem(){
    long *errorCode;
	if (anyMore() == FALSE){
		// no more process, halt machine
		osTerminateProcess(-2l, &errorCode);
		return;
    }
    MEMORY_MAPPED_IO mmio;
    mmio.Mode = Z502Action;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_WRITE(Z502Idle, &mmio);
}