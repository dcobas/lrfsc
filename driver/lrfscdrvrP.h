/* ======================================================================= */
/* Include file to provide the PRIVATE LRFSC device driver tables.         */
/* These tables are used internally by the driver for managing the client, */
/* model, module, and driver contexts.                                     */
/* ======================================================================= */

/* Dont define the module more than once */

#ifndef LRFSCDRVR_P
#define LRFSCDRVR_P

#include <lrfscdrvr.h>

#define LrfscDrvrSTART_OF_RAM	0x80000
#define LrfscDrvrTOTAL_RAM_SIZE	0x80000
#define LrfscDrvrRAM_IQ_ENTRIES	(LrfscDrvrTOTAL_RAM_SIZE/sizeof(LrfscDrvrIQPair))
#define LrfscDrvrTICK_TO_PAIR	2
#define LrfscDrvrSKP_FACTOR     (LrfscDrvrTICK_TO_PAIR * LrfscDrvrBUF_IQ_ENTRIES)

/* ===================================================== */
/* Ram Select enumeration                                */
/* ===================================================== */

typedef enum {
   LrfscDrvrRamSETPOINTS,   /* SetPoints   */
   LrfscDrvrRamFEEDFORWARD, /* FeedForward */
   LrfscDrvrRamDIAG1,	
   LrfscDrvrRamDIAG2,
   LrfscDrvrRamDIAG3,
   LrfscDrvrRamDIAG4
 } LrfscDrvrRamSelection;

#define LrfscDrvrRamSELECTS 6

/* ===================================================== */
/* Diagnostic signal selection                           */
/* ===================================================== */

typedef unsigned short LrfscDrvrDiagRegisters[LrfscDrvrDiagSIGNAL_CHOICES];

/* ===================================================== */
/* Coefficients                                          */
/* ===================================================== */

typedef LrfscDrvrMatrixCoefficients LrfscDrvrMatrixCoefficientRegisters[LrfscDrvrMatrixMATRICES];

/* ===================================================== */
/* PIC registers                                         */
/* They must be posative sign never set                  */
/* ===================================================== */

typedef struct {
   signed short KP;    /* Proportional gain of PI controller*/
   signed short KI;    /* Integral gain of PI controller    */
 } LrfscDrvrPicRegisters;

/* ===================================================== */
/* Diagnostic and Config Ram Layout                                 */
/* ===================================================== */

typedef LrfscDrvrIQPair LrfcsDrvrRamIqArray[LrfscDrvrRAM_IQ_ENTRIES];

typedef LrfscDrvrIQPair LrfcsDrvrConfigArray[LrfscDrvrCONFIG_POINTS];

/* ===================================================== */
/* Module address has base address in it                 */
/* ===================================================== */

typedef struct {
   unsigned short *VMEAddress;     /* Base address for A24,D16    */
   unsigned short *RamAddress;     /* Address for the Ram 1       */
   unsigned short InterruptVector; /* Interrupt vector number     */
   unsigned short InterruptLevel;  /* Interrupt level (2 usually) */
 } LrfscDrvrModuleAddress;

/* ================================================================== */
/* The "info" table passed to the install procedure at startup time.  */
/* Containes the VME address of each modLrfscDrvrMatrixule.           */
/* Comes back from the installer                                      */
/* ================================================================== */

typedef struct {
   unsigned long          Modules;
   LrfscDrvrModuleAddress Addresses[LrfscDrvrMODULE_CONTEXTS];
 } LrfscDrvrInfoTable;

/* ===================================================== */
/* Structures needed to handle interrupt connections     */
/* ===================================================== */

/* Up to 32 incomming events per client are queued */

#define LrfscDrvrCLIENT_QUEUE_SIZE 32

typedef struct {
   unsigned long       QOff;
   unsigned long       Size;
   unsigned long       Head;
   unsigned long       Tail;
   unsigned long       Missed;
   LrfscDrvrConnection Queue[LrfscDrvrCLIENT_QUEUE_SIZE];
 } LrfscDrvrClientQueue;
 
/* ====================================================================== */
/* A client context contains all particular data concerning a particular  */
/* client. The default values are set up by the OPEN routine, which are   */
/* later modifyable via IOCTL calls.                                      */
/* ====================================================================== */

