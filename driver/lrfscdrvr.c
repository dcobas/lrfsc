/* ====================================================================== */
/* Lowlevel RF module driver LRFC                                         */
/* Julian Lewis Monday 5th May 2008                                       */
/* ====================================================================== */

#include <dldd.h>
#include <errno.h>
#include <kernel.h>
#include <io.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/timeout.h>
#include <mem.h>
#include <sys/file.h>
#include <string.h>
#include <signal.h>
#include <conf.h>

#include <lrfscdrvr.h>
#include <lrfscdrvrP.h>

#ifndef COMPILE_TIME
#define COMPILE_TIME 0
#endif

extern int  kkprintf(char *, ...);
extern void cprintf  _AP((char *fmt, ...));
extern int  swait    _AP((int *sem, int flag));
extern int  ssignal  _AP((int *sem));
extern int  ssignaln _AP((int *sem, int count));
extern int  scount   _AP((int *sem));
extern int  sreset   _AP((int *sem));
extern void sysfree  _AP((char *, long));
extern int  recoset  _AP((void));
extern void noreco   _AP((void));
extern int _kill     _AP((int pid, int signal));
extern int _killpg   _AP((int pgrp, int signal));
extern pid_t getpid  _AP((void));

extern  char *sysbrk ();
extern  int  timeout ();

#ifndef OK
#define OK 0
#endif

#ifndef SYSERR
#define SYSERR (-1)
#endif

/* ====================================================================== */
/* references specifics to CES PowerPC Cpus RIO806x                       */

#include <ces/vmelib.h>

extern unsigned long find_controller();
extern unsigned long return_controller();
extern int vme_intset();
extern int vme_intclr();
extern void disable_intr();
extern void enable_intr();

/* Flash the i/o pipe line */

#define EIEIO	asm("eieio")
#ifndef SYNC
#define SYNC	asm("sync")
#endif

/* ====================================================================== */
/* Standard driver interfaces                                             */

void  IntrHandler();
char *LrfscDrvrInstall();
int   LrfscDrvrUninstall();
int   LrfscDrvrOpen();
int   LrfscDrvrClose();
int   LrfscDrvrIoctl();
int   LrfscDrvrRead();
int   LrfscDrvrWrite();
int   LrfscDrvrSelect();

/* ====================================================================== */
/* Declare a static working area global pointer.                          */

static LrfscDrvrWorkingArea *Wa = NULL;

/*========================================================================*/
/* Short copy                                                             */
/*========================================================================*/

void CopyWords(unsigned short *dst, unsigned short *src, int bytes) {
int i, wds;

   wds = bytes/sizeof(unsigned short);
   for (i=0; i<wds; i++) dst[i] = src[i];
}

/*========================================================================*/
/* Cancel a timeout in a safe way                                         */
/*========================================================================*/

static void CancelTimeout(t)
int *t; {

int ps,v;

   disable(ps);
   {
      if ((v = *t)) {
	 *t = 0;
	 cancel_timeout(v);
      }
   }
   restore(ps);
}

/* ====================================================================== */
/* Raw IO                                                                 */
/* ====================================================================== */

#define READ_FLAG 0
#define WRITE_FLAG 1

static int RawIo(LrfscDrvrModuleContext *mcon,
		 LrfscDrvrRawIoBlock    *riob,
		 int rwflg) {

volatile LrfscDrvrModuleAddress *moad; /* Module address, vector, level, ssch */
volatile unsigned short         *mmap; /* Module Memory map */
unsigned short                  *uary;
int                             i, j, rval;

   moad = &(mcon->Address);
   if (riob->RamFlag) mmap = moad->RamAddress;
   else               mmap = moad->VMEAddress;
   uary = riob->UserArray;
   rval = OK;

   if (!recoset()) {         /* Catch bus errors */
      for (i=0; i<riob->Size; i++) {
	 j = riob->Offset+i;
	 if (rwflg) mmap[j] = uary[i];
	 else       uary[i] = mmap[j];
	 EIEIO;
	 SYNC;
      }
   } else {
      noreco();

      cprintf("LrfscDrvr: BUS-ERROR: Module:%d Offset:0x%X Address:0x%X ",
	      (int) mcon->ModuleIndex+1,
	      (int) j,
	      (int) &(mmap[j]));

      if (riob->RamFlag) cprintf("RAM\n");
      else               cprintf("REG\n");

      pseterr(ENXIO);
      return SYSERR;
   }
   noreco();
   return riob->Size;
}

/* ====================================================================== */
/* Copy configuration to module context, hardware will be updated in ISR  */
/* or imediatley if the state is configuration.                           */
/* ====================================================================== */

int ConfToMcon(LrfscDrvrModuleContext *mcon, LrfscDrvrConfigBuf *buf) {

LrfscDrvrConfigArray *mcdst;        /* Module Context Configuration array */

   if ((buf->Points >  LrfscDrvrCONFIG_POINTS)
   ||  (buf->Which  >  LrfscDrvrCONFIGS)
   ||  (buf->Cycle  >= LrfscDrvrCYCLES)) {

      pseterr(EINVAL);
      return SYSERR;
   }

   /* Keep a copy of the unconverted user array in the module context */

   mcdst = &((*mcon).Configs[(int) buf->Which][(int) buf->Cycle]);
   bcopy((void *) buf->Array, (void *) mcdst, sizeof(LrfscDrvrConfigArray));
   mcon->ValidConfigs[buf->Which][buf->Cycle] = 1;

   return OK;
}

/* ====================================================================== */
/* Retrieve configuration from module context                             */
/* ====================================================================== */

int MconToConf(LrfscDrvrModuleContext *mcon, LrfscDrvrConfigBuf *buf) {

LrfscDrvrConfigArray *mcsrc;        /* Module Context Configuration array */

   if ((buf->Points > LrfscDrvrCONFIG_POINTS)
   ||  (buf->Which  > LrfscDrvrCONFIGS)
   ||  (buf->Cycle  > LrfscDrvrCYCLES)) {

      pseterr(EINVAL);
      return SYSERR;
   }

   mcsrc = &((*mcon).Configs[(int) buf->Which][(int) buf->Cycle]);
   bcopy((void *) mcsrc, (void *) buf->Array, sizeof(LrfscDrvrConfigArray));

   return OK;
}

/* ====================================================================== */
/* Set a configuration on a module                                        */
/* ====================================================================== */

