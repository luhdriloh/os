/* Patrick's DEBUG printing constant... */
#define DEBUG 0

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
  /* Head Pointer */
   procPtr         parentProcPtr;     
   procPtr         childProcPtr;
   procPtr         quitChildProcPtr;

   /* Next in List Pointer */
   procPtr         nextProcPtr;       /* for readylist */
   procPtr         nextSiblingPtr;
   procPtr         nextQuitSibling;

   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             exit_status;
   /* other fields as needed... */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};

extern void  clock_interrupt_handler(int dev, void *arg);
extern int   delete_node(procPtr *head, procPtr to_delete, list_to_change which_list);
extern int   add_node(procPtr *head, procPtr to_add, list_to_change which_list);


/* Status of Process */
#define UNUSED 0
#define READY 1
#define BLOCKED 2
#define RUNNING 3
#define QUIT 4
#define JOINBLOCKED 5

#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
#define AMOUNTPRIORITIES 7

