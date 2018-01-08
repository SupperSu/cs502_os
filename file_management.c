/**********************************************************
This file management design to serve the Z502 file structrue.
This file system is based on idx level 1

pcb cache three things, current file block, current dir block,
and index sector
***********************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <ctype.h>
#include 			"my_global.h"
extern DiskBlock diskBlocks[8];
extern LinkedPcb wholePcb[25];
int currentFileCount = 0;
// incase of open dir which use -1 as id.
// not work in multi processor mode
int curDiskId;
// map the sector address with inode 
int mapInodeSec[31];

// initialize pcb index as 0 but not flush it to disk
void initIdxSec(PcbNode **pcb){
    byte container[16];
    for (int i = 0; i < BLOCK_LEN; i++){
        (*pcb)->index[i] = 0;
    }
}
int getOriginalNum(byte msb, byte lsb){
    return (int)(msb << 8) + (int)lsb;
}
byte getMSB(int input){
    return (byte)((input & MASK_MSF)>>8);
}
byte getLSB(int input){
    return (byte)(input & MASK_LSF);
}
// record input sector number in index sector
void setUpIdx(PcbNode **pcb, int input){
    for (int i = 0; i < BLOCK_LEN; i += 2){
        if ((*pcb)->index[i] != 0 || (*pcb)->index[i + 1] != 0)continue;
        (*pcb)->index[i] = getLSB(input);
        (*pcb)->index[i + 1] = getMSB(input);
        return;
    }
    // printf("index is full!!!!!!!");
}
// get Inode
int getInodeId(PcbNode **pcb){
    byte temp = (*pcb)->currentDir[0];
    return temp;
}
// get Inode from current file
int getInodeIdFromFile(PcbNode **pcb){
    byte temp = (*pcb)->currentFile[0];
    return temp;
}
// make current file empty
void clearFile(PcbNode **pcb){
    strcpy((*pcb)->currentFile, "");
}
// get index by using input logical block
int getIdx(PcbNode **pcb, long logicalBlock){
    byte lsf, msf;
    lsf = (*pcb)->index[2 * logicalBlock];
    msf = (*pcb)->index[2 * logicalBlock + 1];
    return getOriginalNum(msf, lsf);
}
// determine any more index for new file ro director.
BOOL hasFreeIdx(PcbNode **pcb){
    for (int i = 0; i < BLOCK_LEN; i += 2){
        if ((*pcb)->index[i] != 0 || (*pcb)->index[i + 1] != 0)continue;
        return TRUE;
    }
    // printf("no free idx");
    return FALSE;
}
// initialize header block for dir or file
void initHeaderBlock(PcbNode **pcb, char *name, BOOL isRoot, BOOL isDir, long *errorCode){
    if ((*pcb)->diskId > 8 || (*pcb)->diskId < 0){
        *errorCode = ERR_DISKID_OUT_OF_RANGE;
        return;
    }
    if (isRoot == TRUE){
        name = "ROOT";
    }
    int len = strlen(name);
    if (len > 7){
        *errorCode = ERR_FILE_NAME_TOO_LONG;
        return;
    }
    if (len == 0){
        return;
    }
    DiskBlock header;
    header.bytes[0] = currentFileCount++;
    // setup Name of file or directory
    int count = 0;
    for (int i = 1; i <= 7; i++){
        if (count < len){
            header.bytes[i] = *(name + count);
            count++;
        } else {
            header.bytes[i] = '\0';
        }
    }
    // setup Dec
    header.bytes[8] = 0;
    if (isDir == TRUE){
        
        set(&header.bytes[8], 0);
        // printf("SWEEEEEE MEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE %d \n", header.bytes[8]);
        // if it is directory no index level. 
        // need to setup parent Inode for root
        if (isRoot == TRUE){
            for (int i = 3; i <= 7; i++){
                set(&header.bytes[8], i);
            }
        } else {
            int parInode;
            parInode = getInodeId(&(*pcb));
            shiftLeft(&parInode, 3);
            header.bytes[8] += parInode;
        } 
        // setup file
    } else {
        // index level defined in myglobal.h which is 1
        // so setup 2th bit to be one
        set(&header.bytes[8], 1);
        
        byte parInode;
        parInode = getInodeId(&(*pcb));
        shiftLeft(&parInode, 3);
        header.bytes[8] += parInode;
    }
    // setup creation time
    header.bytes[9] = DUMMY_TIME & MASK_MSF;
    header.bytes[11] = DUMMY_TIME & MASK_LSF;
    
    // setup index location
    int secNum = nextFreeSectorAndMark();
    initIdxSec(&(*pcb));
    flushIdxBySec(&(*pcb), secNum); 
    // printf("DISK SECTOR of new IDX FOR NEW DIR is %d",(*pcb)->diskSector);
    header.bytes[12] = getLSB(secNum);
    header.bytes[13] = getMSB(secNum);
    // setup file size 
    header.bytes[14] = 0;
    header.bytes[15] = 0;
    int newSec;
    if (isRoot == TRUE){
        newSec = ROOT_DIR_LOC;
    } else {
        newSec = nextFreeSectorAndMark();
        cacheIdx(&(*pcb));
        setUpIdx(&(*pcb),newSec);
        flushIdx(&(*pcb));
    }
    mapInodeSec[currentFileCount - 1] = newSec;
    writeDisk(&(*pcb), newSec, (*pcb)->diskId, &header.bytes);
}
// initialize block 0 for format function
void initBlock0(PcbNode **pcb, long *errorCode){
    if ((*pcb)->diskId > 8 || (*pcb)->diskId < 0){
        *errorCode = ERR_DISKID_OUT_OF_RANGE;
        return;
    }
    *errorCode = ERR_SUCCESS;
    unsigned char tempId;
    tempId = (unsigned char) (*pcb)->diskId;
    DiskBlock temp = diskBlocks[tempId];
    temp.bytes[0] = tempId;
    temp.bytes[1] = BIT_MAP_SIZE;
    temp.bytes[2] = ROOT_DIR_SIZE;
    temp.bytes[3] = SWAP_SIZE;
    temp.bytes[4] = getLSB(DISK_LENGTH); // get lsb
    temp.bytes[5] = getMSB(DISK_LENGTH); // get msb
    temp.bytes[6] = getLSB(BIT_MAP_LOC);
    temp.bytes[7] = getMSB(BIT_MAP_LOC);
    temp.bytes[8] = getLSB(ROOT_DIR_LOC);
    temp.bytes[9] = getMSB(ROOT_DIR_LOC);
    temp.bytes[10] = getLSB(SWAP_LOC);
    temp.bytes[11] = getMSB(SWAP_LOC);
    for (int i = 12; i < 16; i++){
        temp.bytes[i] = '\0';
    }
    writeDisk(&(*pcb), 0, (*pcb)->diskId, &temp.bytes);
}
// check disk system_call
void CheckDisk( long DiskID, long *ReturnedError ){
    if (DiskID < 0 || DiskID > 8){
        *ReturnedError = ERR_DISKID_OUT_OF_RANGE;
        return;
    }
    *ReturnedError = ERR_SUCCESS;
    MEMORY_MAPPED_IO mmio;
    mmio.Mode = Z502CheckDisk;
    mmio.Field1 = (long)DiskID;
    mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_READ(Z502Disk, &mmio);
}
// get parent sector number
int getParSec(PcbNode **pcb){
    int temp;
    temp = getInodeId(&(*pcb));
    return mapInodeSec[temp];
}
// get current dir's index sector
int getIdxSec(PcbNode **pcb){
    byte lsb, msb;
    lsb = (*pcb)->currentDir[12];
    msb = (*pcb)->currentDir[13];
    return getOriginalNum(msb,lsb);
}
// get index sector number from current file
int getIdxSecFromFile(PcbNode **pcb){
    byte lsb, msb;
    lsb = (*pcb)->currentFile[12];
    msb = (*pcb)->currentFile[13];
    return getOriginalNum(msb, lsb);
}
// get dir's name from current directory
void getDirName(PcbNode **pcb, byte *name){
    char fileName[7];
    for (int k = 0; k < 7; k++){
       fileName[k] = (*pcb)->currentDir[NAME_OFFSET + k];
    }
    *name = fileName;
}   

// system call function
// to open directory and cache it into pcb and its corresponding
// index of that director
void openDir(PcbNode **pcb, char *name, long *errorCode) {
    // if open root dir
    byte output[BLOCK_LEN];
    if (strcmp(name, "root") == 0){
        curDiskId = (*pcb)->diskId;
        readDisk(&(*pcb), ROOT_DIR_LOC, curDiskId, &output);
        cacheIdx(&(*pcb));
    } else if (strcmp(name, "..") == 0){
        byte *curDirName;
        getDirName(&(*pcb), &curDirName);
        if (strcmp(curDirName, "root") == 0){
            *errorCode = ERR_IS_ROOT;
        }
        int parSec;
        parSec = getParSec(&(*pcb));
        readDisk(&(*pcb), parSec, curDiskId, &output);
    } else {
        int targetSec;
        if (haveDirNames(&(*pcb), name, &targetSec) == TRUE){
            readDisk(&(*pcb), targetSec, curDiskId, &output);
        } else {
            // printf("no this file ! so create it !!!");
            createDir(&(*pcb), name, errorCode);
            readDisk(&(*pcb), (*pcb)->diskSector, curDiskId, &output);
        }
    }
    *errorCode = ERR_SUCCESS;
    writeToCurDir(&(*pcb), &output);
    cacheIdx(&(*pcb));
}
// system_call function to open a file
void openFile(PcbNode **pcb, char *name, int *Inode, long *errorCode) {
    // if open root dir
    byte output[BLOCK_LEN];
    int targetSec;
    if (haveDirNames(&(*pcb), name, &targetSec) == TRUE){
        readDisk(&(*pcb), targetSec, curDiskId, &output);
    } else {
        // printf("no this file ! so create it and read it!!!");
        createFile(&(*pcb), name, errorCode);
        readDisk(&(*pcb), (*pcb)->diskSector, curDiskId, &output);
    }
    writeToCurFile(&(*pcb), &output);
    *Inode = getInodeIdFromFile(&(*pcb));
    cacheFileIdx(&(*pcb));
}
// system_call function create a directory under this current dir
void createDir(PcbNode **pcb, char *name, long *errorCode){
    if (haveDiskSpace() == FALSE){
        *errorCode = ERR_DISK_NO_SPACE;
        return;
    }
    if (validName(name) == FALSE){
        *errorCode = ERR_FILE_NAME_TOO_LONG;
        return;
    }
    byte *existSector;
    if (haveDirNames(&(*pcb), name, existSector) == TRUE){
        // printf("the name of file has already existed");
        return;
    }
    initHeaderBlock(&(*pcb), name, FALSE, TRUE, errorCode);
}
// system_call function to create a file under this current dir
void createFile(PcbNode **pcb, char *name, long *errorCode){
    if (haveDiskSpace() == FALSE){
        *errorCode = ERR_DISK_NO_SPACE;
        return;
    }
    if (validName(name) == FALSE){
        *errorCode = ERR_FILE_NAME_TOO_LONG;
        return;
    }
    byte *existSector;
    if (haveDirNames(&(*pcb), name, existSector) == TRUE){
        // printf("the name of file has already existed");
        return;
    }
    initHeaderBlock(&(*pcb), name, FALSE, FALSE, errorCode);
}
// system_call function to write data block into file
void writeFile(PcbNode **pcb, long Inode, long logicalBlock, char *writeBuffer, long *errorCode){
    if (getInodeIdFromFile(&(*pcb)) != Inode){
        *errorCode = ERR_INODE_IS_NOT_THIS_FILE;
        return;
    }
    if (logicalBlock > IDX_LEVEL * 8){
        *errorCode = ERR_LOGICAL_BLOCK_TOO_LARGE;
        return;
    }
    // cacheFileIdx(&(*pcb));
    if (hasFreeIdx(&(*pcb)) == FALSE){
        *errorCode = ERR_NO_FREE_SPACE_INSIDE_FILE;
        return;
    }
    int newSec = nextFreeSectorAndMark();
    setUpIdx(&(*pcb), newSec);
    writeDisk(&(*pcb), newSec, curDiskId, writeBuffer);
}
// system_call function to read data block from current file
void readFile(PcbNode **pcb, long Inode, long logicalBlock, char *readBuffer, long *errorCode){
    if (getInodeIdFromFile(&(*pcb)) != Inode){
        *errorCode = ERR_INODE_IS_NOT_THIS_FILE;
        return;
    }
    if (logicalBlock > IDX_LEVEL * 8){
        *errorCode = ERR_LOGICAL_BLOCK_TOO_LARGE;
        return;
    }
    // cacheFileIdx(&(*pcb));
    if (hasFreeIdx(&(*pcb)) == FALSE){
        *errorCode = ERR_NO_FREE_SPACE_INSIDE_FILE;
        return;
    }
    *errorCode = ERR_SUCCESS;
    // modify file's idex sector
    int tempSec = getIdx(&(*pcb), logicalBlock);
    readDisk(&(*pcb), tempSec, curDiskId,readBuffer);
}
// system_call function to close a file and flush everything of that file 
// to disk
void closeFile(PcbNode **pcb, long Inode, long *errorCode){
    flushBitmap(&(*pcb));
    flushFileIdx(&(*pcb));
    cacheIdx(&(*pcb));
    clearFile(&(*pcb));
}
// list current dir's contents
void dirContents(PcbNode **pcb, long *errorCode){
    if ((*pcb)->currentDir == NULL){
        *errorCode = ERR_NO_CURRENT_DIR;
        return;
    }
    printf("Inode,   FileName,    D/F,    Creation Time,    FILE SIZE\n");
    cacheIdx(&(*pcb));
    for (int i = 0; i < BLOCK_LEN; i = i + 2){
        byte lsb = (*pcb)->index[i];
        byte msb = (*pcb)->index[i + 1];
        int temp = getOriginalNum(msb, lsb);
        if (temp == 0){
            continue;
        }
        byte output[BLOCK_LEN];
        readDisk(&(*pcb), temp, curDiskId, &output);
        byte Inode = output[0];
        char name[7];
        for (int i = 0; i < 8; i++){
            name[i] = output[i + NAME_OFFSET];
        }
        byte desc = output[8];
        char type;
        if ((desc & 1) == 1){
            type = 'D';
        } else {
            type = 'F';
        }
        byte time = 1234;
        byte size1 = output[14];
        byte size2 = output[15];
        if (type == 'D'){
            printf("%d     %s      %d       %d          %c\n", Inode, name, type, time, '-');
        } else {
            printf("%d     %s      %c       %d          %d\n", Inode, name, type, time, 8);
        }
        
    }
}
// check have the same name in current dir
BOOL haveDirNames(PcbNode **pcb, char *name, int *sector){
    int idxSec;
    int startSec = (*pcb)->diskSector;
    if ((*pcb)->diskId == -1){
        (*pcb)->diskId = curDiskId;
    }
    cacheIdx(&(*pcb));
    // now we are in index section
    // we need to remeber where we start
    // int idexSec[BLOCK_LEN];
    int count = 0;
    for (int i = 0; i < BLOCK_LEN; i = i + 2){
        byte lsb = (*pcb)->index[i];
        byte msb = (*pcb)->index[i + 1];
        int temp = getOriginalNum(msb,lsb);
        if (temp == 0){
            continue;
        }
        byte output[BLOCK_LEN];
        readDisk(&(*pcb), temp, curDiskId, &output);
        int len = strlen(name);
        char fileName[7];
        for (int k = 0; k < 7; k++){
           fileName[k] = output[NAME_OFFSET + k];
        }
        if (strcmp(name, fileName) == 0){
            *sector = temp;
            return TRUE;
        }
    }
    return FALSE;
}
// determine is this name is too long?
BOOL validName(char *name){
    int len = strlen(name);
    if (len > 0 && len < 8){
        return TRUE;
    } return FALSE;
}
// write input to pcb curent dir buffer
void writeToCurDir(PcbNode **pcb, byte *input){
    for (int i = 0; i < BLOCK_LEN; i++){
        (*pcb)->currentDir[i] = *(input + i);
    }
}
// write input to pcb current idx buffer
void writeToCurIdx(PcbNode **pcb, byte *input){
    int pcount = 0;
    for (int i = 0; i < BLOCK_LEN; i++){
        (*pcb)->index[i] = *(input + i);
    }
}
// write input to pcb current file buffer
void writeToCurFile(PcbNode **pcb, byte *input){
    int pcount = 0;
    for (int i = 0; i < BLOCK_LEN; i++){
        (*pcb)->currentFile[i] = *(input + i);
    }
}
// flush current idx to disk
void flushIdx(PcbNode **pcb){
    int indexSec;
    indexSec = getIdxSec(&(*pcb));
    byte output[BLOCK_LEN];
    writeDisk(&(*pcb), indexSec, (*pcb)->diskId, &((*pcb)->index));
}
// flush current file's idex sector to disk
void flushFileIdx(PcbNode **pcb){
    int indexSec;
    indexSec = getIdxSecFromFile(&(*pcb));
    byte output[BLOCK_LEN];
    writeDisk(&(*pcb), indexSec, (*pcb)->diskId, &((*pcb)->index));
}
// flush current index sector by specificed sector 
void flushIdxBySec(PcbNode **pcb, int indexSec){
    writeDisk(&(*pcb), indexSec, (*pcb)->diskId, &((*pcb)->index));
}
// cache the current dir/s index sector
void cacheIdx(PcbNode **pcb){
    int indexSec;
    indexSec = getIdxSec(&(*pcb));
    byte output[BLOCK_LEN];
    readDisk(&(*pcb), indexSec, (*pcb)->diskId, &output);
    writeToCurIdx(&(*pcb), &output);
}
// cache the current file's index sector
void cacheFileIdx(PcbNode **pcb){
    int indexSec;
    indexSec = getIdxSecFromFile(&(*pcb));
    byte output[BLOCK_LEN];
    readDisk(&(*pcb), indexSec, (*pcb)->diskId, &output);
    writeToCurIdx(&(*pcb), &output);
}
// refresh file size
void refreshFileSize(PcbNode **pcb){
    byte lsf, msf;
    lsf = (*pcb)->currentFile[14];
    msf = (*pcb)->currentFile[15];
    int total =getOriginalNum(msf,lsf);
    int temp = (total / 16) + 1;
    (*pcb)->currentFile[14] = getLSB(temp * 16) ;
    (*pcb)->currentFile[15] = getMSB(temp * 16) ;
    byte dummy;
    writeDisk(&(*pcb), mapInodeSec[(*pcb)->currentFile[0]], curDiskId, &dummy);
}
