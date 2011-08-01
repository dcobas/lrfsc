/***************************************************************************/
/* Llcc Device driver                                                      */
/* Feb 2003                                                                */
/* Ioan Kozsar                                                             */
/***************************************************************************/

/* Dont define the module more than once                                   */

#ifndef LLCCDRVR
#define LLCCDRVR


/***************************************************************************/
/* Constants                                                               */
/***************************************************************************/

#define LlccDrvrMODULE_CONTEXTS 16      /* 16 modules max. to be installed  */
#define LlccDrvrCLIENT_CONTEXTS 16      /* Max number of open file handlers */
#define LlccDrvrMODULE_CHANNELS 8       /* Number of chanels on the card    */

typedef struct {
   unsigned long Module;   /* The module number 1..n     */
 } LlccDrvrConnection;

typedef struct {
   LlccDrvrConnection Connection;
 } LlccDrvrReadBuf;

/* ***************************************************** */
/* Raw IO                                                */

typedef struct {
   unsigned long Size;      /* Number long to read/write */
   unsigned long Offset;    /* Long offset address space */
   unsigned long *UserArray;/* Callers data area for  IO */
 } LlccDrvrRawIoBlock;


/************************/
/* IO Control functions */
/************************/

typedef enum {
		LlccDrvrSET_DEBUG,				/* Switch debug trace mode ON/OFF	 */
		LlccDrvrSET_MODULE,				/* Set module for read/write		 */
		LlccDrvrGET_MODULE,				/* Get module for read/write		 */
		LlccDrvrCONNECT,				/* Connect to interrupt */
		LlccDrvrDISCONNECT,				/* Connect to interrupt */
		LlccDrvrGET_IRQ_VECTOR,			/* Get vector of module			 */
		LlccDrvrSET_IRQ_ENABLE,			/* Set interrupt enable			 */
		LlccDrvrGET_IRQ_ENABLE,			/* Get interrupt status			 */
		LlccDrvrRESET_ALL,				/* Reset the module      		 */
		LlccDrvrREQUEST,				/* Request a synchronized IO operation */
		LlccDrvrSATURATION,				/* Saturation control */
		LlccDrvrINTERRUPTS,				/* Gets the number of interrupts */
		LlccDrvrSET_OUT_SWITCH,			/* Analog Switches - test pos */
		LlccDrvrGET_OUT_SWITCH,
		LlccDrvrSET_REF_SWITCH,
		LlccDrvrGET_REF_SWITCH,
		LlccDrvrSET_FWD_SWITCH,
		LlccDrvrGET_FWD_SWITCH,
		LlccDrvrSET_CAV_SWITCH,
		LlccDrvrGET_CAV_SWITCH,
		LlccDrvrSET_MAINLOOP_SWITCH,	/* Software Switches */
		LlccDrvrGET_MAINLOOP_SWITCH,
		LlccDrvrSET_RAM_SELECT,			/* Ram Select */
		LlccDrvrGET_RAM_SELECT,
		LlccDrvrSET_DIAG1_SELECT,		/* Diagnostics channels */
		LlccDrvrGET_DIAG1_SELECT,
		LlccDrvrSET_DIAG2_SELECT,
		LlccDrvrGET_DIAG2_SELECT,
		LlccDrvrSET_DIAG3_SELECT,
		LlccDrvrGET_DIAG3_SELECT,
		LlccDrvrSET_DIAG4_SELECT,
		LlccDrvrGET_DIAG4_SELECT,
		LlccDrvrGET_RES_CONTROL,	/* Resonance Control Value */
		LlccDrvrSET_REF_MATRIX_A,	/* REF : A, B, C, D matrix */
		LlccDrvrGET_REF_MATRIX_A,
		LlccDrvrSET_REF_MATRIX_B,
		LlccDrvrGET_REF_MATRIX_B,
		LlccDrvrSET_REF_MATRIX_C,
		LlccDrvrGET_REF_MATRIX_C,
		LlccDrvrSET_REF_MATRIX_D,
		LlccDrvrGET_REF_MATRIX_D,
		LlccDrvrSET_FWD_MATRIX_A,	/* FWD : A, B, C, D matrix */
		LlccDrvrGET_FWD_MATRIX_A,
		LlccDrvrSET_FWD_MATRIX_B,
		LlccDrvrGET_FWD_MATRIX_B,
		LlccDrvrSET_FWD_MATRIX_C,
		LlccDrvrGET_FWD_MATRIX_C,
		LlccDrvrSET_FWD_MATRIX_D,
		LlccDrvrGET_FWD_MATRIX_D,
		LlccDrvrSET_CAV_MATRIX_A,	/* CAV : A, B, C, D matrix */
		LlccDrvrGET_CAV_MATRIX_A,
		LlccDrvrSET_CAV_MATRIX_B,
		LlccDrvrGET_CAV_MATRIX_B,
		LlccDrvrSET_CAV_MATRIX_C,
		LlccDrvrGET_CAV_MATRIX_C,
		LlccDrvrSET_CAV_MATRIX_D,
		LlccDrvrGET_CAV_MATRIX_D,
		LlccDrvrSET_DIAGTIME,
		LlccDrvrGET_DIAGTIME,
		LlccDrvrGET_REF_DIAG_I,		/* Snapshots */
		LlccDrvrGET_REF_DIAG_Q,
		LlccDrvrGET_FWD_DIAG_I,
		LlccDrvrGET_FWD_DIAG_Q,
		LlccDrvrGET_CAV_DIAG_I,
		LlccDrvrGET_CAV_DIAG_Q,
		LlccDrvrGET_ERR_DIAG_I,
		LlccDrvrGET_ERR_DIAG_Q,
		LlccDrvrGET_LRN_DIAG_I,
		LlccDrvrGET_LRN_DIAG_Q,
		LlccDrvrGET_OUT_DIAG_I,
		LlccDrvrGET_OUT_DIAG_Q,
		LlccDrvrSET_KP,				/* Proportional gain of PI controller */
		LlccDrvrGET_KP,
		LlccDrvrSET_KI,				/* Integral gain of PI controller */
		LlccDrvrGET_KI,
		LlccDrvrGET_IP_OUT,			/* Output of the PI controller */
		LlccDrvrGET_QP_OUT,
		LlccDrvrGET_II_OUT,
		LlccDrvrGET_QI_OUT,
		LlccDrvrGET_RFOFFTIME,		/* RFOFFTIME */
//		LlccDrvrSET_KLYSGAINSP,		/* Klystron Gain Set Point */
//		LlccDrvrGET_KLYSGAINSP,
		LlccDrvrRAW_READ,		/* Read					 */
		LlccDrvrRAW_WRITE,		/* Write				 */
		LlccGET_DRIVER_VERSION,
		LlccDrvrLAST_IOCTL
   } LlccDrvrControlFunction;

#endif
