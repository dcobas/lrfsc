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
#include <lrfscdrvr.h>
#include <lrfscdrvrP.h>

/* =============================================== */

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

static int LrfscOpen() {

char fnm[32];
int  i, fn;

   for (i = 1; i <= LrfscDrvrCLIENT_CONTEXTS; i++) {
      sprintf(fnm,"/dev/lrfsc.%1d",i);
      if ((fn = open(fnm,O_RDWR,0)) > 0) {
	 return(fn);
      }
   }
   return 0;
}

/* =============================================== */
/* Open a device handle to the Lrfsc.              */

static int FileDescriptors[LrfscDrvrCLIENT_CONTEXTS];
static int Init = 0;
static int Modules = 0;

LibLrfscError LibLrfscInit(int *fn) {

unsigned long cnt;

   if (Init == 0) bzero((void *) &FileDescriptors, LrfscDrvrCLIENT_CONTEXTS * sizeof(int));

   FileDescriptors[Init] = LrfscOpen();
   if (FileDescriptors[Init]) {

      if (ioctl(FileDescriptors[Init],LrfscDrvrGET_MODULE_COUNT,&cnt) < 0) return LibLrfscErrorIO;
      Modules = cnt;

      if (fn) *fn = FileDescriptors[Init];
      Init++;
      return LibLrfscErrorNONE;
   }

   return LibLrfscErrorINIT;
}

/* =============================================== */

static int LrfscCheckInit(int fd) {

int i;

   if (Init == 0) return 0;
   if (fd == 0) return FileDescriptors[0];
   for (i=0; i<LrfscDrvrCLIENT_CONTEXTS; i++) {
      if (fd == FileDescriptors[i]) {
	 return fd;
      }
   }
   return 0;
}

/* ============================================================== */
/* Connect to an interrupt                                        */

static LrfscDrvrInterrupt Connections = 0;

LibLrfscError LibLrfscConnect(int fd,
			      LrfscDrvrConnection *conx,
			      int timeout,
			      int queue) {

int fdu;

   fdu = LrfscCheckInit(fd);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (conx == NULL) return LibLrfscErrorNULL;

   if (conx->Module > Modules)                             return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&(conx->Module)) < 0) return LibLrfscErrorIO;

   if (conx->Interrupt & ~LrfscDrvrINTERUPT_MASK)          return LibLrfscErrorINTERRUPT;
   if (conx->Pulse > LrfscDrvrPULSES)                      return LibLrfscErrorPULSE;

   if (ioctl(fdu,LrfscDrvrCONNECT,conx)          < 0)      return LibLrfscErrorIO;
   if (ioctl(fdu,LrfscDrvrSET_TIMEOUT,&timeout)  < 0)      return LibLrfscErrorIO;
   if (ioctl(fdu,LrfscDrvrSET_QUEUE_FLAG,&queue) < 0)      return LibLrfscErrorIO;

   Connections |= conx->Interrupt;

   return LibLrfscErrorNONE;
}

/* ============================================================== */
/* Wait for an interrupt                                          */

