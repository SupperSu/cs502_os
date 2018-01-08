/*********************
This is place to save my global variables
**********************/
// Error type
#define ERR_PRIORITY                    1
#define ERR_PROCESS_OVERFLOW            2
#define ERR_PROCESS_NOT_EXIST           3
#define ERR_PROCESS_NAME_OVERLAPPING    4
#define ERR_DISKID_OUT_OF_RANGE         5
#define ERR_FILE_NAME_TOO_LONG          6
#define ERR_DISK_NO_SPACE               7
#define ERR_IS_ROOT                     8
#define ERR_INODE_IS_NOT_THIS_FILE      9
#define ERR_LOGICAL_BLOCK_TOO_LARGE     10
#define ERR_NO_FREE_SPACE_INSIDE_FILE   11
#define ERR_NO_CURRENT_DIR              12
// these global value designed for state of process
#define NEW                             1
#define RUNNING                         2
#define WAITING_TIMER                   3
#define WAITING_DISK_READ               4
#define WAITING_DISK_WRITE              5
#define READY                           6
#define TERMINATED                      7
#define DISK_IS_READING                 8
#define DISK_IS_WRITING                 9
#define READING                         10

#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE

// whole process control block number
#define WHOLE_PCB_LEN            25
#define MAX_EXISTING_PCB_NUM     15
// doubly linked circle list

// File system structure 
#define BIT_MAP_SIZE    3
#define ROOT_DIR_SIZE   2
#define SWAP_SIZE       255
#define DISK_LENGTH     2048
#define BLOCK0_LOC      0
#define BIT_MAP_LOC     1
#define ROOT_DIR_LOC    BIT_MAP_LOC + BIT_MAP_SIZE * 4 + 1
#define SWAP_LOC        ROOT_DIR_LOC + ROOT_DIR_SIZE + 1 // ()
#define MASK_LSF        255
#define MASK_MSF        65280
#define IDX_LEVEL       1
#define DUMMY_TIME      12345
#define FILE_SIZE       128
#define BLOCK_LEN       16
#define ROOT_IDX_LOC    28
#define DESC_OFFSET     8
#define NAME_OFFSET     1
#define BLOCK_BYTE_BIT  128
#define BYTE_BIT        8
#define MAX_NUM_FREE_FRAME      64

typedef unsigned char byte;
// container is the current dir
typedef struct Message{
    INT32 fromPid;
    char *contents;
    INT32 messageLength;
} Message;
typedef struct PcbNode{
    char *processName;
    long processId;
    long state;
	long *contextId;
    long wakeTime;
    long priority;
    long diskId;
    int diskSector;
    short *pageTable;
    short pageFaultId;
    long swapedFrame[NUMBER_VIRTUAL_PAGES];
    BOOL fromMem;
    byte *container;
    byte currentDir[BLOCK_LEN];
    byte currentFile[BLOCK_LEN];
    byte index[BLOCK_LEN];
    struct Message *mailBox;
    BOOL hasMails;
    struct PcbNode *next;
    struct PcbNode *prev;
}PcbNode,*LinkedPcb;

typedef struct FrameEntry{
    // virtual page number
    char areaTag[32];
    BOOL isShared;
    int virtualId;
    struct PcbNode *ownerPcb;
} FrameEntry;


typedef struct Block{
    byte bytes[PGSIZE];   // 16 bytes
} DiskBlock;

void addToTimerQueue(PcbNode *);
PcbNode *dequeueFromTimerQueue(void);
void TimerInterruptHandler(void);

void addToDiskQueue(PcbNode *, BOOL);
PcbNode *dequeueFromDiskQueue(long diskId);

void TimerInterruptHandler();

void killPCB(long pId, long *);
void addToReadyQueue(PcbNode *);
PcbNode *dequeueFromReadyQueue(void);
BOOL nothingInReadyQueue(void);

BOOL anyMore();

void formatDisk(PcbNode *, long *);
void initBitMap(PcbNode *, int);