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


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList[AMOUNTPRIORITIES];

// current process ID
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

    // initialize the process table
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");

    // Initialize the clock interrupt handler
    // 

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
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
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
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
    int procSlot = -1;
    procPtr new_process = NULL;
    union psrValues current_psr_status;

    current_psr_status.integerPart = USLOSS_PsrGet();


    // add debugging
    if (DEBUG && debugflag) {
        USLOSS_Console("fork1(): creating process %s\n", name);
    }


    // test if in kernel mode, halt if in user mode
    if (current_psr_status.bits.curMode == 0) {
        USLOSS_Console("fork1(): Processor is in user mode. Halting...\n");
        USLOSS_Halt(1);
    }


    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        USLOSS_Console("fork1(): Stacksize is too small. Returning -2\n");
        return -2;
    }


    // check if name, startFunc and priorities are valid
    if (name == NULL || startFunc == NULL || priority < 1 || priority > 6) {
        USLOSS_Console("fork1(): Stuff was null. Returning -1\n");
        return -1;
    }


    // find an empty slot in the process table, and change PID
    nextPid++;
    for (int i = 0; i < 50; i++) {
        
        if (ProcTable[nextPid % 50].status == UNUSED) {
            procSlot = nextPid % 50;
            break;
        }

        nextPid++;
    }


    // check if table is full
    if (procSlot == -1) {
        USLOSS_Console("fork1(): Process table full. Returning -1.\n");
        return -1;
    }

    new_process = &ProcTable[procSlot];


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
    strcpy(new_process->startArg, arg);
    new_process->startFunc = startFunc;
    new_process->priority = priority;
    new_process->stackSize = stacksize;
    new_process->status = READY;


    // set up procPtr
    new_process->nextProcPtr = NULL;
    new_process->childProcPtr = NULL;
    new_process->nextSiblingPtr = NULL;
    new_process->quitChildProcPtr = NULL;
    new_process->nextQuitSibling = NULL;
    new_process->parentProcPtr = Current;


    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(new_process->state), USLOSS_PsrGet(),
                       new_process->stack,
                       new_process->stackSize,
                       launch);


    // add child to parent children list
    if (Current->childProcPtr != NULL) {
        procPtr current_proc = Current->childProcPtr;

        while (current_proc->nextSiblingPtr != NULL) {
            current_proc = current_proc->nextSiblingPtr;
        }

        current_proc->nextSiblingPtr = new_process;
    }
    else {
        Current->childProcPtr = new_process;
    }


    // insert into ready list
    procPtr temp_process = ReadyList[priority];

    if (temp_process != NULL) {
        while (temp_process->nextProcPtr != NULL) {
            temp_process = temp_process->nextProcPtr; 
        }
    }

    temp_process = new_process;
    
    // call dispatcher to switch context if needed
    if (startFunc != sentinel) {
        dispatcher();
    } 

    // for future phase(s)
    p1_fork(new_process->pid);

    return nextPid;
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
    union psrValues current_psr_status;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    
    // Enable interrupts
    current_psr_status.integerPart = USLOSS_PsrGet();
    current_psr_status.bits.curIntEnable = 1;
    USLOSS_PsrSet(current_psr_status.integerPart);

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

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
    procPtr children, quit_children, ready;
    
    children = Current->childProcPtr;
    quit_children = Current->quitChildProcPtr;
    ready = NULL;


    // check to see that you have no children
    if (children == NULL && quit_children == NULL) {
        return -2;
    }


    // check your quit_children list to check if you have any quit
    // child processes
    if (quit_children != NULL) {
        *status = quit_children->exit_status;
        quit_children->status = UNUSED;
        delete_node(&quit_children, quit_children, QUITLIST);

        return quit_children->pid;
    }
    else {
        // no children have quit yet, you must join block the parent
        Current->status = JOINBLOCKED;
    }


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


    // check that this process has no children
    if (parent->childProcPtr != NULL) {
        USLOSS_Console("quit(): Current has active children.\n");
        USLOSS_Halt(1);
    }

    
    // store the exit status into the exit_status struct member
    Current->exit_status = status;

    
    // delete from parents children list and add to parents quit children list
    delete_node(&parent->childProcPtr, toQuit, CHILDRENLIST);
    add_node(&parent->quitChildProcPtr, toQuit, QUITLIST);

    
    // if parent is blocked child must unblock it
    if (parent->status == JOINBLOCKED) {
        parent->status = READY;
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
    procPtr nextProcess = NULL;

    // loop through ready list in order of greatest priority

    for (int i = MAXPRIORITY; i < AMOUNTPRIORITIES; i++) {

        if (ReadyList[i] != NULL) {
            nextProcess = ReadyList[i];
            
            // if process is anything but ready, it is removed
            while (nextProcess->status != READY) {
                delete_node(&ReadyList[i], nextProcess, READYLIST);
                nextProcess = ReadyList[i];
            }

            if (nextProcess != NULL) {
                break;
            }
        }
    }

    
    if (Current->pid != nextProcess->pid) {
        USLOSS_ContextSwitch(&Current->state, &nextProcess->state);
        p1_switch(Current->pid, nextProcess->pid);
        Current = nextProcess;
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
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */



/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
        // We ARE in kernel mode
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */



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
    }

    return 0;
} /* delete_node */



/* ------------------------------------------------------------------------
   Name - delete node
   Purpose - deletes a node from a list (either the readylist or sibling list)
   Parameters - the head of the list and the address of the node to be removed
   Returns - and int indicating whether the node was successfully remove
   Side Effects - removes the specified node from the list
   ------------------------------------------------------------------------ */
int add_node(procPtr *head, procPtr to_add, list_to_change which_list) {
    procPtr scout;

    scout = *head;

    if (scout == NULL) {
        scout = to_add;
        return 1;
    }

    switch (which_list) {
        case READYLIST:
             while (scout->nextProcPtr != NULL) {
                scout = scout->nextProcPtr;
            }

            scout->nextProcPtr = to_add;
            return 1;

        case QUITLIST:
            while (scout->nextQuitSibling != NULL) {
                scout = scout->nextQuitSibling;
            }

            scout->nextQuitSibling = to_add;
            return 1;

        case CHILDRENLIST:
            while (scout->nextSiblingPtr != NULL) {
                scout = scout->nextSiblingPtr;
            }

            scout->nextSiblingPtr = to_add;
            return 1;
    }

    return 0;
} /* delete_node */









































