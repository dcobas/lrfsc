/* ------------------------------------- */
/* Lrfsc Device driver                   */
/* Julian Lewis AB/CO/HT 28/Apr/2008     */


/* Dont define the module more than once */

#ifndef LRFSCDRVR
#define LRFSCDRVR

/* ------------------------------------- */
/* Constants                             */
/* ------------------------------------- */

#define LrfscDrvrMODULE_CONTEXTS 2   /* 2 modules max. to be installed  */
#define LrfscDrvrCLIENT_CONTEXTS 16  /* Max number of open file handlers */
#define LrfscDrvrDEFAULT_TIMEOUT 1000

/* ------------------------------------ */
/* Interrupt sources                    */

typedef enum {
   LrfscDrvrInterruptPULSE          = 0x1,  /* Falling edge of RF on pulse          */
   LrfscDrvrInterruptSTART_CYCLE    = 0x2   /* Rising edge of start cycle pulse     */
 } LrfscDrvrInterrupt;

#define LrfscDrvrINTERUPT_MASK 0x3
#define LrfscDrvrINTERRUPTS 2
 
typedef enum {
    LrfscDrvrPulseALL,
    LrfscDrvrPulse1,
    LrfscDrvrPulse2,
    LrfscDrvrPulse3,
    LrfscDrvrPulse4,
    LrfscDrvrPulse5
} LrfscDrvrPulse;

#define LrfscDrvrPULSES 5

typedef struct {
   unsigned long        Module;    /* The module number 1..n     */
   LrfscDrvrInterrupt	Interrupt; /* The intterrupt source */
   LrfscDrvrPulse       Pulse;     /* Pulse 0=>All */
 } LrfscDrvrConnection;
 
typedef struct {
   unsigned long        Pid;       /* Pid of client whos connections you want */
   unsigned long        Size;      /* Number of connection entries */
   LrfscDrvrConnection	Connections[LrfscDrvrMODULE_CONTEXTS];	/* The connections it has */
 } LrfscDrvrClientConnections;
 
typedef struct {
   unsigned long Size;
   unsigned long Pid[LrfscDrvrCLIENT_CONTEXTS];
 } LrfscDrvrClientList;
 
 /* ------------------------------------ */
 /* Control commands                     */

typedef enum {
   LrfscDrvrControlNONE        = 0x0,
   LrfscDrvrControlDO_AQN      = 0x1,  /* Request a synchronous acquisition */
   LrfscDrvrControlCONT_WAVE   = 0x2,  /* Set mode to continuous wave (CW) */
   LrfscDrvrControlWINDUP_OFF  = 0x4,  /* Turn anti-windup off in the PIC */
   LrfscDrvrControlSATCTRL_OFF = 0x8   /* Turn stauration control of in the PIC */
} LrfscDrvrControl;

#define LrfscDrvrCONTROLS 5
  
/* ----------------------------------------------------- */
/* Driver and VHDL Firmware Version date UTC times       */
 
typedef struct {
   unsigned long Firmware;
   unsigned long Driver;
 } LrfscDrvrVersion;
 
/* ----------------------------------------------------- */
/* Module status (Cleared on read)                       */
 
typedef enum {
   LrfscDrvrStatusOVER_REF         = 0x01, /* Over range in reflected signal ADC */
   LrfscDrvrStatusOVER_FWD         = 0x02, /* Over range in forward signal ADC */
   LrfscDrvrStatusOVER_CAV         = 0x04, /* Over range in cavity signal ADC */
   LrfscDrvrStatusRF_TOO_LONG      = 0x08, /* RF pulse is too long (>RfOnMaxLen) */
   LrfscDrvrStatusNO_FAST_PROTECT  = 0x10, /* When set its OK else protection is on */
   LrfscDrvrStatusMISSING_TICK     = 0x20  /* Missing IQ clock tick */
 } LrfscDrvrStatus;
 
#define LrfscDrvrSTATAE 6
 
/* ----------------------------------------------------- */
/* 16-Bit short Raw IO                                   */

