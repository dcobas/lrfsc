/***************************************************************************/
/* Include file to provide the PRIVATE LLCC device driver tables.          */
/* These tables are used internally by the driver for managing the client, */
/* model, module, and driver contexts.                                     */
/* Jan 2003                                                                */
/* Ioan Kozsar                                                             */
/***************************************************************************/


/* Dont define the module more than once */

#ifndef LLCCDRVR_P
#define LLCCDRVR_P

#include <llccdrvr.h>

typedef unsigned short word;     /* Llcc module addressing 16 bit access */

/* Interrupt sources */

#define LlccDrvrINTERRUPT_SOURCES 1        /* Number of sources */
#define LlccDrvrCLIENT_QUEUE_SIZE 32

/* ------------------------------------ */
/* Switch Control bits */

#define LlccDrvrOUT_SWITCH      0X0008 /* OUTPUT     bit */
#define LlccDrvrREF_SWITCH      0X0001 /* REFLECTED  bit */
#define LlccDrvrFWD_SWITCH      0x0002 /* FORWARD    bit */
#define LlccDrvrCAV_SWITCH      0x0004 /* CAVITY     bit */

/* ------------------------------------ */
/* Software Switch Control bits */

#define LlccDrvrSOFT_MAINLOOP_SWITCH      0X0001 /* Main Loop		bit */


/* ------------------------------------ */
/* Ram Select bits                      */

#define LlccDrvrSETPOINTS      0x00 /* SETPOINTS	*/
#define LlccDrvrFEEDFORVARD    0x01 /* FEEDFORWARD	*/
#define LlccDrvrDIAG1          0x02 /* REFLECTED	*/
#define LlccDrvrDIAG2          0x03 /* FORWARD		*/
#define LlccDrvrDIAG3          0x04 /* CAVITY		*/
#define LlccDrvrDIAG4          0x05 /* OUTPUT		*/

/* ------------------------------------ */
/* Diag Select bits                      */

#define LlccDrvrREFLECTED             0x00 /* REFLECTED           */
#define LlccDrvrFORWARD               0x01 /* FORWARD             */
#define LlccDrvrCAVITY                0x02 /* CAVITY              */
#define LlccDrvrCAVEERROR             0x03 /* CAVEERROR           */
#define LlccDrvrOUTPUT                0x04 /* OUTPUT              */

/* ------------------------------------ */
/* Matrix offsets                       */

#define LlccDrvrREF_MATRIX             0  /*  */
#define LlccDrvrFWD_MATRIX             4  /*  */
#define LlccDrvrCAV_MATRIX        	   8  /*  */

/* ------------------------------------ */
/* Coefficient offsets                  */

#define LlccDrvrA_MATRIX              0  /*  */
#define LlccDrvrB_MATRIX              1  /*  */
#define LlccDrvrC_MATRIX              2  /*  */
#define LlccDrvrD_MATRIX              3  /*  */

/* ------------------------------------ */
/* Diagnostic I and Q offsets           */

#define LlccDrvrREF_I               0 /*  */
#define LlccDrvrREF_Q               1 /*  */
#define LlccDrvrFWD_I               2 /*  */
#define LlccDrvrFWD_Q               3 /*  */
#define LlccDrvrCAV_I               4 /*  */
#define LlccDrvrCAV_Q               5 /*  */
#define LlccDrvrERR_I               6 /*  */
#define LlccDrvrERR_Q               7 /*  */
#define LlccDrvrLRN_I               8 /*  */
#define LlccDrvrLRN_Q               9 /*  */
#define LlccDrvrOUT_I              10 /*  */
#define LlccDrvrOUT_Q              11 /*  */

/* ------------------------------------ */
/* PI controller offsets                  */

#define LlccDrvrKP             		0  /*  */
#define LlccDrvrKI          	    1  /*  */
#define LlccDrvrIP_OUT              2  /*  */
#define LlccDrvrQP_OUT              3  /*  */
#define LlccDrvrII_OUT              4  /*  */
#define LlccDrvrQI_OUT              5  /*  */


/* ============================================= */
/* Declare the structures in the LLCC memory map */
/* ============================================= */

/* ----------------------------- */
/* Interrupt registers (IRQ)     */

typedef struct {
   unsigned short Status;   /* Interrupt Enable Status and Control */
   unsigned short Control;  /* Misc control bits */
   unsigned short Vec;      /* Interrupt vector                    */
 } LlccDrvrIrq;


/*************************************************************************/
/* Module address has base address in it                                 */
/*************************************************************************/

typedef struct {
   int  *VMEAddress;         /* Base address for A24,D16    */
   int  *RamAddress;		/* Address for the Ram 1	   */
   unsigned short   InterruptVector;    /* Interrupt vector number     */
   unsigned short   InterruptLevel;     /* Interrupt level (2 usually) */
 } LlccDrvrModuleAddress;


/**************************************************************************/
/* The "info" table passed to the install procedure at startup time.      */
/* Containes the VME address of each module.                              */
/* Comes back from the installer                                          */
/**************************************************************************/

typedef struct {
	unsigned long 		Modules;
	LlccDrvrModuleAddress	Addresses[LlccDrvrMODULE_CONTEXTS];
 } LlccDrvrInfoTable;


/* ************************************************* */
/* Structures needed to handle interrupt connections */

/* ----------------------------------------------- */
/* Up to 32 incomming events per client are queued */

#define LlccDrvrCLIENT_QUEUE_SIZE 32

