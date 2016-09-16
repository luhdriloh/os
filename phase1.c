/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
int delete_node(procPtr *head, procPtr to_delete, list_to_change which_list);
int add_node(procPtr *head, procPtr to_add, list_to_change which_list);


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC] = {};

// Process lists
static procPtr ReadyList[AMOUNTPRIORITIES];

// current process
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
    int result; // value returned by call to fork1()
    Current = NULL;

    // initialize the process table
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clock_interrupt_handler;

    // startup a sentinel process
    if (DEBUG && debugflag) {
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    }
    

    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);


    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag) {
        USLOSS_Console("startup(): calling fork1() for start1\n");
    }


    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);


    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }


    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */


/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */


/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int i, newpid, procSlot = -1;
    procPtr new_process = NULL;


    // add debugging
    if (DEBUG && debugflag) {
        USLOSS_Console("fork1(): creating process %s\n", name);
    }


    // test if in kernel mode, halt if in user mode, then enable interrupts
    if (check_user_mode()) {
        USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }


    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        return -2;
    }


    // check if name, startFunc and priorities are valid
    if (name == NULL || startFunc == NULL || priority < 1 || priority > 6) {
        return -1;
    }

    // find an empty slot in the process table, and change PID
    for (i = 0; i < MAXPROC; i++) {
        
        if (ProcTable[nextPid % MAXPROC].status == UNUSED) {
            procSlot = nextPid % MAXPROC;
            break;
        }
        nextPid++;
    }

    newpid = nextPid;
    nextPid++;


    // check if table is full and set pid, then disable interupts
    if (procSlot == -1) {
        return -1;
    }

    new_process = &ProcTable[procSlot];
    new_process->pid = newpid;

    disableInterrupts();


    // fill-in entry in process table */
    if (strlen(name) >= (MAXNAME - 1)) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    

    // copy in name of process into process, and argument
    strcpy(new_process->name, name);
    new_process->startFunc = startFunc;
    
    if (arg == NULL) {
        new_process->startArg[0] = '\0';
    }
    else if (strlen(arg) >= (MAXARG - 1)) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else {
        strcpy(new_process->startArg, arg);
    }

    
    // create new stack space
    char *new_stack = malloc(sizeof(char) * stacksize);
    if (new_stack == NULL) {
        USLOSS_Console("fork1(): Error when allocating stack space.\n");
        USLOSS_Halt(1);
    }
    
    new_process->stack = new_stack;


    // save the start function, arg, priority,
    new_process->startFunc = startFunc;
    new_process->priority = priority;
    new_process->stackSize = stacksize;
    new_process->status = READY;
    new_process->exit_status = 0;
    new_process->zapped = 0;
    new_process->total_time_used = 0;
    new_process->time_slice_start = 0;


    // set up procPtr
    new_process->nextProcPtr = NULL;
    new_process->childProcPtr = NULL;
    new_process->nextSiblingPtr = NULL;
    new_process->quitChildProcPtr = NULL;
    new_process->nextQuitSibling = NULL;
    new_process->zappedProcPtr = NULL;
    new_process->zappersProcPtr = NULL;
    new_process->nextZapperSibling = NULL;
    new_process->parentProcPtr = Current;


    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(new_process->state), USLOSS_PsrGet(),
                       new_process->stack,
                       new_process->stackSize,
                       launch);


    // add child to parent children list
    if (Current != NULL) {
        add_node(&Current->childProcPtr, new_process, CHILDRENLIST);
    }


    // insert into ready list
    procPtr *readylist = &ReadyList[priority];
    add_node(readylist, new_process, READYLIST);
    
    
    // for future phase(s)
    p1_fork(new_process->pid);


    // call dispatcher to switch context if needed
    if (startFunc != sentinel) {
        dispatcher();
    } 


    return newpid;
} /* fork1 */