typedef struct {
   unsigned short Size;      	/* Number long to read/write */
   unsigned short Offset;    	/* Offset address space      */
   unsigned short *UserArray;	/* Callers data area for  IO */
   unsigned short RamFlag;		/* Set when accessing RAM */
 } LrfscDrvrRawIoBlock;

/* ------------------------------------ */
/* State of the module / driver         */
/*    LrfscDrvrSET_STATE,               */
/*    LrfscDrvrGET_STATE,               */

typedef enum {
   LrfscDrvrStateCONFIG,
   LrfscDrvrStatePROD_LOCAL,
   LrfscDrvrStatePROD_REMOTE
 } LrfscDrvrState;

#define LrfscDrvrSTATES 3

/* ------------------------------------ */
/* Diagnostic choices                   */
/*    LrfscDrvrSET_DIAG_CHOICE          */
/*    LrfscDrvrGET_DIAG_CHOICE          */

typedef enum {
   LrfscDrvrDiagREFLECTED,  /* Reflected */
   LrfscDrvrDiagFORWARD,    /* Forward   */
   LrfscDrvrDiagCAVITY,     /* Cavity    */
   LrfscDrvrDiagCAVEERROR,  /* CavityErr */
   LrfscDrvrDiagOUTPUT      /* Output    */
 } LrfscDrvrDiagSignalChoice;

#define LrfscDrvrDiagSIGNALS 5
#define LrfscDrvrDiagSIGNAL_CHOICES 4

typedef LrfscDrvrDiagSignalChoice LrfscDrvrDiagChoices[LrfscDrvrDiagSIGNAL_CHOICES];
 
 /* ------------------------------------ */
 /* An IQ buffer                         */

typedef struct {
    signed short I;
    signed short Q;
 } LrfscDrvrIQPair; /* Two 14 Bit signed values is one IQ pair */

#define LrfscDrvrBUF_IQ_ENTRIES 0x200
typedef LrfscDrvrIQPair LrfscDrvrBufIqArray[LrfscDrvrBUF_IQ_ENTRIES]; /* For acquisitions */

/* ------------------------------------ */
/* Analogue Switch Control bits         */
/*    LrfscDrvrSET_ANALOGUE_SWITCH      */
/*    LrfscDrvrGET_ANALOGUE_SWITCH      */

typedef enum {
   LrfscDrvrAnalogSwitchREF_TEST = 0x1,
   LrfscDrvrAnalogSwitchFWD_TEST = 0x2,
   LrfscDrvrAnalogSwitchCAV_TEST = 0x4,
   LrfscDrvrAnalogSwitchOUT_TEST = 0x8
 } LrfscDrvrAnalogSwitch;

#define LrfscDrvrAnalogSWITCHES 4

/* ------------------------------------ */
/* Software Switch Control bits         */
/* Soft Switch in FPGA                  */
/*    LrfscDrvrSET_SOFT_SWITCH          */
/*    LrfscDrvrGET_SOFT_SWITCH          */

typedef enum {
   LrfscDrvrSotfSwitchMAIN_CLOSED = 0x0,
   LrfscDrvrSoftSwitchMAIN_OPEN   = 0x1
 } LrfscDrvrSoftSwitch;

#define LrfscDrvrSoftSWITCHES 2

/* ------------------------------------ */
/* For each matric there are 4 coefs    */
/*    LrfscDrvrSET_COEFFICIENTS         */
/*    LrfscDrvrGET_COEFFICIENTS         */

typedef enum {
   LrfscDrvrMatrixREF, /* Reflected */
   LrfscDrvrMatrixFWD, /* Feed forward */
   LrfscDrvrMatrixCAV  /* Cavity */
 } LrfscDrvrMatrix;

#define LrfscDrvrMatrixMATRICES 3

typedef struct {
   signed short MatrixA;
   signed short MatrixB;
   signed short MatrixC;
   signed short MatrixD;
 } LrfscDrvrMatrixCoefficients;

typedef struct {
   LrfscDrvrMatrix             Matrix;
   LrfscDrvrMatrixCoefficients Coeficients;
 } LrfscDrvrMatrixCoefficientsBuf;