typedef struct {
   unsigned long   QOff;
   unsigned long   Size;
   unsigned long   Head;
   unsigned long   Tail;
   unsigned long   Missed;
   LlccDrvrReadBuf Queue[LlccDrvrCLIENT_QUEUE_SIZE];
 } LlccDrvrClientQueue;
 
 
/**************************************************************************/
/* A client context contains all particular data concerning a particular  */
/* client. The default values are set up by the OPEN routine, which are   */
/* later modifyable via IOCTL calls.                                      */
/**************************************************************************/
/* ****************** */
/* The client context */

typedef struct {
   unsigned long          InUse;
   unsigned long          Pid;
   unsigned long          ClientIndex;
   unsigned long          ModuleIndex;
   /* unsigned long	  Timeout; */
	    /* int           Timer; */
	    int           Semaphore;
   LlccDrvrClientQueue    Queue;
   unsigned long          DebugOn;
 } LlccDrvrClientContext;


/**************************************************************************/
/* Module context.                                                        */
/**************************************************************************/

typedef struct {
   unsigned long	  ModuleIndex;
   LlccDrvrModuleAddress  Address;
   LlccDrvrClientContext *Clients  [LlccDrvrINTERRUPT_SOURCES][LlccDrvrCLIENT_CONTEXTS];
   unsigned long NbInterrupts;
   unsigned long ToggleRequest;
} LlccDrvrModuleContext;


/**************************************************************************/
/* Driver working area                                                    */
/**************************************************************************/

typedef struct {
	unsigned long		Modules;
	LlccDrvrModuleContext	ModuleContexts[LlccDrvrMODULE_CONTEXTS];
	LlccDrvrClientContext	ClientContexts[LlccDrvrCLIENT_CONTEXTS];
 } LlccDrvrWorkingArea;


/* ===================================================== */
/* Declare the LLCC memory map using the above structure */
/* ===================================================== */

typedef struct {
   /* unsigned short ram[262144]; */	/* memory chip space							*/
   LlccDrvrIrq    Irq;			/* Interrupt control									*/
   unsigned short RamSelect;	/* Selects a RAM chip to read-write						*/
   unsigned short Diag1Select;	/* Choice of signal to send to diagnostics channel 1	*/
   unsigned short Diag2Select;	/* Choice of signal to send to diagnostics channel 2	*/
   unsigned short Diag3Select;	/* Choice of signal to send to diagnostics channel 3	*/
   unsigned short Diag4Select;	/* Choice of signal to send to diagnostics channel 4	*/
   unsigned short ResCtrl;		/* Resonance control value produced by the card			*/
   unsigned short SwitchCtrl;	/* Control of the four on-board analog switches			*/
   unsigned short SoftSwitch;	/* Control of the four program loop switches			*/
   signed   short RefMatrixA;	/* A coefficient for the Reflected matrix				*/
   signed   short RefMatrixB;	/* B coefficient for the Reflected matrix				*/
   signed   short RefMatrixC;	/* C coefficient for the Reflected matrix				*/
   signed   short RefMatrixD;	/* D coefficient for the Reflected matrix				*/
   signed   short FwdMatrixA;	/* A coefficient for the Reflected matrix				*/
   signed   short FwdMatrixB;	/* B coefficient for the Reflected matrix				*/
   signed   short FwdMatrixC;	/* C coefficient for the Reflected matrix				*/
   signed   short FwdMatrixD;	/* D coefficient for the Reflected matrix				*/
   signed   short CavMatrixA;	/* A coefficient for the Reflected matrix				*/
   signed   short CavMatrixB;	/* B coefficient for the Reflected matrix				*/
   signed   short CavMatrixC;	/* C coefficient for the Reflected matrix				*/
   signed   short CavMatrixD;	/* D coefficient for the Reflected matrix				*/
   unsigned short DiagTime;		/* Time after RF ON for snapshot in 40MHz ticks			*/
   unsigned short RefDiagI;		/* Snapshot diagnostic value I for reflected signal		*/
   unsigned short RefDiagQ;		/* Snapshot diagnostic value Q for reflected signal		*/
   unsigned short FwdDiagI;		/* Snapshot diagnostic value I for forwarded signal		*/
   unsigned short FwdDiagQ;		/* Snapshot diagnostic value Q for forwarded signal		*/
   unsigned short CavDiagI;		/* Snapshot diagnostic value I for cavity signal		*/
   unsigned short CavDiagQ;		/* Snapshot diagnostic value Q for cavity signal		*/
   unsigned short ErrDiagI;		/* Snapshot diagnostic value I for error signal			*/
   unsigned short ErrDiagQ;		/* Snapshot diagnostic value Q for error signal			*/
   unsigned short LrnDiagI;		/* Snapshot diagnostic value I for learning signal		*/
   unsigned short LrnDiagQ;		/* Snapshot diagnostic value Q for learning signal		*/
   unsigned short OutDiagI;		/* Snapshot diagnostic value I for output signal		*/
   unsigned short OutDiagQ;		/* Snapshot diagnostic value Q for output signal		*/
   signed short KP;			/* Proportional gain of PI controller					*/
   signed short KI;			/* Integral gain of PI controller						*/
   signed short IPOut;		/* Output of proportional gain for I input in PI		*/
   signed short QPOut;		/* Output of proportional gain for Q input in PI		*/
   signed short IIOut;		/* Output of integrator for I input in PI				*/
   signed short QIOut;		/* Output of integrator for Q input in PI				*/
   unsigned short RfOffTime;	/* Acquired number of 40MHz ticks during RFON pulse		*/
//   unsigned short KlysGainSP;	/* Gain set point for the klystron loop					*/
 } LlccDrvrMemoryMap;

#endif