static int SetConfiguration(LrfscDrvrModuleContext *mcon, LrfscDrvrConfig cnf, int cyc) {

volatile LrfscDrvrModuleAddress *moad;        /* Module address, vector, level */
volatile LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile LrfscDrvrIQPair        *dst;
volatile LrfscDrvrIQPair        *src;

int i, ps;
unsigned short old;

   disable(ps);
   {
      moad = &(mcon->Address);
      mmap  = (LrfscDrvrMemoryMap *) moad->VMEAddress;

      old = mmap->RamSelect;
      mmap->RamSelect = (unsigned short) cnf;

      EIEIO;
      SYNC;

      src = &((*mcon).Configs[(int) cnf][(int) cyc][0]);

      dst = (volatile LrfscDrvrIQPair *)
	    ( (unsigned long) moad->RamAddress
	  |   (unsigned long) ((cyc & 31) << 14) );

      for (i=0; i<LrfscDrvrCONFIG_POINTS; i++) {
	 dst->I = src->I;
	 dst->Q = src->Q;
	 dst++; src++;
      }
      mmap->RamSelect = old;
   }
   restore(ps);
   return OK;
}

/* ====================================================================== */
/* Set coefficients on a module                                           */
/* ====================================================================== */

static int SetMatrixCoefficient(LrfscDrvrModuleContext *mcon, LrfscDrvrMatrix mnm) {

volatile LrfscDrvrModuleAddress *moad;        /* Module address, vector, level */
volatile LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */
LrfscDrvrMatrixCoefficients *mat;

   moad = &(mcon->Address);
   mmap = (LrfscDrvrMemoryMap *) moad->VMEAddress;
   mat  = &(mcon->Coefficients[(int) mnm]);
   CopyWords((unsigned short *) &(mmap->Matrix[(int) mnm]),
	     (unsigned short *) mat,
	     sizeof(LrfscDrvrMatrixCoefficients));
   return OK;
}

/* ====================================================================== */
/* Set signal choice on a module                                          */
/* ====================================================================== */

static int SetSignalChoice(LrfscDrvrModuleContext *mcon, int sgn) {

volatile LrfscDrvrModuleAddress *moad;        /* Module address, vector, level */
volatile LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */

   moad = &(mcon->Address);
   mmap = (LrfscDrvrMemoryMap *) moad->VMEAddress;
   mmap->SignalChoices[sgn] = (unsigned short) mcon->SignalChoices[sgn];
   return OK;
}

/* ====================================================================== */
/* Reset the module to known state                                        */
/* ====================================================================== */

static int Reset(LrfscDrvrModuleContext *mcon) {

volatile LrfscDrvrModuleAddress *moad;        /* Module address, vector, level */
volatile LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */
int                              i,j;

   moad = &(mcon->Address);
   mmap  = (LrfscDrvrMemoryMap *) moad->VMEAddress;
   if (!recoset()) {

      mmap->State = (unsigned short) LrfscDrvrStateCONFIG;
      EIEIO;
      SYNC;

      for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++) SetSignalChoice(mcon, i);
      for (i=0; i<LrfscDrvrMatrixMATRICES; i++) SetMatrixCoefficient(mcon, (LrfscDrvrMatrix) i);
      for (i=0; i<LrfscDrvrCONFIGS; i++) {
	 mmap->RamSelect = (unsigned short) i;
	 for (j=0; j<LrfscDrvrCYCLES; j++) SetConfiguration(mcon, (LrfscDrvrConfig) i, j);
      }

      mmap->Control            = (unsigned short) mcon->Control;
      mmap->Vector             = (unsigned short) moad->InterruptVector;
      mmap->RamSelect          = (unsigned short) mcon->RamSelect;
      mmap->ResCtrl.Time       = (unsigned short) mcon->ResCtrl.Time;
      mmap->ResCtrl.Value      = (unsigned short) mcon->ResCtrl.Value;
      mmap->SwitchCtrl         = (unsigned short) mcon->SwitchCtrl;
      mmap->SoftSwitch         = (unsigned short) mcon->SoftSwitch;
      mmap->SnapShot.DiagTime  = (unsigned short) mcon->DiagTime;
      mmap->Pic.KP             = (  signed short) mcon->Pic.KP;
      mmap->Pic.KI             = (  signed short) mcon->Pic.KI;
      mmap->NextCycle          = (unsigned short) 0;
      mmap->RfOnMaxLen         = (unsigned short) mcon->RfOnMaxLen;
      mmap->State              = (unsigned short) mcon->State;
      EIEIO;
      SYNC;

   } else {
      noreco();
      cprintf("LrfscDrvr: BUS-ERROR: Module:%d. VME Addr:%x Vect:%x Level:%x\n",
	      mcon->ModuleIndex+1,
	      moad->VMEAddress,
	      moad->InterruptVector,
	      moad->InterruptLevel);

      pseterr(ENXIO); /* No such device or address */
      return SYSERR;
   }
   noreco();
   return OK;
}

/* ====================================================================== */
/* Add a module to the driver, called per module from install             */
/* ====================================================================== */

