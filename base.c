/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include 			"my_global.h"
// single processor so only one running process
PcbNode *runningProcess;
// save all the pcb in jobQ
LinkedPcb jobQ;
// save pcb by ranking waketime
LinkedPcb timerQ;
// save pcbs which are ready to be processed order by priority
LinkedPcb readyQ;
LinkedPcb susQ;
// save pcbs which are require IO request
LinkedPcb diskArray[8];
LinkedPcb wholePcb[25];
DiskBlock diskBlocks[8];
int curNumProcess = 0; // to assign the id of process
extern int countOfReadyQ;
extern int countOfTimerQ;
//  Allows the OS and the hardware to agree on where faults occur
extern void *TO_VECTOR[];

char *call_names[] = { "mem_read ", "mem_write", "read_mod ", "get_time ",
		"sleep    ", "get_pid  ", "create   ", "term_proc", "suspend  ",
		"resume   ", "ch_prior ", "send     ", "receive  ", "PhyDskRd ",
		"PhyDskWrt", "def_sh_ar", "Format   ", "CheckDisk", "Open_Dir ",
		"OpenFile ", "Crea_Dir ", "Crea_File", "ReadFile ", "WriteFile",
		"CloseFile", "DirContnt", "Del_Dir  ", "Del_File " };

/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the OS.
 ************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;
	INT32 Status;

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	static BOOL remove_this_in_your_code = TRUE; /** TEMP **/
	static INT32 how_many_interrupt_entries = 0;
	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;
	if (mmio.Field4 != ERR_SUCCESS) {
		printf(
				"The InterruptDevice call in the InterruptHandler has failed.\n");
		printf("The DeviceId and Status that were returned are not valid.\n");
	}
	PcbNode *temp;
	long diskId;
	if (DeviceID == TIMER_INTERRUPT){
		TimerInterruptHandler();
		return ;
	}else if (DeviceID == DISK_INTERRUPT_DISK0){
		diskId = 0;
	} else if (DeviceID == DISK_INTERRUPT_DISK1){
		diskId = 1;
	} else if (DeviceID == DISK_INTERRUPT_DISK2){
		diskId = 2;
	} else if (DeviceID == DISK_INTERRUPT_DISK3){
		diskId = 3;
	} else if (DeviceID == DISK_INTERRUPT_DISK4){
		diskId = 4;
	} else if (DeviceID == DISK_INTERRUPT_DISK5){
		diskId = 5;
	} else if (DeviceID == DISK_INTERRUPT_DISK6){
		diskId = 6;
	} else if (DeviceID == DISK_INTERRUPT_DISK7){
		diskId = 7;
	}
	temp = dequeueFromDiskQueue(diskId);
	addToReadyQueue(temp);
	// in case of yezhizhen
	temp = NULL;
	/** REMOVE THE NEXT SIX LINES **/
	how_many_interrupt_entries++; /** TEMP **/
	if (remove_this_in_your_code && (how_many_interrupt_entries < 10)) {
		printf("Interrupt_handler: Found device ID %d with status %d\n",
				(int) mmio.Field1, (int) mmio.Field2);
	}
}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void FaultHandler(void) {
	// INT32 *LockResult1;
	// READ_MODIFY(MEMORY_INTERLOCK_BASE+ 31, DO_LOCK, SUSPEND_UNTIL_LOCKED,
	// 	&LockResult1);
	INT32 DeviceID;

	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	// Get cause of interrupt
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	mmio.Mode = Z502GetInterruptInfo;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;

	INT32 Status;
	Status = mmio.Field2;
	printf("Fault_handler: Found vector type %d with value %d\n", DeviceID,
			Status);
	if (Status >= NUMBER_VIRTUAL_PAGES){
		mmio.Mode = Z502Action;
		mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Halt, &mmio);
		return;
	}
	frameFaultHandler(Status);
	
	// READ_MODIFY(MEMORY_INTERLOCK_BASE + 31, DO_UNLOCK, SUSPEND_UNTIL_LOCKED,
	// 	&LockResult1);
} // End of FaultHandler

 
/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/

