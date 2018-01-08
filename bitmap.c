/**********************************************************
Bitmap designed for offer function about manipulation of 
bitmap to get free disk sector, and some basic bit 
manipualtion function.

***********************************************************/


#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"
// save all the pcb's information
extern LinkedPcb wholePcb[WHOLE_PCB_LEN];
// cache a bitmap to increase speed
DiskBlock bitmap[BIT_MAP_SIZE * 4];
int curSec = 0;

// make bitmap all the bytes to be '\0'
void initBitMapCache(){
    for (int i = 0; i < BIT_MAP_SIZE * 4; i++){
        for (int k = 0; k < BLOCK_LEN; k++){
            bitmap[i].bytes[k] = '\0';
        }
    }
}
// setup sector which is occupied by block0 to bitmap
void setUpBlock0ForBitMap(){
    DiskBlock *block0 = &bitmap[0];
    byte *temp;
    temp = block0->bytes;
    set(temp, 0);
    int preEndPos;
    for (int i = 1; i <= BIT_MAP_SIZE * 4 + 1; i++){
        set(i / BYTE_BIT + temp, i % BYTE_BIT);
    }
    // set sector for root director 
    preEndPos = BIT_MAP_SIZE * 4 + 1;
    for (int i = preEndPos + 1; i <= ROOT_DIR_SIZE + preEndPos + 1; i++){
        // printf("haha%d", *(i / BYTE_BIT + temp));
        set(i / BYTE_BIT + temp, i % BYTE_BIT);
        // printf("!!!!! %d", *(i / BYTE_BIT + temp));
    }
    preEndPos = ROOT_DIR_SIZE + preEndPos + 1;
    // set sector for swap
    for (int i = preEndPos + 1; i <= preEndPos + 1 + SWAP_SIZE * 4; i++){
        // printf("haha%d", *(i / BYTE_BIT + temp));
        set(i / BYTE_BIT + temp, i % BYTE_BIT);
        // printf("!!!!! %d", *(i / BYTE_BIT + temp));
    }
    curSec = 29;
}
// setup input position bit to 1
void set(byte *pbyte, int pos){
    *pbyte |= (1 << pos);
}
// shift left by input units
void shiftLeft(byte *pbyte, int units){
    *pbyte = *pbyte << units;
}
// shift right by input units
void shiftRight(byte *pbyte, int units){
    *pbyte = *pbyte >> units;
}

int nextFreeSectorAndMark(){
    // first determine which block 
    DiskBlock *temp = &bitmap[curSec / BLOCK_BYTE_BIT];
    // second determine which byte within that block
    int bytePos = (curSec % BLOCK_BYTE_BIT) / BYTE_BIT;
    byte *tempByte;
    tempByte = &temp->bytes[bytePos];
    // third determine which bit within that byte
    int tempEndPos = curSec % BLOCK_BYTE_BIT;
    int bitPos = tempEndPos % BYTE_BIT;
    // then check is it occupied?
    // printf("!!!!!!!!!!!!!!!!!!!!!!%d", bytePos);
    if (isOccupied(tempByte, bitPos) == TRUE){
        curSec++;
        return nextFreeSectorAndMark();
    } else {
        // printf("end Pos is !!!!!!! %d \n", curSec);
        // printf("hahahaha BIT POS IS %d", bitPos);
        // printf("before tempByte is %x \n", *tempByte);
        set(tempByte, bitPos);
        // printf("tempByte is %x \n", bitmap[0].bytes[bytePos]);
        int ans = curSec;
        curSec++;
        return ans;
    }
}
// determine whetehr this bit of the input byte is 1 or not
BOOL isOccupied(byte *pbyte, int pos){
    byte temp = 0;
    set(&temp, pos);
    if ((*pbyte & temp) != 0){
        return TRUE;
    } else return FALSE;
}
// fluch bitmap to disk
void flushBitmap(PcbNode **pcb){
    for (int i = 0; i < BIT_MAP_SIZE * 4; i++){
        (*pcb)->container = bitmap[i].bytes;
        (*pcb)->diskSector = BIT_MAP_LOC + i;
        addToDiskQueue(*pcb, TRUE);
        *pcb = NULL;
        dispatcher();
    }
}
// determine are there any more space from disk
BOOL haveDiskSpace(){
    if (curSec < BIT_MAP_LOC + BIT_MAP_SIZE * 4 * BLOCK_LEN * BYTE_BIT){
        return TRUE;
    }else return FALSE;
}