static int AddModule(LrfscDrvrModuleContext *mcon, unsigned long index) {

volatile LrfscDrvrModuleAddress *moad; /* Module address, vector, level, ssch */
volatile LrfscDrvrMemoryMap     *mmap; /* Module Memory map */
unsigned long                    addr; /* VME base address */
unsigned long                    coco; /* Completion code */

struct pdparam_master param;
volatile short *vmeaddr;
volatile short *ramaddr;

unsigned short wrd;

   /* Compute Virtual memory address as seen from system memory mapping */

   moad = &(mcon->Address);

   /* CES: build an address window (64 kbyte) for VME A24-D16 accesses */

   addr = (unsigned long) moad->VMEAddress;
   addr &= 0x007fffff;                      /* A24 - Bit-19 */

   bzero((char *)&param, sizeof(struct pdparam_master));

   param.iack   = 1;     /* no iack */
   param.rdpref = 0;     /* no VME read prefetch option */
   param.wrpost = 0;     /* no VME write posting option */
   param.swap   = 1;     /* VME auto swap option */
   param.dum[0] = 0;     /* window is sharable */

   vmeaddr = (void *) find_controller(addr,    /* Vme base address */
				      0x10000, /* Module address space */
				      0x39,    /* Address modifier A24 */
				      0,       /* Offset */
				      2,       /* Size is D16 */
				      &param); /* Parameter block */
   if (vmeaddr == (void *) (-1)) {
      cprintf("LrfscDrvr: find_controller: ERROR: Module:%d. VME Addr:%x\n",
	      index+1,
	      addr);

      pseterr(ENXIO);        /* No such device or address */
      return SYSERR;
   }
   moad->VMEAddress = (unsigned short *) vmeaddr;

   mmap  = (LrfscDrvrMemoryMap *) moad->VMEAddress;

   mmap->State = (unsigned short) 0; /* Interrupts off/Config mode */
   wrd = mmap->IrqSource;            /* Clear interrup register */

   addr |= LrfscDrvrSTART_OF_RAM;
   ramaddr = (void *) find_controller(addr,    /* Vme RAM base address */
				      LrfscDrvrTOTAL_RAM_SIZE, /* Module address space */
				      0x39,                    /* Address modifier A24 */
				      0,                       /* Offset */
				      2,                       /* Size is D16 */
				      &param);                 /* Parameter block */
   if (ramaddr == (void *) (-1)) {
      cprintf("LrfscDrvr: find_controller: ERROR: Module:%d. RAM Addr:%x\n",
	      index+1,
	      addr);

      pseterr(ENXIO); /* No such device or address */
      return SYSERR;
   }
   moad->RamAddress = (unsigned short *) ramaddr;

   if (!recoset()) {
      wrd = (unsigned short) *ramaddr;
      EIEIO;
      SYNC;

   } else {
      noreco();
      cprintf("LrfscDrvr: BUS-ERROR: Module:%d. RAM Addr:%x Vect:%x Level:%x\n",
	      mcon->ModuleIndex+1,
	      moad->RamAddress,
	      moad->InterruptVector,
	      moad->InterruptLevel);

      pseterr(ENXIO); /* No such device or address */
      return SYSERR;
   }
   noreco();

   coco = vme_intset((moad->InterruptVector),  /* Vector */
		     (void *) IntrHandler,     /* Address of ISR */
		     (char *) mcon,            /* Parameter to pass */
		      0);                      /* Don't save previous */
   if (coco < 0) {
      cprintf("LrfscDrvr: vme_intset: ERROR %d, MODULE %d\n",coco,index+1);
      pseterr(EFAULT);
      return SYSERR;
   }

   mcon->ModuleIndex             = index;
   mcon->State                   = LrfscDrvrStateCONFIG;
   mcon->Control                 = LrfscDrvrControlNONE;
   mcon->RamSelect               = LrfscDrvrRamSETPOINTS;
   mcon->ResCtrl.Time            = 0xFF;
   mcon->SwitchCtrl              = 0;
   mcon->SoftSwitch              = LrfscDrvrSotfSwitchMAIN_CLOSED;
   mcon->DiagTime                = 0x2000;
   mcon->RfOnMaxLen              = 45056;
   mcon->Pic.KI                  = 246;
   mcon->Pic.KP                  = 640;
   mcon->SignalChoices[0]        = LrfscDrvrDiagCAVITY;
   mcon->SignalChoices[1]        = LrfscDrvrDiagFORWARD;
   mcon->SignalChoices[2]        = LrfscDrvrDiagREFLECTED;
   mcon->SignalChoices[3]        = LrfscDrvrDiagOUTPUT;
   mcon->Coefficients[0].MatrixA = 0xED2C;
   mcon->Coefficients[0].MatrixB = 0x0EC6;
   mcon->Coefficients[0].MatrixC = 0xF13A;
   mcon->Coefficients[0].MatrixD = 0xED2C;
   mcon->Coefficients[1].MatrixA = 0xED2C;
   mcon->Coefficients[1].MatrixB = 0x0EC6;
   mcon->Coefficients[1].MatrixC = 0xF13A;
   mcon->Coefficients[1].MatrixD = 0xED2C;
   mcon->Coefficients[2].MatrixA = 0xED2C;
   mcon->Coefficients[2].MatrixB = 0x0EC6;
   mcon->Coefficients[2].MatrixC = 0xF13A;
   mcon->Coefficients[2].MatrixD = 0xED2C;
   mcon->Pulse                   = 1;
   return Reset(mcon);
}

/* ====================================================================== */
/* Connect to an interrupt                                                */
/* ====================================================================== */

static int Connect(LrfscDrvrConnection *conx,
		   LrfscDrvrClientContext *ccon) {

LrfscDrvrModuleContext *mcon;
int modi = 0;
int clni;

   if (conx->Module) modi = conx->Module -1;
   mcon = &(Wa->ModuleContexts[modi]);
   if (mcon->State == LrfscDrvrStateCONFIG) {
      pseterr(EBUSY); /* Device busy, in local */
      return SYSERR;
   }

   clni = ccon->ClientIndex;
   mcon->Clients[clni] |= conx->Interrupt;
   if ((conx->Interrupt & LrfscDrvrInterruptPULSE) != 0)
      ccon->Pulse = conx->Pulse;

   return OK;
}

/* ====================================================================== */
/* Disconnect from module interrupts                                      */
/* ====================================================================== */

static int DisConnect(LrfscDrvrConnection *conx,
		      LrfscDrvrClientContext *ccon) {

LrfscDrvrModuleContext *mcon;
int modi = 0;
int clni;

   if (conx->Module) modi = conx->Module -1;
   mcon = &(Wa->ModuleContexts[modi]);

   clni = ccon->ClientIndex;
   mcon->Clients[clni] &= ~conx->Interrupt;
   if ((mcon->Clients[clni] & LrfscDrvrInterruptPULSE) == 0)
      ccon->Pulse = 0;
   return OK;
}

/* ====================================================================== */
/* Interrupt handler                                                      */
/* ====================================================================== */