void svc(SYSTEM_CALL_DATA *SystemCallData) {
	short call_type;
	static short do_print = 10;
	short i;
	MEMORY_MAPPED_IO 		mmio;
	call_type = (short) SystemCallData->SystemCallNumber;
	// if (do_print > 0) {
	// 	printf("SVC handler: %s\n", call_names[call_type]);
	// 	for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
	// 		//Value = (long)*SystemCallData->Argument[i];
	// 		printf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
	// 				(unsigned long) SystemCallData->Argument[i],
	// 				(unsigned long) SystemCallData->Argument[i]);
	// 	}
	// 	do_print--;
	// }
	INT32 *LockResult;
	
	switch (call_type) {
		case SYSNUM_GET_TIME_OF_DAY:
			mmio.Mode =Z502ReturnValue;
			mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
			MEM_READ(Z502Clock, &mmio);
			*(long *)SystemCallData->Argument[0] = mmio.Field1;
			break;
		case SYSNUM_TERMINATE_PROCESS:   
			osTerminateProcess(SystemCallData->Argument[0], SystemCallData->Argument[1]);
			printWholePCB();
			break;
		case SYSNUM_SLEEP:
			startTimer(&runningProcess, (long) SystemCallData->Argument[0]);
			printWholePCB();
			break;
		case SYSNUM_GET_PROCESS_ID:
			getProcessId(SystemCallData->Argument[0], SystemCallData->Argument[1],SystemCallData->Argument[2]);
			break;
		case SYSNUM_PHYSICAL_DISK_WRITE:
			writeDisk(&runningProcess, SystemCallData->Argument[1], SystemCallData->Argument[0], SystemCallData->Argument[2]);
			printWholePCB();
			break;
		case SYSNUM_PHYSICAL_DISK_READ:
			readDisk(&runningProcess, SystemCallData->Argument[1], SystemCallData->Argument[0], SystemCallData->Argument[2]);
			printWholePCB();
			break;
		case SYSNUM_CREATE_PROCESS:
			osCreateProcess(SystemCallData->Argument[0],SystemCallData->Argument[1] , SystemCallData->Argument[2], 
				SystemCallData->Argument[3], SystemCallData->Argument[4]);
			printWholePCB();
			break;
		case SYSNUM_FORMAT:
			runningProcess->diskId = SystemCallData->Argument[0];
			initBitMapCache();
			initBlock0(&runningProcess,SystemCallData->Argument[1]);
			setUpBlock0ForBitMap();
			initHeaderBlock(&runningProcess,"ar",TRUE, TRUE,SystemCallData->Argument[1]);
			initSwapArea(&runningProcess);
			flushBitmap(&runningProcess);
			break;
		case SYSNUM_CHECK_DISK:
			CheckDisk(SystemCallData->Argument[0], SystemCallData->Argument[1]);
			break;
		case SYSNUM_OPEN_DIR:
			runningProcess->diskId = SystemCallData->Argument[0];
			openDir(&runningProcess, SystemCallData->Argument[1], SystemCallData->Argument[2]);
			flushBitmap(&runningProcess);	
			printWholePCB();		
			break;
		case SYSNUM_CREATE_DIR:
			createDir(&runningProcess, SystemCallData->Argument[0],SystemCallData->Argument[1]);
			flushBitmap(&runningProcess);
			printWholePCB();
			break;
		case SYSNUM_CREATE_FILE:
			createFile(&runningProcess, SystemCallData->Argument[0],SystemCallData->Argument[1]);
			flushBitmap(&runningProcess);	
			printWholePCB();		
			break;
		case SYSNUM_OPEN_FILE:
			openFile(&runningProcess, SystemCallData->Argument[0], SystemCallData->Argument[1], SystemCallData->Argument[2]);
			flushBitmap(&runningProcess);
			printWholePCB();
			break;
		case SYSNUM_WRITE_FILE:
			writeFile(&runningProcess, SystemCallData->Argument[0], SystemCallData->Argument[1], SystemCallData->Argument[2], SystemCallData->Argument[3]);
			printWholePCB();
			break;
		case SYSNUM_CLOSE_FILE:
			closeFile(&runningProcess, SystemCallData->Argument[0], SystemCallData->Argument[1]);
			printWholePCB();
			break;
		case SYSNUM_READ_FILE:
			readFile(&runningProcess, SystemCallData->Argument[0], SystemCallData->Argument[1], SystemCallData->Argument[2], SystemCallData->Argument[3]);
			printWholePCB();
			break;
		case SYSNUM_DIR_CONTENTS:
			dirContents(&runningProcess, SystemCallData->Argument[0]);
			break;
		case  SYSNUM_DEFINE_SHARED_AREA:
			defineArea(SystemCallData->Argument[0], SystemCallData->Argument[1], SystemCallData->Argument[2], SystemCallData->Argument[3], SystemCallData->Argument[4]);
			break;
		case SYSNUM_SEND_MESSAGE:
			sendMessage(SystemCallData->Argument[0], SystemCallData->Argument[1],SystemCallData->Argument[2],SystemCallData->Argument[3]);
			printWholePCB();
			break;
		case SYSNUM_RECEIVE_MESSAGE:
			if (runningProcess->hasMails == FALSE){
					addToSuspendQ(runningProcess);
					runningProcess = NULL;
					dispatcher();
			}
			receiveMessage(SystemCallData->Argument[0],SystemCallData->Argument[1],SystemCallData->Argument[2],SystemCallData->Argument[3],
				SystemCallData->Argument[4],SystemCallData->Argument[5]);
			printWholePCB();
			break;
	}
}                                               // End of svc

