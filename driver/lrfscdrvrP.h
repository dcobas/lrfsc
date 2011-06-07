/* ======================================================================= */
/* Include file to provide the PRIVATE LRFSC device driver tables.         */
/* These tables are used internally by the driver for managing the client, */
/* model, module, and driver contexts.                                     */
/* ======================================================================= */

/* Dont define the module more than once */

#ifndef LRFSCDRVR_P
#define LRFSCDRVR_P

#define LrfscDrvrSTART_OF_RAM	0x80000
#define LrfscDrvrTOTAL_RAM_SIZE	0x80000
#define LrfscDrvrRAM_IQ_ENTRIES	(LrfscDrvrTOTAL_RAM_SIZE/sizeof(LrfscDrvrIQPair))
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
/* Configuration ram layout                              */
/* ===================================================== */

typedef struct {
   short              High;
   short              Low;
 } LrfscDrvr2Shorts;

typedef union {
   int                Long;
   LrfscDrvr2Shorts   Shorts;
 } LrfscDrvrIncrement;

typedef struct __attribute__ ((__packed__)) {
   unsigned short     Next;     /* Next vector short address */
   unsigned short     Ticks;
   LrfscDrvrIncrement IncI;
   LrfscDrvrIncrement IncQ;
 } LrfscDrvrVector;

typedef LrfscDrvrVector LrfscDrvrVectorArray[LrfscDrvrBUF_IQ_ENTRIES];

/* ===================================================== */
/* Diagnostic Ram Layout                                 */
/* ===================================================== */

typedef LrfscDrvrIQPair LrfcsDrvrRamIqArray[LrfscDrvrRAM_IQ_ENTRIES];

/* ===================================================== */
/* Module context.                                       */
/* ===================================================== */

typedef struct {
	unsigned long                ModuleIndex;
	unsigned long                SkipCount;
	unsigned long                SkipStart;
	LrfscDrvrPulse               Pulse;
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
	LrfscDrvrVectorArray         Vectors[LrfscDrvrCONFIGS][LrfscDrvrCYCLES];
	unsigned long                ValidVecs[LrfscDrvrCONFIGS][LrfscDrvrCYCLES];
	LrfscDrvrMatrixCoefficients  Coefficients[LrfscDrvrMatrixMATRICES];
	LrfscDrvrDiagChoices         SignalChoices;
	LrfscDrvrBufIqArray          Diags[LrfscDrvrDiagSIGNALS][LrfscDrvrCYCLES];
	unsigned long                ValidDiags[LrfscDrvrDiagSIGNALS][LrfscDrvrCYCLES];

	void *map1;                     /* First mapped vme address */
	struct vme_berr_handler *ber1;	/* First bus error handler */
	void *map2;                     /* Second mapped vme address */
	struct vme_berr_handler *ber2;  /* Second bus error handler */
	wait_queue_head_t queue;	/* For interrupt waiting */
	int debug;                      /* Debug level */

	int timeout;                    /* Timeout value for wait queue */
	unsigned int icnt;              /* Interrupt counter */

	unsigned short isrc;            /* Last interrupt source mask */
	unsigned short cnum;            /* Last interrupt cycle number */
	unsigned short pnum;            /* Last interrupt pulse number */

} LrfscDrvrModuleContext;

/* ===================================================== */
/* Driver working area                                   */
/* ===================================================== */

typedef struct {
   unsigned long          Modules;
   LrfscDrvrModuleContext ModuleContexts[LrfscDrvrMODULE_CONTEXTS];
 } LrfscDrvrWorkingArea;

/* ===================================================== */
/* Declare the LRFSC memory map using the above structure */
/* ===================================================== */

typedef struct {

   unsigned short       IrqSource;      /* RO X LrfscDrvrInterrupt (Clear on Read) */
   unsigned short       State;          /* RW X LrfscDrvrState                     */
   unsigned short       Control;        /* RW P LrfscDrvrControl                   */
   unsigned short       Vector;         /* RW Interrupt vector                     */
   unsigned short       RamSelect;      /* RW P LrfscDrvrRamSelection              */

   LrfscDrvrDiagRegisters SignalChoices;        /* RW X LrfscDrvrDiagSignalChoice  */

   LrfscDrvrResCtrl     ResCtrl;        /* RW+RO X Resonance cavity control        */
   
   unsigned short       SwitchCtrl;     /* RW X LrfscDrvrAnalogSwitch              */
   unsigned short       SoftSwitch;     /* RW X LrfscDrvrSoftSwitch                */

   LrfscDrvrMatrixCoefficientRegisters  Matrix;         /* RW P Ordered by LrfscDrvrMatrix      */
   LrfscDrvrDiagSnapShot                SnapShot;       /* RW P                                 */
   LrfscDrvrPicRegisters                Pic;            /* RW P Must be posative                */

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
