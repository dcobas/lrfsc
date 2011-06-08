/* ============================================================================ */
/* Access library routines for the LRFSC                                        */
/* Julian Lewis Mon 23rd June 2008                                              */
/* ============================================================================ */

#include <lrfscdrvr.h>

#ifndef LIBLRFSC
#define LIBLRFSC

/* ============================================================================ */

typedef enum {
   LibLrfscErrorNONE,          /* No Error, all OK                              */
   LibLrfscErrorINIT,          /* Library has not been initialized correctly    */
   LibLrfscErrorRANGE,         /* One or more parameter(s) out of range         */
   LibLrfscErrorTIMEOUT,       /* A timeout has occured                         */
   LibLrfscErrorNULL,          /* NULL parameter pointer not allowed here       */
   LibLrfscErrorIO,            /* General driver IO error, see errno            */
   LibLrfscErrorMODULE,        /* No such Module installed                      */
   LibLrfscErrorINTERRUPT,     /* No such interrupt source                      */
   LibLrfscErrorPULSE,         /* RF pulse number out of range                  */
   LibLrfscErrorCONNECTIONS,   /* No connections to wait for                    */

   LibLrfscERRORS
 } LibLrfscError;

/* ============================================================================ */
/* Convert a given LibLrfscError (err) into a printable string.                 */
/* This routine returns a static pointer to an error string, hence be careful   */
/* multi-threaded code.                                                         */

char *LibLrfscErrorToString(LibLrfscError err);

/* ============================================================================ */
/* Initialize the library. A file descriptor is returned each time the routine  */
/* is called so that multi-threaded waits can be implemented correctly. Do not  */
/* call this routine in a loop it allocates a FD for each call.                 */

LibLrfscError LibLrfscInit(int *fd);

/* ============================================================================ */
/* Thread safe routine to connect to an interrupt                               */


LibLrfscError LibLrfscConnect(int fd,                       /* FD from init             */
			      LrfscDrvrConnection *conx,    /* Connection you want      */
			      int timeout,                  /* Time out in system clock */
			      int queue_flag);              /* When zero use a queue    */

/* ============================================================================ */
/* Wait for an interrupt. The return parameter can indicate a timeout           */

LibLrfscError LibLrfscWait(int fd,                          /* FD from init             */
			   LrfscDrvrConnection *conx);      /* Connection that occured  */

/* ============================================================================ */
/* Get Driver and VHDL version dates                                            */

LibLrfscError LibLrfscGetVersion(int module, LrfscDrvrVersion *vers);

/* ============================================================================ */
/* Read the Lrfsc module status                                                 */

LibLrfscError LibLrfscGetStatus(int module, LrfscDrvrStatus *status);

/* ============================================================================ */
/* Get and set the Lrfsc module state.                                          */

LibLrfscError LibLrfscGetState(int module, LrfscDrvrState *state);
LibLrfscError LibLrfscSetState(int module, LrfscDrvrState  state);

/* ============================================================================ */
/* Control Lrfsc switches                                                       */

LibLrfscError LibLrfscGetAnalogueSwitch(int module, LrfscDrvrAnalogSwitch *aswtch);
LibLrfscError LibLrfscSetAnalogueSwitch(int module, LrfscDrvrAnalogSwitch  aswtch);

LibLrfscError LibLrfscGetSoftSwitch(int module, LrfscDrvrSoftSwitch *swtch);
LibLrfscError LibLrfscSetSoftSwitch(int module, LrfscDrvrSoftSwitch  swtch);

/* ============================================================================ */
/* Get and set Lrfsc module matrix coefficients                                 */

LibLrfscError LibLrfscGetMatrixCoefficients(int module, LrfscDrvrMatrixCoefficientsBuf *matb);
LibLrfscError LibLrfscSetMatrixCoefficients(int module, LrfscDrvrMatrixCoefficientsBuf *matb);

/* ============================================================================ */
/* Get diagnostic snap shot, and set the snap shot time in the RF Pulse         */

LibLrfscError LibLrfscGetDiagSnapShot(int module, LrfscDrvrDiagSnapShot *snap);
LibLrfscError LibLrfscSetDiagSnapShotTime(int module, int diag_time);

/* ============================================================================ */
/* Get and set resonance cavity control parameters                              */

LibLrfscError LibLrfscGetResCtrl(int module, LrfscDrvrResCtrl *ctrl);
LibLrfscError LibLrfscSetResCtrl(int module, LrfscDrvrResCtrl *ctrl);

/* ============================================================================ */
/* Set the KP and KI PIC parameters                                             */

LibLrfscError LibLrfscSetPic(int module, LrfscDrvrPicSetBuf *pic);

/* ============================================================================ */
/* Get and set the configuration PPM functions                                  */

LibLrfscError LibLrfscGetConfig(int module, LrfscDrvrConfigBuf *cbuf);
LibLrfscError LibLrfscSetConfig(int module, LrfscDrvrConfigBuf *cbuf);

/* ============================================================================ */
/* Control and acquire diagnostic sampling in the driver ISR.                   */
/* If the skip_interval is zero, the library calculates the best value based on */
/* the RF pulse length and takes 512 samples based on that measurment. The      */
/* calculated skip interval is returned.                                        */

LibLrfscError LibLrfscSetSample(int module,
				int skip_start,      /* Start position in pulse */
				int pulse,           /* Pulse to acquire in ISR */
				int *skip_interval); /* Skip interval or zero   */

/* ============================================================================ */
/* Get and set active diagnostic signal choices                                 */

LibLrfscError LibLrfscGetDiagChoice(int module, LrfscDrvrDiagChoices *choices);
LibLrfscError LibLrfscSetDiagChoice(int module, LrfscDrvrDiagChoices *choices);

/* ============================================================================ */
/* Get a diagnostic buffer from the drivers casche. These have been acquired in */
/* the drivers ISR. There is a valid field for each signal and cycle. One pulse */
/* is acquired for each chosen signal diagnostic choice each cycle and stored.  */

LibLrfscError LibLrfscGetDiagnostics(int module, LrfscDrvrDiagBuf *diag);
LibLrfscError LibLrfscSetCycle(int module, int cycle);

#endif