// send message to processid   -1 means broad cast
void sendMessage(INT32 processId, char messageBuffer[], INT32 sendLength, INT32 *errorCode){
	if (processId == -1){
		for (int i = 2; i < curNumProcess; i++){
			Message *newOne = (Message *)calloc (1, sizeof(Message));
			newOne->fromPid = runningProcess->processId;
			newOne->contents = messageBuffer;
			newOne->messageLength = sendLength;
			wholePcb[i]->mailBox[boxFindFreePlace(wholePcb[i])] = *newOne;
			wholePcb[processId]->hasMails = TRUE;			
		}
	} else {
		Message *newOne = (Message *)calloc (1, sizeof(Message));
		newOne->fromPid = runningProcess->processId;
		newOne->contents = messageBuffer;
		newOne->messageLength = sendLength;
		// printf("%d   \n",wholePcb[processId]->processId);
		int freeOne = boxFindFreePlace(wholePcb[processId]);
		wholePcb[processId]->mailBox[boxFindFreePlace(wholePcb[processId])] = *newOne;
		wholePcb[processId]->hasMails = TRUE;
		PcbNode *canReceive;
		// canReceive = dequeueFromSuspendtQueueByPid(processId);
		canReceive = wholePcb[processId];
		addToReadyQueue(canReceive);
		dispatcher();
		// printf("\nprocessId %d mailbox[%d] = %d\n", processId,  freeOne,wholePcb[processId]->mailBox[freeOne].contents[0]);
	}
}
// find free place in mailbox 
int boxFindFreePlace(PcbNode *pcb){
	for (int i = 0; i < 512; i++){
		if (pcb->mailBox[i].fromPid == 0){
			// printf("find free box place %d", i);
			return i;
		}
	}
}
// find message by using sender process id
Message getMessageByPID(INT32 senderId){
	for (int i = 0; i < 512; i++){
		if(runningProcess->mailBox[i].fromPid != 0){
			if (runningProcess->mailBox[i].fromPid == senderId){
				Message ans = runningProcess->mailBox[i];
				runningProcess->mailBox[i].fromPid = 0;
				return ans;
			} else if (senderId == -1){
				Message ans = runningProcess->mailBox[i];
				runningProcess->mailBox[i].fromPid = 0;				
				return ans;
			}
		}
	}
}
// receive message
void receiveMessage(INT32 SourcePid, char messageBuffer[], INT32 MessageReceiveLength, 
	INT32 *MessageSendLength, INT32 *MessageSenderPid, INT32 *ErrorReturned){
		if (runningProcess->hasMails == TRUE){
			Message ans = getMessageByPID(SourcePid);
			messageBuffer =ans.contents;
			*MessageSendLength = ans.messageLength;
			*MessageSenderPid = ans.fromPid;
			runningProcess->hasMails = FALSE;
		} 
		
	}