void IntrHandler(LrfscDrvrModuleContext *mcon) {

volatile LrfscDrvrMemoryMap     *mmap;
LrfscDrvrClientContext          *ccon;
LrfscDrvrClientQueue            *queue;
LrfscDrvrConnection              rb;
unsigned int                     i, j, iq, ci;
unsigned short                   isrc, csrc, pnum, cnum, npnum;

   mmap =  (LrfscDrvrMemoryMap *) mcon->Address.VMEAddress;

   isrc = mmap->IrqSource;
   cnum = mmap->PresCycle;
   pnum = mmap->PulseNumber;

   if (isrc & LrfscDrvrInterruptSTART_CYCLE) npnum = 1;
   else                                      npnum = pnum + 1;

   bzero((void *) &rb, sizeof(LrfscDrvrConnection));
   rb.Module = mcon->ModuleIndex + 1;

   while (isrc) {
      for (ci=0; ci<LrfscDrvrCLIENT_CONTEXTS; ci++) {
	 ccon = &(Wa->ClientContexts[ci]);
	 if (ccon->InUse) {
	    csrc = isrc & mcon->Clients[ci];
	    if (csrc & LrfscDrvrInterruptPULSE) {
	       if ((ccon->Pulse == 0) || (pnum == ccon->Pulse)) {
		  rb.Pulse = pnum;
		  rb.Interrupt |= LrfscDrvrInterruptPULSE;
	       }
	    }
	    if (csrc & LrfscDrvrInterruptSTART_CYCLE)
	       rb.Interrupt |= LrfscDrvrInterruptSTART_CYCLE;
	 }

	 if (rb.Interrupt) {
	    queue = &(ccon->Queue);
	    if (queue->Size < LrfscDrvrCLIENT_QUEUE_SIZE) {
	       queue->Size++;
	       iq = queue->Head;
	       queue->Queue[iq++] = rb;
	       ssignal(&(ccon->Semaphore));
	       if (iq < LrfscDrvrCLIENT_QUEUE_SIZE) queue->Head = iq;
	       else                                 queue->Head = 0;
	    } else {
	       queue->Missed++;
	    }
	    rb.Interrupt = 0;
	    rb.Pulse     = 0;
	 }
      }

      if (mcon->State == LrfscDrvrStatePROD_REMOTE) {
	 if ((!mcon->AqnDone) && (mcon->Pulse == npnum)) {
	    mmap->Control |= LrfscDrvrControlDO_AQN;
	    mcon->AqnDone = npnum;
	 }
      }

      if (isrc & LrfscDrvrInterruptPULSE) {
	 for (i=0; i<LrfscDrvrCONFIGS; i++) {
	    for (j=0; j<LrfscDrvrCYCLES; j++) {
	       if (mcon->ValidConfigs[i][j]) {
		  mcon->ValidConfigs[i][j] = 0;
		  mmap->State = 0;
		  mmap->RamSelect = (unsigned short) i;
		  SetConfiguration(mcon, (LrfscDrvrConfig) i, j);
		  mmap->State = (unsigned short) mcon->State;
	       }
	    }
	 }
      }
      isrc = mmap->IrqSource;
      pnum = mmap->PulseNumber;
      cnum = mmap->PresCycle;
   }
}

/***************************************************************************/
/* INSTALL                                                                 */
/***************************************************************************/

char *LrfscDrvrInstall(LrfscDrvrInfoTable *info) {

int                              i, count;
LrfscDrvrModuleContext          *mcon;     /* Module context */
volatile LrfscDrvrModuleAddress *moad;     /* Modules address */
LrfscDrvrWorkingArea            *wa;

   /*Allocate the driver working area*/

   wa = (LrfscDrvrWorkingArea *) sysbrk(sizeof(LrfscDrvrWorkingArea));
   if (!wa) {
      cprintf("LrfscDrvrInstall: NOT ENOUGH MEMORY(WorkingArea)\n");
      pseterr(ENOMEM);          /* Not enough core */
      return((char *) SYSERR);
   }
   Wa = wa;                     /* Global working area pointer */

   /****************************************/
   /* Initialize the driver's working area */
   /* and add the modules ISR into LynxOS  */
   /****************************************/

   bzero((void *) wa, sizeof(LrfscDrvrWorkingArea));       /* Clear working area */

   count = 0;
   for (i=0; i<info->Modules; i++) {
      mcon  = &(wa->ModuleContexts[i]);
      moad  = &(mcon->Address);
      *moad = info->Addresses[i];

      if (AddModule(mcon,count) != OK) {
	 cprintf("LrfscDrvr: Module: %d Not Installed\n", i+1);
	 bzero((void *) moad,sizeof(LrfscDrvrModuleAddress));     /* Wipe it out */
      } else {
	 count++;
	 mcon->ModuleIndex = count -1;
	 cprintf("LrfscDrvr: Module %d. VME Addr: %x RAM Addr: %x Vect: %x Level: %x Installed OK\n",
		 count,
		 moad->VMEAddress,
		 moad->RamAddress,
		 moad->InterruptVector,
		 moad->InterruptLevel);
      }

   }
   wa->Modules = count;
   cprintf("LrfscDrvr: Installed: %d LRFSC Modules in Total\n",count);
   return((char*) wa);
}

/***************************************************************************/
/* UNINSTALL                                                               */
/***************************************************************************/

int LrfscDrvrUninstall(wa)
LrfscDrvrWorkingArea * wa; {     /* Drivers working area pointer */

   cprintf("LrfscDrvr: ERROR: UnInstall capabilities are not supported\n");

   /* EPERM = "Operation not permitted" */

   pseterr(EPERM);             /* Not supported */
   return SYSERR;

}

/***************************************************************************/
/* OPEN                                                                    */
/***************************************************************************/

int LrfscDrvrOpen(wa,dnm,flp)
LrfscDrvrWorkingArea *wa;
int dnm;
struct file *flp; {

LrfscDrvrClientContext *ccon; /* Client context */
int                     cnum; /* Client number */

   /* We allow one client per minor device, we use the minor device */
   /* number as an index into the client contexts array. */

   cnum = minor(flp->dev) -1;
   if ((cnum < 0) || (cnum >= LrfscDrvrCLIENT_CONTEXTS)) {

      /* EFAULT = "Bad address" */

      pseterr(EFAULT);
      return SYSERR;
   }
   ccon = &(wa->ClientContexts[cnum]);


   /* If already open by someone else, give a permission denied error */

   if (ccon->InUse) {

      /* This next error is normal */
      /* EBUSY = "Resource busy" */

      pseterr(EBUSY);           /* File descriptor already open */
      return SYSERR;
   }

   /* Initialize a client context */

   bzero((void *) ccon,(long) sizeof(LrfscDrvrClientContext));
   ccon->ClientIndex = cnum;
   ccon->Timeout     = LrfscDrvrDEFAULT_TIMEOUT;
   ccon->InUse       = 1;
   ccon->Pid         = getpid();
   return OK;
}

/***************************************************************************/
/* CLOSE                                                                   */
/***************************************************************************/

int LrfscDrvrClose(wa, flp)
LrfscDrvrWorkingArea * wa;          /* Working area */
struct file * flp; {               /* File pointer */

int cnum;                          /* Client number */

LrfscDrvrModuleContext     * mcon;   /* Module context */
LrfscDrvrClientContext     * ccon;   /* Client context */

   /* We allow one client per minor device, we use the minor device */
   /* number as an index into the client contexts array.            */

