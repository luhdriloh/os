/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
  /* Head Pointer */
   procPtr         parentProcPtr;     
   procPtr         childProcPtr;
   procPtr         quitChildProcPtr;
   procPtr         zappedProcPtr;
   procPtr         zappersProcPtr;

   /* Next in List Pointer */
   procPtr         nextProcPtr;       /* for readylist */
   procPtr         nextSiblingPtr;
   procPtr         nextQuitSibling;
   procPtr         nextZapperSibling;

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
   int             zapped;
   int             total_time_used;
   int             time_slice_start;
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


/* Status of Process */
#define UNUSED 0
#define READY 1
#define RUNNING 2
#define QUIT 3
#define BLOCKED  4
#define JOINBLOCKED  5
#define ZAPBLOCKED  6

#define BlOCKMEBLOCKED 11

#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)
#define AMOUNTPRIORITIES 7

