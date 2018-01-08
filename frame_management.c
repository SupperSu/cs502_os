#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"

// Second-chance (clock) page-replacement algorithm
// record how many free frame assigned.
int usedFrame = 0;
FrameEntry frameTable[64]; 
extern PcbNode *runningProcess;
char tempSwapArea[1020][16];
// int readBackTime = 0;
long my_clock = 63;
extern PcbNode *runningProcess;
INT32 sharedId = 0;
// record which swap area is used
BOOL tempSwapStatus[1020];
// this function give free frame or LRU frame by using clock
void frameFaultHandler(INT32 virtualId){
    BOOL useSwap = FALSE;
    int avaFrameId = getFreeFrame();
    // if no free frame need to replace 
    if (avaFrameId == -1){
        avaFrameId = getAvlFrame();
    }
     // if the requested virtual page has been swapped out, we need to read it back
     int swappedId = runningProcess->swapedFrame[virtualId];     
     if (swappedId != -1){
        char DataWrite[PGSIZE];
        for (int i = 0; i < 16; i++){
            DataWrite[i] = tempSwapArea[swappedId][i];
        }
        Z502WritePhysicalMemory(avaFrameId, DataWrite);
        tempSwapStatus[swappedId] = FALSE;
        runningProcess->swapedFrame[virtualId] = -1;
        useSwap = TRUE;
        printf("read back data  %x from sector %d\n", *(long*)DataWrite, swappedId);
    }    
    runningProcess->pageTable[virtualId] = (UINT16) PTBL_VALID_BIT;
    runningProcess->pageTable[virtualId] += avaFrameId;
    frameTable[avaFrameId].virtualId = virtualId;
    frameTable[avaFrameId].ownerPcb = runningProcess;
    MP_INPUT_DATA MPData;
	for (int j = 0; j < NUMBER_PHYSICAL_PAGES; j++) {
		if (j < usedFrame){
            if (frameTable[j].ownerPcb == NULL){
                // -1 means it is shared.
                MPData.frames[j].Pid = -1;
            }else {
                MPData.frames[j].Pid = frameTable[j].ownerPcb->processId;
            }
			int virtualId = frameTable[j].virtualId;
			MPData.frames[j].InUse = TRUE;
			
			MPData.frames[j].LogicalPage = frameTable[j].virtualId;
			INT16 state = 0;
            // printf("hahahah %d \n", frameTable[j].ownerPcb->pageTable[virtualId]);
            if (frameTable[j].ownerPcb != NULL){
                if ((frameTable[j].ownerPcb->pageTable[virtualId] & PTBL_MODIFIED_BIT) != 0){
                    state += FRAME_MODIFIED;
                }
                if ((frameTable[j].ownerPcb->pageTable[virtualId] & PTBL_REFERENCED_BIT) != 0){
                    state += FRAME_REFERENCED;
                }
                if ((frameTable[j].ownerPcb->pageTable[virtualId] & PTBL_VALID_BIT) != 0){
                    state += FRAME_VALID;
                }
            }
			
			MPData.frames[j].State = state;
		} else {
			MPData.frames[j].InUse = TRUE;
			MPData.frames[j].Pid = 0;
			MPData.frames[j].LogicalPage = 0;
			MPData.frames[j].State = 0;
		}
	}
	MPPrintLine(&MPData);
}
// get free unused frame
int getFreeFrame(){
    if (usedFrame < NUMBER_PHYSICAL_PAGES){
        int freeOne = usedFrame;
        usedFrame++;
        return freeOne;
    }
    return -1;
}
// by using clock algorithm to get frame
int getAvlFrame(){
    // if no free frame, free one by using clock page repacing algorithm and write it to disk swap area, 
    // then set valid bit to valid
    int vicitim = -1;
    while (vicitim == -1){
        int i = my_clock % 64;
        short *tempPage = frameTable[i].ownerPcb->pageTable;
        if (frameTable[i].isShared == TRUE){
            my_clock++;
            continue;
        }
        long tempVirtualId = frameTable[i].virtualId;
        if ((tempPage[tempVirtualId] & (UINT16)PTBL_REFERENCED_BIT) == 0){
            vicitim = i;
            break;
        } else {
            tempPage[tempVirtualId] = tempPage[tempVirtualId] - (UINT16)PTBL_REFERENCED_BIT;
        }
        my_clock++;
    }
    // now check whether the vicitim needs to write to swap are or not
    short *vicitimPage = frameTable[vicitim].ownerPcb->pageTable;
    long vicitimVirtualId = frameTable[vicitim].virtualId;
    
    // // IF PAGE table is modified we need to write it to swap area
    int secId = getFreeTempSec(vicitimVirtualId);
    char DataRead[PGSIZE];
        Z502ReadPhysicalMemory(vicitim, DataRead);
        for (int i = 0; i < 16; i++){
            tempSwapArea[secId][i] = DataRead[i];
        }
    vicitimPage[vicitimVirtualId] -= (UINT16)PTBL_VALID_BIT;    
    frameTable[vicitim].ownerPcb->swapedFrame[vicitimVirtualId] = secId;
    
    printf("!!!VicitimPage %d VicitimFrame %d belongToPID %d swapSecID %d data %x\n", vicitimVirtualId, vicitim, frameTable[vicitim].ownerPcb->processId, secId, *(long*)DataRead);
    // then tell the process that the swapped out frame
    // virtual id is the place where to send frame to. 
    // initialization of swpaedFrame is -1
    return vicitim;
    // so now we need to find virtual Id's corresponding 
}
// get free temporary sector
int getFreeTempSec(int virtualId){
    // if virtual page hold a entry. keep it.
    for (int i = 0; i < 1020; i++){
        if (tempSwapStatus[i] == FALSE){
            tempSwapStatus[i] = TRUE;
            return i;
        }
    }
    printf("swap area is full!");

    return -1;
}
int cntForChoosen = 0;
int choosenFrame[64];
int getFreeFrameForShare(char areaTag[32]){
    if (usedFrame < NUMBER_PHYSICAL_PAGES){
        int freeOne = usedFrame;
        usedFrame++;
        frameTable[freeOne].isShared = TRUE;
        choosenFrame[cntForChoosen++] = freeOne;        
        return freeOne;
    }
    return -1;
}

// define share area
void defineArea( long startAddrVirt, long numOfVirtPages, char areaTag[32], INT32 *ourSharedId, INT32 *errorCode){
    
    *ourSharedId = sharedId++;
    if (*ourSharedId == 0){
        for (int i = startAddrVirt / PGSIZE; i < startAddrVirt + numOfVirtPages; i++){
            int choosenOne = getFreeFrameForShare(areaTag);
            runningProcess->pageTable[i] = PTBL_VALID_BIT;
            runningProcess->pageTable[i] += choosenOne;
        }
    } else {
        int j = 0;
        for (int i = startAddrVirt / PGSIZE; i < startAddrVirt + numOfVirtPages; i++){
            runningProcess->pageTable[i] = PTBL_VALID_BIT;
            runningProcess->pageTable[i] += choosenFrame[j++];
        }
    }
}