/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;


    // debug statement and enable interupt for launch
    if (DEBUG && debugflag) {
        USLOSS_Console("launch(): started\n");
    }

    enableInterrupts();
    

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag) {
        USLOSS_Console("Process %d returned to launch\n", Current->pid);
    }

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    // block off the parents
    procPtr children, quit_children;
    
    children = Current->childProcPtr;
    quit_children = Current->quitChildProcPtr;


    // check to see that you have no children
    if (children == NULL && quit_children == NULL) {
        return -2;
    }


    // check your quit_children list to check if you have any quit
    // child processes
    if (quit_children != NULL) {
        *status = quit_children->exit_status;
        quit_children->status = UNUSED;

        // delete of the parents quit list and also from the readylist
        delete_node(&Current->quitChildProcPtr, quit_children, QUITLIST);
        delete_node(&ReadyList[quit_children->priority], quit_children, READYLIST);

        // check to see if you are already zapped
        if (isZapped()) {
            return -1;
        }

        return quit_children->pid;
    }
    else {
        // no children have quit yet, you must join block the parent
        Current->status = JOINBLOCKED;
    }


    // call dispatcher, then check if process got zapped
    dispatcher();

    return join(status);
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents' child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    procPtr toQuit = Current;
    procPtr parent = toQuit->parentProcPtr;


    // check if in user mode
    if (check_user_mode()) {
        USLOSS_Console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }

    // check that this process has no children
    if (toQuit->childProcPtr != NULL) {
        USLOSS_Console("quit(): process 2, 'start1', has active children. Halting...\n");
        USLOSS_Halt(1);
    }

    disableInterrupts();
    
    // store the exit status into the exit_status struct member
    Current->exit_status = status;

    
    // change the status to quit
    Current->status = QUIT;


    // delete from parents children list and add to parents quit children list
    if (parent != NULL) {
        // here is the error
        delete_node(&parent->childProcPtr, toQuit, CHILDRENLIST);
        add_node(&parent->quitChildProcPtr, toQuit, QUITLIST);
   	    
        // if parent is blocked child must unblock it
   	    if (parent->status == JOINBLOCKED) {
            unblockRegularProc(parent->pid);
        }
    }


    // unblock all zappers
    if (Current->zappersProcPtr != NULL) {
        procPtr scout = Current->zappersProcPtr; 
        
        while (scout != NULL) {
            unblockRegularProc(scout->pid);
            scout = scout->nextZapperSibling;
        }
    }
    

    p1_quit(Current->pid);
    dispatcher();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr next_process = NULL;
    int i;


    // loop through ready list in order of greatest priority
    for (i = MAXPRIORITY; i < AMOUNTPRIORITIES; i++) {

        if (ReadyList[i] != NULL) {
            next_process = ReadyList[i];
            
            // if process is anything but ready, it is removed
            while (next_process != NULL && next_process->status != READY) {
                delete_node(&ReadyList[i], next_process, READYLIST);
                next_process = ReadyList[i];
            }

            if (next_process != NULL) {
                break;
            }
        }
    }


    // set the pointers to the new processes
    procPtr old_process = Current;
    Current = next_process;


    // add the process to the end of the readylist
    delete_node(&ReadyList[Current->priority], Current, READYLIST);
    add_node(&ReadyList[Current->priority], Current, READYLIST);
    Current->nextProcPtr = NULL;

    
    // dont save the state of the process at the very beginning if sentinel, or
    // if the old process' state is QUIT, enable interrupts
    if (old_process == NULL || old_process->status == QUIT) {
        enableInterrupts();
        USLOSS_ContextSwitch(NULL, &Current->state);
    }
    else {
        enableInterrupts();
        p1_switch(old_process->pid, Current->pid);
        USLOSS_ContextSwitch(&old_process->state, &Current->state);
    }

} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag) {
        USLOSS_Console("sentinel(): called\n");
    }

    while (1) {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */



/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    int i;

    // check through all of the ReadyList
    for (i = 0; i < MAXPROC; i++) {

        // make sure all processes are done
        if (ProcTable[i].priority != SENTINELPRIORITY && ProcTable[i].status == ZAPBLOCKED) {
            USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", ProcTable[i].pid);
            USLOSS_Halt(1);
        }
    }

    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    }
    else {
        // We ARE in kernel mode
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT);
    }
} /* disableInterrupts */


/*
 * Enables the interrupts.
 */
void enableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("enable interrupts\n");
        USLOSS_Halt(1);
    }
    else {
        // We ARE in kernel mode
        USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    }
} /* enableInterrupts */



/* ------------------------------------------------------------------------
   Name - delete node
   Purpose - deletes a node from a list (either the readylist or sibling list)
   Parameters - the head of the list and the address of the node to be removed
   Returns - and int indicating whether the node was successfully remove
   Side Effects - removes the specified node from the list
   ------------------------------------------------------------------------ */
