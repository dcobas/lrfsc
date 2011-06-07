/* ============================================================================ */
/* Access library routines for the LRFSC                                        */
/* Julian Lewis Mon 23rd June 2008                                              */
/* ============================================================================ */

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sched.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <liblrfsc.h>

/* =============================================== */

typedef struct {
   int fn[LrfscDrvrMODULE_CONTEXTS];
} lrfsc_handle_t;

static char *ErrorStrings[LibLrfscERRORS + 1] = {
   "No Error, all OK",
   "Library has not been initialized correctly",
   "One or more parameter(s) out of range",
   "A timeout has occured",
   "NULL parameter pointer not allowed here",
   "General driver IO error, see errno",
   "No such Module installed",
   "No such interrupt source",
   "RF pulse number out of range",
   "No connections to wait for",

   "LibLrfsc Illegal error code"
 };

/* =============================================== */
/* Convert an error number to a string             */

char *LibLrfscErrorToString(LibLrfscError err) {

   if (err >= LibLrfscERRORS) return ErrorStrings[LibLrfscERRORS];
   return ErrorStrings[err];
}

/* =============================================== */

static int LrfscOpen(int module) {

char fnm[32];
int  fn;

   sprintf(fnm,"/dev/lrfsc.%1d",module);
   if ((fn = open(fnm,O_RDWR,0)) > 0)
      return(fn);
   return 0;
}

/* =============================================== */
/* Open device handles for the Lrfsc.              */

void *LibLrfscInit() {

lrfsc_handle_t *h;
int i;

   h = (lrfsc_handle_t *) malloc(sizeof(lrfsc_handle_t));
   if (!h) return h;

   for (i=0; i<LrfscDrvrMODULE_CONTEXTS; i++)
      h->fn[i] = LrfscOpen(i);

   return (void *) h;
}

/* ============================================================== */
/* Wait for an interrupt                                          */

LibLrfscError LibLrfscWait(void *handle, int module, LrfscDrvrInterruptBuffer *ibuf) {

lrfsc_handle_t *h;
int cc;

   h = handle;
   if (!h)    return LibLrfscErrorINIT;
   if (!ibuf) return LibLrfscErrorNULL;

   cc = read(h->fn[module], ibuf, sizeof(LrfscDrvrInterruptBuffer));
   if (cc <= 0) return LibLrfscErrorTIMEOUT;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetVersion(void *handle, int module, LrfscDrvrVersion *vers) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                 return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                 return LibLrfscErrorMODULE;
   if (vers == NULL)                                       return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_VERSION,vers) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetStatus(void *handle, int module, LrfscDrvrStatus *status) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                  return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                  return LibLrfscErrorMODULE;
   if (status == NULL)                                      return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_STATUS,status) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetState(void *handle, int module, LrfscDrvrState *state) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                return LibLrfscErrorMODULE;
   if (state == NULL)                                     return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_STATE,state) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetState(void *handle, int module, LrfscDrvrState state) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                 return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                 return LibLrfscErrorMODULE;
   if (ioctl(h->fn[module],LrfscDrvrSET_STATE,&state) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetDiagChoice(void *handle, int module, LrfscDrvrDiagChoices *choices) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                        return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                        return LibLrfscErrorMODULE;
   if (choices == NULL)                                           return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_DIAG_CHOICE,choices) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetDiagChoice(void *handle, int module, LrfscDrvrDiagChoices *choices) {

lrfsc_handle_t *h;
int i;

   h = handle;
   if (!h)                                                        return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                        return LibLrfscErrorMODULE;
   if (choices == NULL)                                           return LibLrfscErrorNULL;
   for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++)
      if ( (*choices)[i] >= LrfscDrvrDiagSIGNALS)                 return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrSET_DIAG_CHOICE,choices) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetAnalogueSwitch(void *handle, int module, LrfscDrvrAnalogSwitch *aswtch) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                           return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                           return LibLrfscErrorMODULE;
   if (aswtch == NULL)                                               return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_ANALOGUE_SWITCH,aswtch) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetAnalogueSwitch(void *handle, int module, LrfscDrvrAnalogSwitch aswtch) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                           return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                           return LibLrfscErrorMODULE;
   if (aswtch > 0xF)                                                 return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrSET_ANALOGUE_SWITCH,&aswtch) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetSoftSwitch(void *handle, int module, LrfscDrvrSoftSwitch *swtch) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (swtch == NULL)                                           return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_SOFT_SWITCH,swtch) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetSoftSwitch(void *handle, int module, LrfscDrvrSoftSwitch swtch) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                       return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                       return LibLrfscErrorMODULE;
   if (swtch >= LrfscDrvrSoftSWITCHES)                           return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrSET_SOFT_SWITCH,&swtch) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetMatrixCoefficients(void *handle, int module, LrfscDrvrMatrixCoefficientsBuf *matb) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (matb == NULL)                                            return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_COEFFICIENTS,matb) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetMatrixCoefficients(void *handle, int module, LrfscDrvrMatrixCoefficientsBuf *matb) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (matb == NULL)                                            return LibLrfscErrorNULL;
   if (matb->Matrix >= LrfscDrvrMatrixMATRICES)       return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrGET_COEFFICIENTS,matb) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetDiagSnapShot(void *handle, int module, LrfscDrvrDiagSnapShot *snap) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (snap == NULL)                                return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_SNAP_SHOTS,snap) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetDiagSnapShotTime(void *handle, int module, int diag_time) {

lrfsc_handle_t *h;
LrfscDrvrDiagSnapShot snap;

   h = handle;
   if (!h)                                                              return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                              return LibLrfscErrorMODULE;
   if (diag_time > 0xFFFF)                                              return LibLrfscErrorRANGE;
   snap.DiagTime = diag_time;
   if (ioctl(h->fn[module],LrfscDrvrSET_SNAP_SHOT_TIME,&diag_time) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetResCtrl(void *handle, int module, LrfscDrvrResCtrl *ctrl) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (ctrl == NULL)                               return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_RES_CTRL,ctrl) < 0)  return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetResCtrl(void *handle, int module, LrfscDrvrResCtrl *ctrl) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                  return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                  return LibLrfscErrorMODULE;
   if (ctrl == NULL)                                        return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrSET_RES_CTRL,ctrl) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetPic(void *handle, int module, LrfscDrvrPicSetBuf *pic) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                            return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)            return LibLrfscErrorMODULE;
   if (pic == NULL)                                   return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrSET_PIC,pic) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetPic(void *handle, int module, LrfscDrvrPicSetBuf *pic) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                            return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)            return LibLrfscErrorMODULE;
   if (pic == NULL)                                   return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrGET_PIC,pic) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetConfig(void *handle, int module, LrfscDrvrConfigBuf *cbuf) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (cbuf == NULL)                                            return LibLrfscErrorNULL;
   if (cbuf->Which  >= LrfscDrvrCONFIGS)                        return LibLrfscErrorRANGE;
   if (cbuf->Points >  LrfscDrvrCONFIG_POINTS)                  return LibLrfscErrorRANGE;
   if (cbuf->Cycle  >= 32)                                      return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrGET_CYCLE_CONFIG,cbuf) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetConfig(void *handle, int module, LrfscDrvrConfigBuf *cbuf) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if (cbuf == NULL)                                            return LibLrfscErrorNULL;
   if (cbuf->Which  >= LrfscDrvrCONFIGS)                        return LibLrfscErrorRANGE;
   if (cbuf->Points >  LrfscDrvrCONFIG_POINTS)                  return LibLrfscErrorRANGE;
   if (cbuf->Cycle  >= 32)                                      return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrSET_CYCLE_CONFIG,cbuf) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