typedef struct {
   unsigned long          InUse;
   unsigned long          Pid;
   unsigned long          ClientIndex;
   unsigned long          ModuleIndex;
   unsigned long          Timeout;
   int                    Timer;
   int                    Semaphore;
   LrfscDrvrClientQueue   Queue;
   unsigned long          Debug;
   LrfscDrvrPulse         Pulse;
 } LrfscDrvrClientContext;

/* ===================================================== */
/* Module context.                                       */
/* ===================================================== */

typedef struct {
   unsigned long                ModuleIndex;
   unsigned long                SkipCount;
   unsigned long                SkipStart;
   unsigned long                AqnDone;
   LrfscDrvrPulse               Pulse;
   LrfscDrvrInterrupt           Clients[LrfscDrvrCLIENT_CONTEXTS];
   LrfscDrvrState               State;
   LrfscDrvrModuleAddress       Address;
   LrfscDrvrControl             Control;
   LrfscDrvrRamSelection        RamSelect;
   LrfscDrvrResCtrl             ResCtrl;
   LrfscDrvrAnalogSwitch        SwitchCtrl;
   LrfscDrvrSoftSwitch          SoftSwitch;
   LrfscDrvrPicRegisters        Pic;
   unsigned long                DiagTime;
   unsigned long                RfOnMaxLen;
   LrfscDrvrConfigArray         Configs[LrfscDrvrCONFIGS][LrfscDrvrCYCLES];
   unsigned long                ValidConfigs[LrfscDrvrCONFIGS][LrfscDrvrCYCLES];
   LrfscDrvrMatrixCoefficients  Coefficients[LrfscDrvrMatrixMATRICES];
   LrfscDrvrDiagChoices         SignalChoices;
} LrfscDrvrModuleContext;

/* ===================================================== */
/* Driver working area                                   */
/* ===================================================== */

typedef struct {
   unsigned long          Modules;
   LrfscDrvrModuleContext ModuleContexts[LrfscDrvrMODULE_CONTEXTS];
   LrfscDrvrClientContext ClientContexts[LrfscDrvrCLIENT_CONTEXTS];
 } LrfscDrvrWorkingArea;

/* ===================================================== */
/* Declare the LRFSC memory map using the above structure */
/* ===================================================== */

typedef struct {

   unsigned short	IrqSource;	/* RO X LrfscDrvrInterrupt (Clear on Read)	*/
   unsigned short	State;		/* RW X LrfscDrvrState						*/
   unsigned short	Control;	/* RW P LrfscDrvrControl					*/
   unsigned short	Vector;		/* RW Interrupt vector						*/
   unsigned short	RamSelect;	/* RW P LrfscDrvrRamSelection 				*/

   LrfscDrvrDiagRegisters SignalChoices;	/* RW X LrfscDrvrDiagSignalChoice	*/

   LrfscDrvrResCtrl	ResCtrl;	/* RW+RO X Resonance cavity control */
   
   unsigned short	SwitchCtrl;	/* RW X LrfscDrvrAnalogSwitch	*/
   unsigned short	SoftSwitch;	/* RW X LrfscDrvrSoftSwitch		*/

   LrfscDrvrMatrixCoefficientRegisters	Matrix;		/* RW P Ordered by LrfscDrvrMatrix */
   LrfscDrvrDiagSnapShot                SnapShot;       /* RW P */
   LrfscDrvrPicRegisters                Pic;            /* RW P Must be posative */

   unsigned short       RfOffTime;      /* RW Acquired number of 40MHz ticks during RFON pulse  */

   unsigned short       PulseNumber;    /* RO Pulse number in the cycle                         */
   unsigned short       NextCycle;      /* RW Next 0..31 cycle. Active on next start cycle      */
   unsigned short       PresCycle;      /* RO Present cycle executing                           */
   unsigned short       VhdlVerH;       /* RO High word of VHDL version 32 Bit UTC time         */
   unsigned short       VhdlVerL;       /* RO Low  word of VHDL version 32 Bit UTC time         */
   unsigned short       Status;         /* RO LrfscDrvrStatus Module status                     */
   unsigned short       RfOnMaxLen;     /* RW Maximum RF pulse length in ticks                  */
 } LrfscDrvrMemoryMap;

#endif