LibLrfscError LibLrfscWait(int fd,
			   LrfscDrvrConnection *conx) {

int fdu, cc;

   fdu = LrfscCheckInit(0);
   if (fdu == 0)         return LibLrfscErrorINIT;

   if (Connections == 0) return LibLrfscErrorCONNECTIONS;

   if (conx == NULL)     return LibLrfscErrorNULL;

   cc = read(fdu,conx,sizeof(LrfscDrvrConnection));

   if (cc <= 0)          return LibLrfscErrorTIMEOUT;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetVersion(int module, LrfscDrvrVersion *vers) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (vers == NULL)                               return LibLrfscErrorNULL;
   if (ioctl(fdu,LrfscDrvrGET_VERSION,vers) < 0)   return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetStatus(int module, LrfscDrvrStatus *status) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (status == NULL)                             return LibLrfscErrorNULL;
   if (ioctl(fdu,LrfscDrvrGET_STATUS,status) < 0)  return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetState(int module, LrfscDrvrState *state) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (state == NULL)                              return LibLrfscErrorNULL;
   if (ioctl(fdu,LrfscDrvrGET_STATE,state) < 0)    return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetState(int module, LrfscDrvrState state) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (state >= LrfscDrvrSTATES)                   return LibLrfscErrorRANGE;
   if (ioctl(fdu,LrfscDrvrSET_STATE,&state) < 0)   return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetDiagChoice(int module, LrfscDrvrDiagChoices *choices) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                 return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)       return LibLrfscErrorIO;

   if (choices == NULL)                                  return LibLrfscErrorNULL;
   if (ioctl(fdu,LrfscDrvrGET_DIAG_CHOICE,choices) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetDiagChoice(int module, LrfscDrvrDiagChoices *choices) {

int fdu, i;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)      return LibLrfscErrorIO;

   if (choices == NULL)                                 return LibLrfscErrorNULL;
   for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++)
      if ( (*choices)[i] >= LrfscDrvrDiagSIGNALS)       return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrSET_DIAG_CHOICE,choices) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetAnalogueSwitch(int module, LrfscDrvrAnalogSwitch *aswtch) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                   return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)         return LibLrfscErrorIO;

   if (aswtch == NULL)                                     return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_ANALOGUE_SWITCH,aswtch) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetAnalogueSwitch(int module, LrfscDrvrAnalogSwitch aswtch) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                    return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)          return LibLrfscErrorIO;

   if (aswtch > 0xF)                                        return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrSET_ANALOGUE_SWITCH,&aswtch) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetSoftSwitch(int module, LrfscDrvrSoftSwitch *swtch) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if (swtch == NULL)                                 return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_SOFT_SWITCH,swtch) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetSoftSwitch(int module, LrfscDrvrSoftSwitch  swtch) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)      return LibLrfscErrorIO;

   if (swtch >= LrfscDrvrSoftSWITCHES)                 return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrSET_SOFT_SWITCH,&swtch) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetMatrixCoefficients(int module, LrfscDrvrMatrixCoefficientsBuf *matb) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if (matb == NULL)                                  return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_COEFFICIENTS,matb) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetMatrixCoefficients(int module, LrfscDrvrMatrixCoefficientsBuf *matb) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if (matb == NULL)                                  return LibLrfscErrorNULL;
   if (matb->Matrix >= LrfscDrvrMatrixMATRICES)       return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrSET_COEFFICIENTS,matb) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetDiagSnapShot(int module, LrfscDrvrDiagSnapShot *snap) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                            return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)  return LibLrfscErrorIO;

   if (snap == NULL)                                return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_SNAP_SHOTS,snap) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetDiagSnapShotTime(int module, int diag_time) {

int fdu;
LrfscDrvrDiagSnapShot snap;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                 return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)       return LibLrfscErrorIO;

   if (diag_time > 0xFFFF)                               return LibLrfscErrorRANGE;
   snap.DiagTime = diag_time;

   if (ioctl(fdu,LrfscDrvrSET_SNAP_SHOT_TIME,&diag_time) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetResCtrl(int module, LrfscDrvrResCtrl *ctrl) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (ctrl == NULL)                               return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_RES_CTRL,ctrl) < 0)  return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetResCtrl(int module, LrfscDrvrResCtrl *ctrl) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (ctrl == NULL)                               return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrSET_RES_CTRL,ctrl) < 0)  return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetPic(int module, LrfscDrvrPicSetBuf *pic) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (pic == NULL)                                return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrSET_PIC,pic) < 0)        return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetPic(int module, LrfscDrvrPicSetBuf *pic) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                           return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0) return LibLrfscErrorIO;

   if (pic == NULL)                                return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrGET_PIC,pic) < 0)        return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscGetConfig(int module, LrfscDrvrConfigBuf *cbuf) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if (cbuf == NULL)                                  return LibLrfscErrorNULL;
   if (cbuf->Which  >= LrfscDrvrCONFIGS)              return LibLrfscErrorRANGE;
   if (cbuf->Points >  LrfscDrvrCONFIG_POINTS)        return LibLrfscErrorRANGE;
   if (cbuf->Cycle  >= 32)                            return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrGET_CYCLE_CONFIG,cbuf) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetConfig(int module, LrfscDrvrConfigBuf *cbuf) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if (cbuf == NULL)                                  return LibLrfscErrorNULL;
   if (cbuf->Which  >= LrfscDrvrCONFIGS)              return LibLrfscErrorRANGE;
   if (cbuf->Points >  LrfscDrvrCONFIG_POINTS)        return LibLrfscErrorRANGE;
   if (cbuf->Cycle  >= 32)                            return LibLrfscErrorRANGE;

   if (ioctl(fdu,LrfscDrvrSET_CYCLE_CONFIG,cbuf) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

static int ConvertDacValue(short val) {

int upr, lwr;

   upr = ((val & 0x03FF) - 512) << 4;
   lwr = ((val & 0x3C00) >> 10);
   return (upr | lwr);
}

LibLrfscError LibLrfscGetDiagnostics(int module, LrfscDrvrDiagBuf *diag) {

int fdu, i;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                             return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)   return LibLrfscErrorIO;

   if (diag == NULL)                                 return LibLrfscErrorNULL;

   if (diag->Size   > LrfscDrvrBUF_IQ_ENTRIES)       return LibLrfscErrorRANGE;
   if (diag->Pulse  >= LrfscDrvrPULSES)              return LibLrfscErrorRANGE;
   if (diag->Choice >= LrfscDrvrDiagSIGNALS)         return LibLrfscErrorRANGE;
   if (diag->Cycle  >= 32)                           return LibLrfscErrorRANGE;


   if (ioctl(fdu,LrfscDrvrGET_DIAGNOSTICS,diag) < 0) return LibLrfscErrorIO;
   for (i=0; i<diag->Size; i++) {
      if (i >= LrfscDrvrBUF_IQ_ENTRIES) break;
      (*diag).Array[i].I = ConvertDacValue((*diag).Array[i].I);
      (*diag).Array[i].Q = ConvertDacValue((*diag).Array[i].Q);
   }
   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetSample(int module, int skip_start, int pulse, int *skip_interval) {

int fdu;
unsigned long plen;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                                     return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)           return LibLrfscErrorIO;

   if (ioctl(fdu,LrfscDrvrSET_SKIP_START,&skip_start) < 0)   return LibLrfscErrorNULL;

   if (*skip_interval == 0) {
      if (ioctl(fdu,LrfscDrvrGET_PULSE_LENGTH,&plen) < 0)    return LibLrfscErrorNULL;
      *skip_interval = plen / (LrfscDrvrBUF_IQ_ENTRIES * LrfscDrvrTICK_TO_PAIR);
   }

   if (ioctl(fdu,LrfscDrvrSET_SKIP_COUNT,skip_interval) < 0) return LibLrfscErrorNULL;

   if (pulse  >= LrfscDrvrPULSES)                            return LibLrfscErrorRANGE;
   if (ioctl(fdu,LrfscDrvrSET_PULSE,&pulse) < 0)             return LibLrfscErrorNULL;

   return LibLrfscErrorNONE;
}

/* ============================================================== */

LibLrfscError LibLrfscSetCycle(int module, int cycle) {

int fdu;

   fdu = LrfscCheckInit(0);
   if (fdu == 0) return LibLrfscErrorINIT;

   if (module > Modules)                              return LibLrfscErrorMODULE;
   if (ioctl(fdu,LrfscDrvrSET_MODULE,&module) < 0)    return LibLrfscErrorIO;

   if ((cycle < 0) || (cycle > 31))                   return LibLrfscErrorNULL;

   if (ioctl(fdu,LrfscDrvrSET_NEXT_CYCLE,&cycle) < 0) return LibLrfscErrorIO;

   return LibLrfscErrorNONE;
}