/* ------------------------------------ */
/* Snap Shots                           */
/*    LrfscDrvrGET_SNAP_SHOTS           */
/* The snap shot time is set by ...     */
/*    LrfscDrvrSET_SNAP_SHOT_TIME       */

typedef struct {
   unsigned short  DiagTime;    /* Time after RFON for snapshot 40MHz ticks */
   LrfscDrvrIQPair RefDiag;     /* Reflected */
   LrfscDrvrIQPair FwdDiag;     /* Forware */
   LrfscDrvrIQPair CavDiag;     /* Cavity */
   LrfscDrvrIQPair ErrDiag;     /* Error */
   LrfscDrvrIQPair OutDiag;     /* Output */
   LrfscDrvrIQPair POutDiag;    /* PI controler gain */
   LrfscDrvrIQPair IOutDiag;    /* PI integrator */
 } LrfscDrvrDiagSnapShot;

/* ------------------------------------ */
/* Resonance control                    */
/*    LrfscDrvrSET_RES_CTRL             */
/*    LrfscDrvrGET_RES_CTRL             */

typedef struct {
   unsigned short  Time;    /* RW Control time */
   unsigned short  Value;   /* RW Control value */
   LrfscDrvrIQPair Fwd;     /* RO X Forward */
   LrfscDrvrIQPair Cav;     /* RO X Cavity */
 } LrfscDrvrResCtrl;

/* ------------------------------------ */
/* Set the PIC constants                */
/*    LrfscDrvrSET_PIC                  */

typedef struct {
   signed short KP;    /* Proportional gain of PI controller */
   signed short KI;    /* Integral gain of PI controller     */
 } LrfscDrvrPicSetBuf;

/* ------------------------------------ */
/* Cycle configurations                 */
/*    LrfscDrvrSET_CYCLE_CONFIG         */
/*    LrfscDrvrGET_CYCLE_CONFIG         */

#define LrfscDrvrCYCLES 32
#define LrfscDrvrCONFIGS 2
#define LrfscDrvrCONFIG_POINTS 4096

typedef enum {
   LrfscDrvrConfigSETPOINTS,   /* SetPoints   */
   LrfscDrvrConfigFEEDFORWARD  /* FeedForward */
 } LrfscDrvrConfig;

typedef LrfscDrvrIQPair LrfscDrvrConfigArray[LrfscDrvrCONFIG_POINTS];

typedef struct {
   LrfscDrvrConfig      Which;   /* Which configuration ram */
   unsigned long        Points;  /* Number of points in buffer */
   unsigned long        Cycle;   /* Cycle number 0..31 */
   LrfscDrvrConfigArray Array;   /* Configuration array */
 } LrfscDrvrConfigBuf;

/* ------------------------------------ */
/* Diagnostics                          */
/*    LrfscDrvrSET_PULSE                */
/*    LrfscDrvrGET_DIAGNOSTICS          */

typedef struct {
   unsigned long                Size;   /* Set your buffer size in IQ Pairs */
   LrfscDrvrPulse               Pulse;  /* Set the pulse */
   LrfscDrvrDiagSignalChoice	Choice;	/* Set your choice of signal */
   
   unsigned long                SkipStart;  /* Returns Start of acquisition */
   unsigned long                SkipCount;  /* Returns Distance between samples */
   unsigned long                Valid;      /* Returns True id diagnostic data is valid */
   LrfscDrvrBufIqArray          Array;      /* Returns Array of acquired pulses */
 } LrfscDrvrDiagBuf;

/************************/
/* IO Control functions */
/************************/