int delete_node(procPtr *head, procPtr to_delete, list_to_change which_list) {
    procPtr scout;

    scout = *head;

    if (scout == NULL) {
        return 0;
    }

    switch (which_list) {
        case READYLIST:
            if (scout == to_delete) {
                *head = scout->nextProcPtr;
                return 1;
            }

            while (scout->nextProcPtr != NULL) {
                if (scout->nextProcPtr == to_delete) {
                    scout->nextProcPtr = scout->nextProcPtr->nextProcPtr;
                    return 1;
                }

                scout = scout->nextProcPtr;
            }

            return 0;

        case QUITLIST:
            if (scout == to_delete) {
                *head = scout->nextQuitSibling;
                return 1;
            }

            while (scout->nextQuitSibling != NULL) {
                if (scout->nextQuitSibling == to_delete) {
                    scout->nextQuitSibling = scout->nextQuitSibling->nextQuitSibling;
                    return 1;
                }

                scout = scout->nextQuitSibling;
            }

            return 0;

        case CHILDRENLIST:
            if (scout == to_delete) {
                *head = scout->nextSiblingPtr;
                return 1;
            }
	
             while (scout->nextSiblingPtr != NULL) {
                if (scout->nextSiblingPtr == to_delete) {
                    scout->nextSiblingPtr = scout->nextSiblingPtr->nextSiblingPtr;
                    return 1;
                }

                scout = scout->nextSiblingPtr;
            }

            return 0;

        case ZAPPERLIST:
            if (scout == to_delete) { 
                *head = scout->nextZapperSibling;
                return 1;
            }
    
             while (scout->nextZapperSibling != NULL) {
                if (scout->nextZapperSibling == to_delete) {
                    scout->nextZapperSibling = scout->nextZapperSibling->nextZapperSibling;
                    return 1;
                }

                scout = scout->nextZapperSibling;
            }

            return 0;
    }

    return 0;
} /* deletenode */



/* ------------------------------------------------------------------------
   Name - add node
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int add_node(procPtr *head, procPtr to_add, list_to_change which_list) {
    procPtr scout;
    scout = *head;

    if (*head == NULL) {
        *head = to_add;
        return 1;
    }

    switch (which_list) {
        case READYLIST:
             while (scout->nextProcPtr != NULL) {
                
                if (scout == to_add) {
                    USLOSS_Console("THE HORSE HAS POUNCED ON YOU!");
                    USLOSS_Halt(1);
                }
                scout = scout->nextProcPtr;
            }

            scout->nextProcPtr = to_add;
            return 1;

        case QUITLIST:
            while (scout->nextQuitSibling != NULL) {
                
                if (scout == to_add) {
                    USLOSS_Console("THE HORSE HAS POUNCED ON YOU!");
                    USLOSS_Halt(1);
                }
                scout = scout->nextQuitSibling;
            }

            scout->nextQuitSibling = to_add;
            return 1;

        case CHILDRENLIST:
            while (scout->nextSiblingPtr != NULL) {
                
                if (scout == to_add) {
                    USLOSS_Console("THE HORSE HAS POUNCED ON YOU!");
                    USLOSS_Halt(1);
                }
                scout = scout->nextSiblingPtr;
            }

            scout->nextSiblingPtr = to_add;
            return 1;

        case ZAPPERLIST:
            while (scout->nextZapperSibling != NULL) {
                
                if (scout == to_add) {
                    USLOSS_Console("THE HORSE HAS POUNCED ON YOU!");
                    USLOSS_Halt(1);
                }
                scout = scout->nextZapperSibling;
            }

            scout->nextZapperSibling = to_add;
            return 1;
    }

    return 0;
} /* add_node */


/* ------------------------------------------------------------------------
   Name -  clock_interrupt_handler
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void clock_interrupt_handler(int dev, void *arg) {
    timeSlice();
} /* clock_interrupt_handler */



/* ------------------------------------------------------------------------
   Name -  readCurStartTime
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int readCurStartTime() {
    return Current->time_slice_start;
} /* readCurStartTime */