/************************************************************************
printWholePCB

a wrapper function to generate requried input data for SPPrintLine to show
the all processes information.
 ************************************************************************/
void printWholePCB(void){
	SP_INPUT_DATA *input;
	input = (SP_INPUT_DATA *)calloc(1, sizeof(SP_INPUT_DATA));
	input->CurrentlyRunningPID = runningProcess->processId;
	if (readyQ->next != readyQ){
		input->TargetPID = readyQ->next->processId;
	} else {
		input->TargetPID = -1;
	}
	
	input->NumberOfRunningProcesses = 1;
	// input->RunningProcessPIDs
	input->NumberOfReadyProcesses = countOfReadyQ;
	input->NumberOfTimerSuspendedProcesses = countOfTimerQ;
	int tempIdTimer = 0;
	int tempIdDisk = 0;
	int tempIdTerminate = 0;
	for (int i = 0; i < WHOLE_PCB_LEN; i++){
		if(wholePcb[i] == NULL)continue;
		PcbNode *temp = wholePcb[i];
		if(temp->state == WAITING_TIMER){
			input->TimerSuspendedProcessPIDs[tempIdTimer++] = temp->processId;
		} else if (temp ->state == WAITING_DISK_READ || temp->state == WAITING_DISK_WRITE
		|| temp->state == DISK_IS_READING || temp->state == DISK_IS_WRITING){
			input->DiskSuspendedProcessPIDs[tempIdDisk++] = temp->processId;
		} else if (temp ->state == TERMINATED){
			input->TerminatedProcessPIDs[tempIdTerminate++] = temp->processId;
		}
	}
	input->NumberOfTerminatedProcesses = tempIdTerminate + 1;
	SPPrintLine(input);
}

/************************************************************************
getProcessId
get processId, if input string is "", it means get currently running one.
otherwise, loop through the whole pcb array
 ************************************************************************/
void getProcessId(char *name, long *output, long *errorCode){
	// debug use
	if (strcmp(name, "") == 0){
		*errorCode = ERR_SUCCESS;
		*output = runningProcess->processId;
		return;
	}
	for (int i = 0; i < WHOLE_PCB_LEN; i++){
		if (wholePcb[i] == NULL)continue;
		if (strcmp(wholePcb[i]->processName, name) == 0 && wholePcb[i]->state != TERMINATED){
			*output = wholePcb[i]->processId;
			*errorCode = ERR_SUCCESS;
			return;
		}
	}	
	*errorCode = ERR_PROCESS_NOT_EXIST;
	return;
}
/************************************************************************
anyWorkRunning
are currently any work is running.
 ************************************************************************/
BOOL anyWorkRunning(){
	if (runningProcess == NULL){
		return FALSE;
	} else return TRUE;
}

/************************************************************************
dispatcher
check the ready queue to see any available process can run, called when runningProcess = null;
 ************************************************************************/