typedef enum {
   
   LrfscDrvrSET_MODULE,             /* ulong Set module for read/write 1..2 */
   LrfscDrvrGET_MODULE,             /* ulong Get module for read/write 1..2 */
   LrfscDrvrGET_MODULE_COUNT,       /* ulong Get number of installed modules */
   LrfscDrvrGET_MODULE_ADDRESS,     /* LrfscDrvrModuleAddress Read */
   LrfscDrvrRESET,                  /* NULL Reset the module and driver */
   LrfscDrvrGET_STATUS,             /* LrfscDrvrStatus Get status */

   LrfscDrvrSET_SW_DEBUG,           /* ulong Set software debug level */
   LrfscDrvrGET_SW_DEBUG,           /* ulong Get software debug level */

   LrfscDrvrRAW_READ,               /* LrfscDrvrRawIoBlock Read  */
   LrfscDrvrRAW_WRITE,              /* LrfscDrvrRawIoBlock Write */

   LrfscDrvrSET_TIMEOUT,            /* ulong Set read timeout */
   LrfscDrvrGET_TIMEOUT,            /* ulong Get read timeout */

   LrfscDrvrCONNECT,                /* LrfscDrvrConnection Connect to interrupt */
   LrfscDrvrDISCONNECT,             /* LrfscDrvrConnection DisConnect from interrupt */

   LrfscDrvrSET_QUEUE_FLAG,         /* ulong Set queue off 1 or on 0 */
   LrfscDrvrGET_QUEUE_FLAG,
   LrfscDrvrGET_QUEUE_SIZE,
   LrfscDrvrGET_QUEUE_OVERFLOW,

   LrfscDrvrGET_CLIENT_LIST,        /* LrfscDrvrClientList Get all clients */
   LrfscDrvrGET_CLIENT_CONNECTIONS, /* LrfscDrvrClientConnections Get clients connections */

   LrfscDrvrSET_STATE,              /* LrfscDrvrState Get Module state */
   LrfscDrvrGET_STATE,

   LrfscDrvrSET_DIAG_CHOICE,        /* LrfscDrvrDiagChoices Configure diagnostics channels */
   LrfscDrvrGET_DIAG_CHOICE,

   LrfscDrvrSET_RES_CTRL,           /* LrfscDrvrResCtrl Set Resonance control value */
   LrfscDrvrGET_RES_CTRL,

   LrfscDrvrSET_ANALOGUE_SWITCH,    /* LrfscDrvrAnalogSwitch */
   LrfscDrvrGET_ANALOGUE_SWITCH,

   LrfscDrvrSET_SOFT_SWITCH,        /* LrfscDrvrSoftSwitch */
   LrfscDrvrGET_SOFT_SWITCH,

   LrfscDrvrSET_COEFFICIENTS,       /* LrfscDrvrMatrixCoefficientsBuf */
   LrfscDrvrGET_COEFFICIENTS,

   LrfscDrvrSET_SNAP_SHOT_TIME,     /* ulong Snapshot time */
   LrfscDrvrGET_SNAP_SHOTS,         /* LrfscDrvrDiagSnapShotBuf */

   LrfscDrvrSET_PIC,                /* LrfscDrvrPicSetBuf PI Controller */
   LrfscDrvrGET_PIC,

   LrfscDrvrGET_PULSE_LENGTH,       /* ulong RF Pulse length */
   LrfscDrvrSET_MAX_PULSE_LENGTH,   /* ulong maximum RF pulse length */
   LrfscDrvrGET_MAX_PULSE_LENGTH,   /* ulong maximum RF pulse length */

   LrfscDrvrSET_NEXT_CYCLE,         /* ulong Next cycle 1..32 */
   LrfscDrvrGET_PRES_CYCLE,

   LrfscDrvrSET_CYCLE_CONFIG,       /* LrfscDrvrConfigBuf Set a cycle configuration */
   LrfscDrvrGET_CYCLE_CONFIG,       /* LrfscDrvrConfigBuf Get a cycle configuration */

   LrfscDrvrSET_SKIP_COUNT,         /* ulong Set diagnostic skip count */
   LrfscDrvrSET_SKIP_START,         /* ulong Set diagnostic skip count */
   LrfscDrvrSET_PULSE,              /* LrfscDrvrPulse Set acquisition pulse */
   LrfscDrvrGET_DIAGNOSTICS,        /* LrfscDrvrDiagBuf Get a diagnostic ram */

   LrfscDrvrGET_VERSION             /* LrfscDrvrVersion Get UTC version dates */

 } LrfscDrvrControlFunction;

#endif
