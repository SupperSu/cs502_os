#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"

BOOL swapStatus[1020];
extern char tempSwapArea[1020][16];
void initSwapArea(PcbNode **pcb){
    for (int i = 0; i < SWAP_SIZE * 4; i++){
        initSwap(&(*pcb), i);
    }
}
void initSwap(PcbNode **pcb, int k){
    byte noneBytes[16];
    for (int i = 0; i < 16; i++){
        noneBytes[i] = '\0';
        tempSwapArea[k][i] = '\0';
    } 
    
    (*pcb)->diskSector = SWAP_LOC + k;
    (*pcb)->container = noneBytes;
    writeDisk(&(*pcb), (*pcb)->diskSector,(*pcb)->diskId, (*pcb)->container);
}
void updateSwapArea(PcbNode **pcb){
    for (int i = 0; i < SWAP_SIZE * 4; i++){
        updateSwap(&(*pcb), i + SWAP_LOC);
    }
}
void updateSwap(PcbNode **pcb, int k){
    byte noneBytes[16];
    for (int i = 0; i < 16; i++){
        noneBytes[i] = (byte)tempSwapArea[k][i];
    } 
    (*pcb)->diskSector = k;
    (*pcb)->container = noneBytes;
    writeDisk(&(*pcb), (*pcb)->diskSector,1, (*pcb)->container);
}
int getFreeSwapSector(int virtualId){
    // try virtual page 
    if (swapStatus[virtualId] == FALSE){
        swapStatus[virtualId] = TRUE;
        return virtualId + SWAP_LOC;
    }
    // if virtual page hold a entry. keep it.
    for (int i = 0; i < 1020; i++){
        if (swapStatus[i] == FALSE){
            swapStatus[i] = TRUE;
            return i + SWAP_LOC;
        }
    }
    printf("swap area is full!");
    return -1;
}