static int ConvertDacValue(short val) {

int upr, lwr;

   upr = ((val & 0x03FF) - 512) << 4;
   lwr = ((val & 0x3C00) >> 10);
   return (upr | lwr);
}

LibLrfscError LibLrfscGetDiagnostics(void *handle, int module, LrfscDrvrDiagBuf *diag) {

lrfsc_handle_t *h;
int i;

   h = handle;
   if (!h)                                                     return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                     return LibLrfscErrorMODULE;
   if (diag == NULL)                                           return LibLrfscErrorNULL;
   if (diag->Size   > LrfscDrvrBUF_IQ_ENTRIES)                 return LibLrfscErrorRANGE;
   if (diag->Pulse  >= LrfscDrvrPULSES)                        return LibLrfscErrorRANGE;
   if (diag->Choice >= LrfscDrvrDiagSIGNALS)                   return LibLrfscErrorRANGE;
   if (diag->Cycle  >= 32)                                     return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrGET_DIAGNOSTICS,diag) < 0) return LibLrfscErrorIO;
   for (i=0; i<diag->Size; i++) {
      if (i >= LrfscDrvrBUF_IQ_ENTRIES) break;
      (*diag).Array[i].I = ConvertDacValue((*diag).Array[i].I);
      (*diag).Array[i].Q = ConvertDacValue((*diag).Array[i].Q);
   }
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetSample(void *handle, int module, int skip_start, int pulse, int *skip_interval) {

lrfsc_handle_t *h;
unsigned long plen;

   h = handle;
   if (!h)                                                             return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                             return LibLrfscErrorMODULE;
   if (ioctl(h->fn[module],LrfscDrvrSET_SKIP_START,&skip_start) < 0)   return LibLrfscErrorNULL;
   if (*skip_interval == 0) {
      if (ioctl(h->fn[module],LrfscDrvrGET_PULSE_LENGTH,&plen) < 0)    return LibLrfscErrorNULL;
      *skip_interval = plen / (LrfscDrvrBUF_IQ_ENTRIES * LrfscDrvrTICK_TO_PAIR);
   }
   if (ioctl(h->fn[module],LrfscDrvrSET_SKIP_COUNT,skip_interval) < 0) return LibLrfscErrorNULL;
   if (pulse  >= LrfscDrvrPULSES)                                      return LibLrfscErrorRANGE;
   if (ioctl(h->fn[module],LrfscDrvrSET_PULSE,&pulse) < 0)             return LibLrfscErrorNULL;
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetCycle(void *handle, int module, int cycle) {

lrfsc_handle_t *h;

   h = handle;
   if (!h)                                                      return LibLrfscErrorINIT;
   if (module >= LrfscDrvrMODULE_CONTEXTS)                      return LibLrfscErrorMODULE;
   if ((cycle < 0) || (cycle > 31))                             return LibLrfscErrorNULL;
   if (ioctl(h->fn[module],LrfscDrvrSET_NEXT_CYCLE,&cycle) < 0) return LibLrfscErrorIO;
   return LibLrfscErrorNONE;
}