   cnum = minor(flp->dev) -1;
   if ((cnum < 0) || (cnum >= LrfscDrvrCLIENT_CONTEXTS)) {

      /* EFAULT = "Bad address" */

      pseterr(EFAULT);
      return SYSERR;
   }
   ccon = &(Wa->ClientContexts[cnum]);

   /* Cancel any pending timeouts */
   /* Disconnect this client from events */

   CancelTimeout(&ccon->Timer);
   mcon->Clients[ccon->ClientIndex] = 0;

   /* Wipe out the client context */

   bzero((void *) ccon,sizeof(LrfscDrvrClientContext));
   return(OK);
}

/***************************************************************************/
/* IOCTL                                                                   */
/***************************************************************************/

int LrfscDrvrIoctl(wa, flp, cm, arg)
LrfscDrvrWorkingArea *wa;
struct file *flp;
LrfscDrvrControlFunction cm;
char *arg; {

int cnum;                       /* Client number */
int rcnt, wcnt;                 /* Readable, Writable byte counts at arg address */
long lav;                       /* Long value from arg */
volatile long *lap;             /* Long Value pointed to by Arg */
volatile unsigned short *sap;   /* Short IO value pointer */
unsigned short sav;             /* Short IO value */

volatile LrfscDrvrMemoryMap     *mmap;

LrfscDrvrClientContext          *ccon;
LrfscDrvrModuleContext          *mcon;
LrfscDrvrRawIoBlock             *riob;
LrfscDrvrConnection             *conx;
LrfscDrvrDiagChoices            *diagch;
LrfscDrvrDiagSignalChoice       sigch;
LrfscDrvrResCtrl                *resctl;
LrfscDrvrAnalogSwitch           *anlgsw;
LrfscDrvrSoftSwitch             *softsw;
LrfscDrvrMatrixCoefficientsBuf  *coefbuf;
LrfscDrvrMatrix                 matrx;
LrfscDrvrDiagSnapShot           *snapsht;
LrfscDrvrPicSetBuf              *picbuff;
LrfscDrvrConfigBuf              *confbuf;
LrfscDrvrDiagBuf                *diagbuf;
LrfscDrvrVersion                *verbuf;
LrfscDrvrModuleAddress          *moadbuf;
LrfscDrvrClientList             *clsbuf;
LrfscDrvrClientConnections      *ccnbuf;
LrfscDrvrIQPair                 *src, *dst;
LrfscDrvrIQPair                 *ram;
LrfscDrvrDiagSignalChoice       sigc;

int i, j, si, err, size, skpstr, skpcnt;

   /* Check argument contains a valid address for reading or writing. */
   /* We can not allow bus errors to occur inside the driver due to   */
   /* the caller providing a garbage address in "arg". So if arg is   */
   /* not null set "rcnt" and "wcnt" to contain the byte counts which */
   /* can be read or written to without error. */

   if (arg != NULL) {
      rcnt = rbounds((int)arg);       /* Number of readable bytes without error */
      wcnt = wbounds((int)arg);       /* Number of writable bytes without error */
      if (rcnt < sizeof(long)) {      /* We at least need to read one long */
	 cprintf("LrfscDrvrIoctl: ILLEGAL NON NULL ARG POINTER, RCNT=%d/%d\n",
		 rcnt,sizeof(long));
	 pseterr(EINVAL);          /* Invalid argument */
	 return SYSERR;
      }
      lav = *((long *) arg);       /* Long argument value */
      lap =   (long *) arg ;       /* Long argument pointer */
   } else {
      rcnt = 0; wcnt = 0; lav = 0; lap = NULL; /* Null arg = zero read/write counts */
   }

   /* We allow one client per minor device, we use the minor device */
   /* number as an index into the client contexts array. */

   cnum = minor(flp->dev) -1;
   if ((cnum < 0) || (cnum >= LrfscDrvrCLIENT_CONTEXTS)) {
      pseterr(ENODEV);          /* No such device */
      return SYSERR;
   }

   /* We can't control a file which is not open. */

   ccon = &(wa->ClientContexts[cnum]);
   if (ccon->InUse == 0) {
      cprintf("LrfscDrvrIoctl: DEVICE %2d IS NOT OPEN\n",cnum+1);
      pseterr(EBADF);           /* Bad file number */
      return SYSERR;
   }

   mcon = &(wa->ModuleContexts[ccon->ModuleIndex]); /* Default module selected */
   mmap =  (LrfscDrvrMemoryMap     *) mcon->Address.VMEAddress;

   /*************************************/
   /* Decode callers command and do it. */
   /*************************************/

   switch (cm) {

      case LrfscDrvrSET_MODULE:              /* Select the module to work with */
	 if ((lav >= 1) &&  (lav <= Wa->Modules)) {
		 ccon->ModuleIndex = lav -1;
		 return OK;
	 }
      break;

      case LrfscDrvrGET_MODULE:
	 if (lap) {
		 *lap = ccon->ModuleIndex +1;
		 return OK;
	 }
      break;

      case LrfscDrvrGET_MODULE_COUNT:
	 if (lap) {
	    *lap = Wa->Modules;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_MODULE_ADDRESS:       /* LrfscDrvrModuleAddress Read */
	 if (wcnt >= sizeof(LrfscDrvrModuleAddress)) {
	    moadbuf = (LrfscDrvrModuleAddress *) arg;
	    *moadbuf = mcon->Address;
	    return OK;
	 }
      break;

      case LrfscDrvrRESET:
	 return Reset(mcon);

      case LrfscDrvrGET_STATUS:               /* LrfscDrvrStatus Get status */
	 if (lap) {
		 sav = mmap->Status;
		 *lap = (unsigned long) sav;
		 return OK;
	 }
      break;

      case LrfscDrvrSET_SW_DEBUG:             /* ulong Set software debug level */
	 if (lap) {
	    ccon->Debug = lav;
	    if (lav) cprintf("lrfsc driver:Debug level:%d on\n", (int) lav);
	    else     cprintf("lrfsc driver:Debug off\n");
	    return OK;
	 }
      break;

      case LrfscDrvrGET_SW_DEBUG:             /* ulong Get software debug level */
	 if (lap) {
	    *lap = ccon->Debug;
	    return OK;
	 }
      break;

      case LrfscDrvrRAW_READ:         /* LrfscDrvrRawIoBlock Read  */
	 if (rcnt >= sizeof(LrfscDrvrRawIoBlock)) {
	    riob = (LrfscDrvrRawIoBlock *) arg;
	    RawIo(mcon, riob, READ_FLAG);
	    return OK;
	 }
      break;

      case LrfscDrvrRAW_WRITE:        /* LrfscDrvrRawIoBlock Write */
	 if (rcnt >= sizeof(LrfscDrvrRawIoBlock)) {
	    riob = (LrfscDrvrRawIoBlock *) arg;
	    RawIo(mcon, riob, WRITE_FLAG);
	    return OK;
	 }
      break;

      case LrfscDrvrSET_TIMEOUT:              /* ulong Set read timeout */
	 if (lap) {
	    ccon->Timeout = lav;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_TIMEOUT:              /* ulong Get read timeout */
	 if (lap) {
	    *lap = ccon->Timeout;
	    return OK;
	 }
      break;

      case LrfscDrvrCONNECT:
	 if (rcnt >= sizeof(LrfscDrvrConnection)) {
	    conx = (LrfscDrvrConnection *) arg;
	    if (conx->Interrupt) return Connect(conx, ccon);
	 }
	 break;

      case LrfscDrvrDISCONNECT:
	 if (rcnt >= sizeof(LrfscDrvrConnection)) {
	    conx = (LrfscDrvrConnection *) arg;
	    if (conx->Interrupt == 0) conx->Interrupt = LrfscDrvrINTERUPT_MASK;
	    return DisConnect(conx, ccon);
	 }
      break;

      case LrfscDrvrSET_QUEUE_FLAG:           /* ulong Set queue off 1 or on 0 */
	 if (lap) {
	    ccon->Queue.QOff = lav;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_QUEUE_FLAG:
	 if (lap) {
	    *lap = ccon->Queue.QOff;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_QUEUE_SIZE:
	 if (lap) {
	    *lap = ccon->Queue.Size;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_QUEUE_OVERFLOW:
	 if (lap) {
	    *lap = ccon->Queue.Missed;
	    ccon->Queue.Missed = 0;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_CLIENT_LIST:                  /* LrfscDrvrClientList Get all clients */
	 if (wcnt >= sizeof(LrfscDrvrClientList)) {
	    clsbuf = (LrfscDrvrClientList *) arg;
	    bzero((void *) clsbuf, sizeof(LrfscDrvrClientList));
	    for (i=0; i<LrfscDrvrCLIENT_CONTEXTS; i++) {
	       ccon = &(wa->ClientContexts[i]);
	       if (ccon->InUse) clsbuf->Pid[clsbuf->Size++] = ccon->Pid;
	    }
	    return OK;
	 }
      break;

      case LrfscDrvrGET_CLIENT_CONNECTIONS:   /* LrfscDrvrClientConnections Get clients connections */
	 if (wcnt >= sizeof(LrfscDrvrClientConnections)) {
	    ccnbuf = (LrfscDrvrClientConnections *) arg;
	    ccnbuf->Size = 0; size = 0;
	    for (i=0; i<LrfscDrvrCLIENT_CONTEXTS; i++) {
	       ccon = &(wa->ClientContexts[i]);
	       if ((ccon->InUse) && (ccon->Pid == ccnbuf->Pid)) {
		  for (j=0; j<Wa->Modules; j++) {
		     mcon = &(Wa->ModuleContexts[j]);
		     if (mcon->Clients[i]) {
			ccnbuf->Connections[size].Module    = j+1;
			ccnbuf->Connections[size].Interrupt = mcon->Clients[i];
			ccnbuf->Connections[size].Pulse     = ccon->Pulse;
			ccnbuf->Size = ++size;
		     }
		  }
	       }
	    }
	    return OK;
	 }
      break;

      case LrfscDrvrSET_STATE:                        /* LrfscDrvrState Get Module state */
	 if (lap) {
	    sap = &(mmap->State);
	    sav = (unsigned short) *lap;
	    if (sav < LrfscDrvrSTATES) {
	       *sap = sav;
	       mcon->State = (LrfscDrvrState) sav;
	       return OK;
	    }
	 }
      break;

      case LrfscDrvrGET_STATE:
	 if (lap) {
	    sap = &(mmap->State);
	    sav = *sap;
	    if (sav >= LrfscDrvrSTATES) {
	       *sap = (unsigned short ) LrfscDrvrStateCONFIG;
	       mcon->State = LrfscDrvrStateCONFIG;
	    }
	    *lap = (unsigned long) sav;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_DIAG_CHOICE:          /* Configure diagnostics channels */
	 if (rcnt >= sizeof(LrfscDrvrDiagChoices)) {
	    diagch = (LrfscDrvrDiagChoices *) arg;
	    for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++) {
	       sigch = (*diagch)[i];
	       mcon->SignalChoices[i] = sigch;
	       SetSignalChoice(mcon,i);
	    }
	    return OK;
	 }
      break;

      case LrfscDrvrGET_DIAG_CHOICE:
	 if (wcnt >= sizeof(LrfscDrvrDiagChoices)) {
	    diagch = (LrfscDrvrDiagChoices *) arg;
	    for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++) {
	       sigch = mcon->SignalChoices[i];
	       (*diagch)[i] = sigch;
	    }
	    return OK;
	 }
      break;

      case LrfscDrvrSET_RES_CTRL:                     /* LrfscDrvrResCtrl Set Resonance control value */
	 if (rcnt >= sizeof(LrfscDrvrResCtrl)) {
	    resctl = (LrfscDrvrResCtrl *) arg;
	    mcon->ResCtrl = *resctl;
	    mmap->ResCtrl.Time  = (unsigned short) mcon->ResCtrl.Time;
	    mmap->ResCtrl.Value = (unsigned short) mcon->ResCtrl.Value;
	    return OK;
	 }
	 break;

      case LrfscDrvrGET_RES_CTRL:
	 if (wcnt >= sizeof(LrfscDrvrResCtrl)) {
	    resctl = (LrfscDrvrResCtrl *) arg;
	    *resctl = mcon->ResCtrl;
	    resctl->Cav.I = (unsigned short) mmap->ResCtrl.Cav.I;
	    resctl->Cav.Q = (unsigned short) mmap->ResCtrl.Cav.Q;
	    resctl->Fwd.I = (unsigned short) mmap->ResCtrl.Fwd.I;
	    resctl->Fwd.Q = (unsigned short) mmap->ResCtrl.Fwd.Q;
	    return OK;
	 }
	 break;

      case LrfscDrvrSET_ANALOGUE_SWITCH:      /* LrfscDrvrAnalogSwitch */
	 if (rcnt >= sizeof(LrfscDrvrAnalogSwitch)) {
	    anlgsw = (LrfscDrvrAnalogSwitch *) arg;
	    mcon->SwitchCtrl = *anlgsw;
	    mmap->SwitchCtrl = (unsigned short) mcon->SwitchCtrl;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_ANALOGUE_SWITCH:
	 if (wcnt >= sizeof(LrfscDrvrAnalogSwitch)) {
	    anlgsw = (LrfscDrvrAnalogSwitch *) arg;
	    *anlgsw = mcon->SwitchCtrl;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_SOFT_SWITCH:          /* LrfscDrvrSoftSwitch */
	 if (rcnt >= sizeof(LrfscDrvrSoftSwitch)) {
	    softsw = (LrfscDrvrSoftSwitch *) arg;
	    mcon->SoftSwitch = *softsw;
	    mmap->SoftSwitch = (unsigned short) mcon->SoftSwitch;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_SOFT_SWITCH:
	 if (wcnt >= sizeof(LrfscDrvrSoftSwitch)) {
	    softsw = (LrfscDrvrSoftSwitch *) arg;
	    *softsw = mcon->SoftSwitch;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_COEFFICIENTS:         /* LrfscDrvrMatrixCoefficientsBuf */
	 if (rcnt >= sizeof(LrfscDrvrMatrixCoefficientsBuf)) {
	    coefbuf = (LrfscDrvrMatrixCoefficientsBuf *) arg;
	    matrx = coefbuf->Matrix;
	    mcon->Coefficients[matrx] = coefbuf->Coeficients;
	    return SetMatrixCoefficient(mcon, matrx);
	 }
      break;

      case LrfscDrvrGET_COEFFICIENTS:
	 if (wcnt >= sizeof(LrfscDrvrMatrixCoefficientsBuf)) {
	    coefbuf = (LrfscDrvrMatrixCoefficientsBuf *) arg;
	    matrx = coefbuf->Matrix;
	    coefbuf->Coeficients = mcon->Coefficients[matrx];
	    return OK;
	 }
      break;

      case LrfscDrvrSET_SNAP_SHOT_TIME:       /* ulong Snapshot time */
	 if (lap) {
	    mcon->DiagTime = lav;
	    mmap->SnapShot.DiagTime = (unsigned short) mcon->DiagTime;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_SNAP_SHOTS:           /* LrfscDrvrDiagSnapShot */
	 if (wcnt >= sizeof(LrfscDrvrDiagSnapShot)) {
	    snapsht = (LrfscDrvrDiagSnapShot *) arg;
	    snapsht->DiagTime       = mmap->SnapShot.DiagTime;
	    snapsht->RefDiag.I      = mmap->SnapShot.RefDiag.I;
	    snapsht->RefDiag.Q      = mmap->SnapShot.RefDiag.Q;
	    snapsht->FwdDiag.I      = mmap->SnapShot.FwdDiag.I;
	    snapsht->FwdDiag.Q      = mmap->SnapShot.FwdDiag.Q;
	    snapsht->CavDiag.I      = mmap->SnapShot.CavDiag.I;
	    snapsht->CavDiag.Q      = mmap->SnapShot.CavDiag.Q;
	    snapsht->ErrDiag.I      = mmap->SnapShot.ErrDiag.I;
	    snapsht->ErrDiag.Q      = mmap->SnapShot.ErrDiag.Q;
	    snapsht->OutDiag.I      = mmap->SnapShot.OutDiag.I;
	    snapsht->OutDiag.Q      = mmap->SnapShot.OutDiag.Q;
	    snapsht->POutDiag.I     = mmap->SnapShot.POutDiag.I;
	    snapsht->POutDiag.Q     = mmap->SnapShot.POutDiag.Q;
	    snapsht->IOutDiag.I     = mmap->SnapShot.IOutDiag.I;
	    snapsht->IOutDiag.Q     = mmap->SnapShot.IOutDiag.Q;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_PIC:                          /* LrfscDrvrPicSetBuf PI Controller */
	 if (rcnt >= sizeof(LrfscDrvrPicSetBuf)) {
	    picbuff = (LrfscDrvrPicSetBuf *) arg;
	    mcon->Pic.KI = picbuff->KI;
	    mcon->Pic.KP = picbuff->KP;
	    mmap->Pic.KI = (short) mcon->Pic.KI;
	    mmap->Pic.KP = (short) mcon->Pic.KP;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_PIC:
	 if (wcnt >= sizeof(LrfscDrvrPicSetBuf)) {
	    picbuff = (LrfscDrvrPicSetBuf *) arg;
	    picbuff->KI = mcon->Pic.KI;
	    picbuff->KP = mcon->Pic.KP;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_PULSE_LENGTH:         /* ulong RF Pulse length */
	 if (lap) {
	    *lap = (unsigned short) mmap->RfOffTime;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_MAX_PULSE_LENGTH: /* ulong maximum RF pulse length */
	 if (lap) {
	    mcon->RfOnMaxLen = lav;
	    mmap->RfOnMaxLen = (unsigned short) mcon->RfOnMaxLen;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_MAX_PULSE_LENGTH: /* ulong maximum RF pulse length */
	 if (lap) {
	    *lap = (unsigned long) mcon->RfOnMaxLen;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_NEXT_CYCLE:           /* ulong Next cycle 1..32 */
	 if (lap) {
	    mmap->NextCycle = (unsigned short) lav;
	    return OK;
	 }
      break;

      case LrfscDrvrGET_PRES_CYCLE:
	 if (lap) {
	    sav = mmap->PresCycle;
	    *lap = sav;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_CYCLE_CONFIG:                         /* LrfscDrvrConfigBuf Set a cycle configuration */
	 if (rcnt >= sizeof(LrfscDrvrConfigBuf)) {
	    confbuf = (LrfscDrvrConfigBuf *) arg;
	    err = ConfToMcon(mcon, confbuf);
	    if (mcon->State == LrfscDrvrStateCONFIG) {
	       SetConfiguration(mcon, confbuf->Which, confbuf->Cycle);
	       mcon->ValidConfigs[confbuf->Which][confbuf->Cycle] = 0;
	    }
	    return err;
	 }
      break;

      case LrfscDrvrGET_CYCLE_CONFIG:                         /* LrfscDrvrConfigBuf Get a cycle configuration */
	 if (rcnt >= sizeof(LrfscDrvrConfigBuf)) {
	    confbuf = (LrfscDrvrConfigBuf *) arg;
	    return MconToConf(mcon, confbuf);
	    return OK;
	 }
      break;

      case LrfscDrvrSET_SKIP_COUNT:           /* ulong Set diagnostic skip count */
	 if (lap) {
	    if (lav > 0) {
	       mcon->SkipCount = lav;
	       return OK;
	    }
	 }
      break;

      case LrfscDrvrSET_SKIP_START:           /* ulong Set diagnostic skip count */
	 if (lap) {
	    mcon->SkipStart = lav;
	    return OK;
	 }
      break;

      case LrfscDrvrSET_PULSE:                /* LrfscDrvrPulse Set acquisition pulses */
	 if (lap) {
	    if ((lav > 0) && (lav <= LrfscDrvrPULSES)) {
	       mcon->Pulse = lav;
	       return OK;
	    }
	 }
      break;

      case LrfscDrvrGET_DIAGNOSTICS:          /* LrfscDrvrDiagBuf Get a diagnostic ram */
	 if (wcnt >= sizeof(LrfscDrvrDiagBuf)) {
	    diagbuf = (LrfscDrvrDiagBuf *) arg;
	    diagbuf->SkipStart = mcon->SkipStart;
	    diagbuf->SkipCount = mcon->SkipCount;
	    sigch = diagbuf->Choice;
	    diagbuf->Valid = mcon->AqnDone;

	    ram = (LrfscDrvrIQPair *) mcon->Address.RamAddress;
	    for (si=0; si<LrfscDrvrDiagSIGNAL_CHOICES; si++) {
	       sigc = (LrfscDrvrDiagSignalChoice) (unsigned short) mmap->SignalChoices[si];
	       if (sigch == sigc) {
		  mmap->RamSelect = (unsigned short) (LrfscDrvrRamSelection) si + LrfscDrvrRamDIAG1;
		  skpstr = mcon->SkipStart;
		  skpcnt = mcon->SkipCount;
		  if (skpcnt == 0)
		     skpcnt = mmap->RfOffTime / LrfscDrvrBUF_IQ_ENTRIES;

		  for (i=skpstr, j=0; j<LrfscDrvrBUF_IQ_ENTRIES; i+=skpcnt, j++) {
		     dst = &((*diagbuf).Array[j]);
		     src = &(ram[i]);
		     dst->I = src->I;
		     dst->Q = src->Q;
		  }
	       }
	    }
	    mcon->AqnDone = 0;
	    return OK;
	 }
      break;

      case  LrfscDrvrGET_VERSION:          /* LrfscDrvrVersion Get driver and VHDL versions */
	 if (wcnt >= sizeof(LrfscDrvrVersion)) {
	    verbuf = (LrfscDrvrVersion *) arg;
	    sav = mmap->VhdlVerH;
	    verbuf->Firmware = sav << 16;
	    sav = mmap->VhdlVerL;
	    verbuf->Firmware |= sav;
	    verbuf->Driver = COMPILE_TIME;
	    return OK;
	 }
      break;

      default:
      break;
   }

   /***********************************/
   /* End of switch                   */
   /***********************************/

   pseterr(EINVAL);       /* Invalid argument */
   return(SYSERR);
}

/***************************************************************************/
/* READ                                                                    */
/***************************************************************************/

int LrfscDrvrRead(wa, flp, u_buf, cnt)
LrfscDrvrWorkingArea *wa;
struct file *flp;
char *u_buf;
int cnt; {

LrfscDrvrClientContext *ccon;    /* Client context */
LrfscDrvrClientQueue   *queue;
LrfscDrvrConnection    *rb;
int                    cnum;    /* Client number */
int                    wcnt;    /* Writable byte counts at arg address */
int                    ps;
unsigned int           iq;

   if (u_buf != NULL) {
      wcnt = wbounds((int)u_buf);           /* Number of writable bytes without error */
      if (wcnt < sizeof(LrfscDrvrConnection)) {
	 pseterr(EINVAL);
	 cprintf("LrfscDrvr READ: First Exit\n");
	 return 0;
      }
   }

   cnum = minor(flp->dev) -1;
   ccon = &(wa->ClientContexts[cnum]);

   queue = &(ccon->Queue);
   if (queue->QOff) {
      disable(ps);
      {
	 queue->Size   = 0;
	 queue->Tail   = 0;
	 queue->Head   = 0;
	 queue->Missed = 0;         /* ToDo: What to do with this info ? */
	 sreset(&(ccon->Semaphore));
      }
      restore(ps);
   }

   if (swait(&(ccon->Semaphore), SEM_SIGABORT)) {

      /* EINTR = "Interrupted system call" */

      pseterr(EINTR);   /* We have been signaled */
      return 0;
   }

   rb = (LrfscDrvrConnection *) u_buf;

   if (queue->Size) {
      disable(ps);
      {
	 iq = queue->Tail;
	 *rb = queue->Queue[iq++];
	 if (iq < LrfscDrvrCLIENT_QUEUE_SIZE) queue->Tail = iq;
	 else                                 queue->Tail = 0;
	 queue->Size--;
      }
      restore(ps);
      return sizeof(LrfscDrvrConnection);
   }

   pseterr(EINTR);
   return 0;
}

/***************************************************************************/
/* WRITE                                                                   */
/***************************************************************************/

int LrfscDrvrWrite(wa, flp, u_buf, cnt)
LrfscDrvrWorkingArea * wa;       /* Working area */
struct file * flp;              /* File pointer */
char * u_buf;                   /* Users buffer */
int cnt; {                      /* Byte count in buffer */

   pseterr(EPERM);              /* Not supported */
   return 0;
}

/***************************************************************************/
/* SELECT                                                                  */
/***************************************************************************/

int LrfscDrvrSelect(wa, flp, wch, ffs)
LrfscDrvrWorkingArea * wa;      /* Working area */
struct file * flp;              /* File pointer */
int wch;                        /* Read/Write direction */
struct sel * ffs; {             /* Selection structurs */

LrfscDrvrClientContext * ccon;
int cnum;

   cnum = minor(flp->dev) -1;
   ccon = &(wa->ClientContexts[cnum]);

   if (wch == SREAD) {
      ffs->iosem = (int *) &(ccon->Semaphore); /* Watch out here I hope   */
      return OK;                               /* the system dosn't swait */
   }                                           /* the read does it too !! */

   pseterr(EACCES);         /* Permission denied */
   return SYSERR;
}

/*************************************************************/
/* Dynamic loading information for driver install routine.   */
/*************************************************************/

struct dldd entry_points = {
   LrfscDrvrOpen,
   LrfscDrvrClose,
   LrfscDrvrRead,
   LrfscDrvrWrite,
   LrfscDrvrSelect,
   LrfscDrvrIoctl,
   LrfscDrvrInstall,
   LrfscDrvrUninstall,
};