void dispatcher(void){
	MEMORY_MAPPED_IO mmio;
	if (anyWorkRunning() == TRUE){
		
		// printf("there is a process currently running no need to dispatch \n");
		return NULL;
	} 	
	// check are there any more pcb not terminated
	long *errorCode;
	if (anyMore() == FALSE){
		// no more process, halt machine
		osTerminateProcess(-2l, &errorCode);
		return;
	}
	int i = 0;
	
	while (nothingInReadyQ() == TRUE){
		// printf("nothing in ready queue");
		// printf("process1 is %d", wholePcb[1]->state);
		CALL(WasteTime());
	}
	runningProcess = dequeueFromReadyQueue();
	runningProcess->state = RUNNING;
	mmio.Mode = Z502StartContext;
	mmio.Field1 = runningProcess->contextId;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	mmio.Field3 = mmio.Field4 = 0;
	MEM_WRITE(Z502Context, &mmio);  
}

/************************************************************************
WasteTime
move simulation forward
 ************************************************************************/
void WasteTime(){

}
/************************************************************************
 osTerminateProcess

 This function is to terminate process and its' control block
 ************************************************************************/
void osTerminateProcess(long ProcessId, long *errorCode){
	MEMORY_MAPPED_IO mmio;
	if (ProcessId == (long)-2){
		// halt machine
		mmio.Mode = Z502Action;
		mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
		MEM_WRITE(Z502Halt, &mmio);
		return;
	}
	
	if (ProcessId == (long)-1){
		// terminate itself
		// just release pcb and call dispatcher
		
		runningProcess->state = TERMINATED;
		if (anyMore() == FALSE)	{
			updateSwapArea(&runningProcess);
		}	
		runningProcess->state = TERMINATED;
		runningProcess = NULL;
		dispatcher();
		return;
	}
	// in running process to terminate other process
	if (ProcessId != runningProcess->processId){
		killPCB(ProcessId, errorCode);
		dispatcher();
		return;
	}
}
/************************************************************************
 osCreateProcess

 This function is to create process control block, and save them in a linked list
 structure, which is defined in the head.
 ************************************************************************/

 void osCreateProcess(char *name, long tempTest, int priority, long *PID, long *errorCode){
	if (curNumProcess > MAX_EXISTING_PCB_NUM){
		// printf("\n");
		// printf("run out of PCB!!!!!!!!!!!!!!!!!");
		*errorCode = ERR_PROCESS_OVERFLOW;
		return;
	}
	if (priority < 0){
		// printf("\n");
		// printf("priority is illegal");
		*errorCode = ERR_PRIORITY;		
		return;
	}
	if (checkExistByName(name) == TRUE){
		*errorCode = ERR_PROCESS_NAME_OVERLAPPING;
		return;
	}
	*errorCode = ERR_SUCCESS;
	MEMORY_MAPPED_IO mmio;
	void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	PcbNode *item;
	item = (PcbNode *)calloc(1, sizeof(PcbNode));
	item->state = NEW;	
	item->processName = strdup(name);
	// printf("process name is!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! %s", name);
	item->next = NULL;
	item->prev = NULL;
	item->processId = ++curNumProcess;
	*PID = item->processId;
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 =  tempTest;
	mmio.Field3 =  (long)PageTable;	
	MEM_WRITE(Z502Context, &mmio);
	item->contextId = mmio.Field1;	
	item->pageTable = (short *)PageTable;
	Message *mailBox = (Message *)calloc (512, sizeof(Message));
	item->mailBox = mailBox;
	item->fromMem = FALSE;
	item->hasMails = FALSE;
	memset(item->swapedFrame, -1, NUMBER_VIRTUAL_PAGES*sizeof(item->swapedFrame[0]));
	addToReadyQueue(item);
	wholePcb[item->processId] = item;
}
/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/
void osInit(int argc, char *argv[]) {
	void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	INT32 i;
	MEMORY_MAPPED_IO mmio;
	long ErrorReturned;
	
	long ProcessID1;
	long priority = 0;
	// Demonstrates how calling arguments are passed thru to here
	// initialize timeQ, readyQ, diskQs
	//*************************************************
	timerQ = (LinkedPcb) calloc(1, sizeof(PcbNode));
	timerQ->next = timerQ;
	timerQ->prev = timerQ;
	for (int i = 0; i < 8; i++){
		LinkedPcb diskQ;
		diskQ = (LinkedPcb) calloc(1, sizeof(PcbNode));
		diskQ->next = diskQ;
		diskQ->prev = diskQ;
		diskArray[i] = diskQ;
	}
	readyQ = (LinkedPcb) calloc(1, sizeof(PcbNode));
	readyQ->next = readyQ;
	readyQ->prev = readyQ;
	susQ = (LinkedPcb) calloc(1, sizeof(PcbNode));
	susQ->next = susQ;
	susQ->prev = susQ;
	//**************************************************
	printf("Program called with %d arguments:", argc);
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("Calling with argument 'sample' executes the sample program.\n");

	// Here we check if a second argument is present on the command line.
	// If so, run in multiprocessor mode.  Note - sometimes people change
	// around where the "M" should go.  Allow for both possibilities
	if (argc > 2) {
		if ((strcmp(argv[1], "M") ==0) || (strcmp(argv[1], "m")==0)) {
			strcpy(argv[1], argv[2]);
			strcpy(argv[2],"M\0");
		}
		if ((strcmp(argv[2], "M") ==0) || (strcmp(argv[2], "m")==0)) {
			printf("Simulation is running as a MultProcessor\n\n");
			mmio.Mode = Z502SetProcessorNumber;
			mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
			mmio.Field2 = (long) 0;
			mmio.Field3 = (long) 0;
			mmio.Field4 = (long) 0;
			MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
		}
	} else {
		printf("Simulation is running as a UniProcessor\n");
		printf(
				"Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
	}

	//          Setup so handlers will come to code in base.c

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR ] = (void *) InterruptHandler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR ] = (void *) FaultHandler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR ] = (void *) svc;

	//  Determine if the switch was set, and if so go to demo routine.
	
	PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {
		mmio.Mode = Z502InitializeContext;
		mmio.Field1 = 0;
		mmio.Field2 = (long) SampleCode;
		mmio.Field3 = (long) PageTable;

		MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence
		mmio.Mode = Z502StartContext;
		// Field1 contains the value of the context returned in the last call
		mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
		MEM_WRITE(Z502Context, &mmio);     // Start up the context

	} 
	
	printf("input test case\n");
	int testNum;
	fflush(stdin);
	scanf("%d", &testNum);
	
	long tempTest;
	switch (testNum){
		case 0:
			tempTest = (long)test0;
			break;
		case 1:
			tempTest = (long)test1;
			break;
		case 2:
			tempTest = (long)test2;
			break;
		case 3:
			tempTest = (long)test3;
			break;
		case 4:
			tempTest = (long)test4;
			break;
		case 5:
			tempTest = (long)test5;
			break;
		case 6:
			tempTest = (long)test6;
			break;
		case 7:
			tempTest = (long)test7;
			break;
		case 8:
			tempTest = (long)test8;
			break;
		case 9:
			tempTest = (long)test9;
			break;
		case 10:
			tempTest = (long)test10;
			break;
		case 11:
			tempTest = (long)test11;
			break;
		case 12:
			tempTest = (long)test12;
			break;
		case 13:
			tempTest = (long)test13;
			break;
		case 21:
			tempTest = (long)test21;
			break;
		case 22:
			tempTest = (long)test22;
			break;
		case 23:
			tempTest = (long)test23;
			break;
		case 24:
			tempTest = (long)test24;
			break;
		case 25:
			tempTest = (long)test25;
			break;
		case 26:
			tempTest = (long)test26;
			break;
		case 27:
			tempTest = (long)test27;
			break;
		case 28:
			tempTest = (long)test28;
			break;
	}
	osCreateProcess("original test", tempTest, priority, &ProcessID1, &ErrorReturned);	
	dispatcher();
}                                               // End of osInit