/* ------------------------------------------------------------------------
   Name -  dumpProcesses
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void dumpProcesses() {
    int i;

    printf("PID\tParent  Priority\tStatus\t\t# Kids  CPUtime Name\n");

    for (i = 0; i < MAXPROC; i++) {
        procStruct current = ProcTable[i];
        printf("%5d\t", current.pid);
    	
    	if (current.parentProcPtr == NULL) {
    	    printf("%5d\t", -2);	
    	}
    	else {
            printf("%5d\t", current.parentProcPtr->pid);   
	}

        printf("%10d\t", current.priority);
        printf("%5d\t", current.status);
        printf("%8d\t", 5);
        printf("%5d\t", 10);
        printf("%s\t\n", current.name);
    }
} /* dumpProcesses */


/* ------------------------------------------------------------------------
   Name - zap
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int zap(int pid) {
    procPtr process_to_zap;

    process_to_zap = &ProcTable[pid % MAXPROC];


    // check calling process is not zapped itself
    if (isZapped()) {
        return -1;
    }


    // check if you tried to zap yourself
    if (Current->pid == process_to_zap->pid) {
        USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }


    // check zapped process exists and that it is not itself
    if (process_to_zap->status == UNUSED || process_to_zap->pid != pid) {
        USLOSS_Console("zap(): process being zapped does not exist.  Halting...\n");
        USLOSS_Halt(1);
    }

    
    // if process has quit then return 0
    if (process_to_zap->status == QUIT) {
        return 0;
    }


    // set the statuses
    process_to_zap->zapped = 1;
    Current->status = ZAPBLOCKED;


    // set the zapped pointer to the zapped process
    Current->zappedProcPtr = process_to_zap;


    // add current process to list of zappers, and delete Current off readylist
    delete_node(&ReadyList[Current->priority], Current, READYLIST);
    add_node(&process_to_zap->zappersProcPtr, Current, ZAPPERLIST);


    dispatcher();

    // check calling process is not zapped itself
    if (isZapped()) {
        return -1;
    }

    return 0;
} /* zap */


/* ------------------------------------------------------------------------
   Name - isZapped
   Purpose - 
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int isZapped() {
    return Current->zapped;
} /* isZapped */


/* ------------------------------------------------------------------------
   Name - getPid
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int getpid() {
    return Current->pid;
} /* getpid */


/* ------------------------------------------------------------------------
   Name - blockMe
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int blockMe(int newStatus) {

    // halt and print error message if newstatus is not a block status
    if (newStatus < 10) {
        USLOSS_Console("blockMe(): newStatus parameter is less than 10.\n");
        USLOSS_Halt(1);
    }

    
    // process attempted to block while zapped
    if (isZapped()) {
        return -1;
    }


    // change status and take off the ReadyList
    disableInterrupts();
    Current->status = newStatus;
    delete_node(&ReadyList[Current->priority], Current, READYLIST);


    // call dispatcher then check if process was zapped while blocked
    dispatcher();

    if (isZapped()) {
        return -1;
    }

    return 0;
} /* blockMe */




/* ------------------------------------------------------------------------
   Name - unblockProc
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int unblockProc(int pid) {
    disableInterrupts();
    unblockRegularProc(pid);
    dispatcher();

    return 0;
} /* unblockProc */


/* ------------------------------------------------------------------------
   Name - unblockRegularProc
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int unblockRegularProc(int pid) {
    procPtr process_to_unblock;

    process_to_unblock = &ProcTable[pid % MAXPROC];

    // change the status of the process to READY and put it into the ReadyList
    process_to_unblock->status = READY;
    add_node(&ReadyList[process_to_unblock->priority], process_to_unblock, READYLIST);


    return 0;
} /* unblockRegularProc */


/* ------------------------------------------------------------------------
   Name - timeSlice
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void timeSlice() {
    // int current_time = USLOSS_Clock();

    // if (Current->time_slice_start - current_time > 80) {
    //     dispatcher();
    // }
} /* timeSlice */



/* ------------------------------------------------------------------------
   Name - readtime
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int readtime() {
    return Current->total_time_used + readCurStartTime();
} /* readtime */



/* ------------------------------------------------------------------------
   Name - check_user_mode
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
int check_user_mode() {
    union psrValues current_psr_status;

    current_psr_status.integerPart = USLOSS_PsrGet();

    // test if in kernel mode, halt if in user mode
    if (current_psr_status.bits.curMode == 0) {
        return 1;
    }

    return 0;
} /* check_user_mode */






















