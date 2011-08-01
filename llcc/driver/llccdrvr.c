/***************************************************************************/
/* Llcc Device driver.                                                     */
/* Feb 2003                                                                */
/* Ioan Kozsar                                                             */
/***************************************************************************/

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

#include <llccdrvr.h>
#include <llccdrvrP.h>

/* references */
extern  char *sysbrk ();
extern  int  timeout ();

#if defined(__powerpc__)

/* references specifics to CES PowerPC Cpus RIO806x for VME memory and VME interrupts mapping */

#include <ces/vmelib.h>
extern unsigned long find_controller   _AP((unsigned long vmeaddr, unsigned long len, unsigned long am, unsigned long offset, unsigned long size, struct pdparam_master  *param));
extern unsigned long return_controller _AP((unsigned long physaddr, unsigned long len));
extern int vme_intset                  _AP((int vct, int (*handler)(),char *arg, long *sav));
extern int vme_intclr                  _AP((int vct, long *sav));
extern void disable_intr();
extern void enable_intr();

/* Flash the i/o pipe line */
#define EIEIO asm("eieio")

#ifndef SYNC
#define SYNC  asm("sync")
#endif

#else
#define EIEIO
#define SYNC
#endif


/* Forward references */

#if defined(__powerpc__) && !defined(CETIA_PPC)

static int RawIo(LlccDrvrModuleContext *mcon,
		 LlccDrvrRawIoBlock    *riob,
		 unsigned long          flag,
		 unsigned long          debug);
static void Request(LlccDrvrModuleContext *mcon, long val);
static void Saturation(LlccDrvrModuleContext *mcon, long val);
//static unsigned long GetInterrupts(LlccDrvrModuleContext *mcon);
static void SetSwitchCtrl(LlccDrvrModuleContext *mcon,
			  unsigned long          flag,
			  unsigned long          wch);
static unsigned short GetSwitchCtrl(LlccDrvrModuleContext *mcon,
			            unsigned long          wch);
static void SetSoftSwitchCtrl(LlccDrvrModuleContext *mcon,
			  unsigned long          flag,
			  unsigned long          wch);
static unsigned short GetSoftSwitchCtrl(LlccDrvrModuleContext *mcon,
			            unsigned long          wch);
static void SetRamSelect(LlccDrvrModuleContext *mcon,
			  unsigned long          wch);
static unsigned short GetRamSelect(LlccDrvrModuleContext *mcon);
static void SetDiagSelect(LlccDrvrModuleContext *mcon,
                          unsigned long          wch,
                          unsigned short          of);
static unsigned short GetDiagSelect(LlccDrvrModuleContext *mcon,
				    unsigned long          of);

static unsigned short ResControl(LlccDrvrModuleContext *mcon);
static void SetMatrix(LlccDrvrModuleContext *mcon,
		      long           val,
		      unsigned long          wch,
		      unsigned long          coef);
static signed short GetMatrix(LlccDrvrModuleContext *mcon,
			      unsigned long          wch,
			      unsigned long          coef);
static void SetDiagTime(LlccDrvrModuleContext *mcon,
						long          nb);
static unsigned short GetDiagTime(LlccDrvrModuleContext *mcon);
static void SetPointsRam(LlccDrvrModuleContext *mcon,
						long          *setPt);
static signed short GetDiag(LlccDrvrModuleContext *mcon,
								unsigned long 		  wch);
static void SetPI(LlccDrvrModuleContext *mcon,
					long		val,
					unsigned long		wch);
static signed short GetPI(LlccDrvrModuleContext *mcon,
								unsigned long 		  wch);
static unsigned short GetRfOffTime(LlccDrvrModuleContext *mcon);
//static void SetKlysGainSP(LlccDrvrModuleContext *mcon,
//			  unsigned short         val);
//static unsigned short GetKlysGainSP(LlccDrvrModuleContext *mcon);
static int PingModule(LlccDrvrModuleContext *mcon);
static int AddModule(LlccDrvrModuleContext *mcon,
		     int                    index);
static void Reset(LlccDrvrModuleContext *mcon,
		  unsigned long          flag);
static int DisConnect(LlccDrvrModuleContext *mcon,
		      LlccDrvrClientContext *ccon);
static int Connect(LlccDrvrClientContext *ccon,
		   LlccDrvrModuleContext    *mcon);
static void SetIrqEnable(LlccDrvrModuleContext *mcon, long flag);
static unsigned short GetVector(LlccDrvrModuleContext *mcon);
static unsigned short GetStatus(LlccDrvrModuleContext *mcon);
static void ResetInterrupters(LlccDrvrModuleContext *mcon);

#else
static int                         RawIo();
static void						   Request();
static void						   Saturation();
//static unsigned long			   GetInterrupts();
static void                        SetSwitchCtrl();
static unsigned short              GetSwitchCtrl();
static void                        SetSoftSwitchCtrl();
static unsigned short              GetSoftSwitchCtrl();
static void                        SetRamSelect();
static unsigned short 		   	   GetRamSelect();
static void                        SetDiagSelect();
static unsigned short              GetDiagSelect();
static unsigned short              ResControl();
static void                        SetMatrix();
static signed short                GetMatrix();
static void                        SetDiagTime();
static unsigned short 		   	   GetDiagTime();
static void                        SetPointsRam();
static unsigned short			   GetDiag();
static void						   SetPI();
static unsigned short			   GetPI();
static unsigned short			   GetRfOffTime();
//static void                        SetKlysGainSP();
//static unsigned short              GetKlysGainSP();
static int                         PingModule();
static int                         AddModule();
static void                        Reset();
static int                         DisConnect();
static int                         Connect();
static void 					   SetIrqEnable();
static unsigned short              GetVector();
static unsigned short              GetStatus();
static void			   ResetInterrupters();

#endif


/* Standard driver interfaces */

void  IntrHandler();
char *LlccDrvrInstall();
int   LlccDrvrUninstall();
int   LlccDrvrOpen();
int   LlccDrvrClose();
int   LlccDrvrIoctl();
int   LlccDrvrRead();
int   LlccDrvrWrite();
int   LlccDrvrSelect();
/***************************************************************************/
/* Declare a static working area global pointer.                           */
/***************************************************************************/

static LlccDrvrWorkingArea *Wa = NULL;

/*========================================================================*/
/* Raw IO                                                                 */
/*========================================================================*/

static int RawIo(mcon,riob,flag,debg)
LlccDrvrModuleContext *mcon;
LlccDrvrRawIoBlock    *riob;
unsigned long          flag;
unsigned long          debg; {

volatile LlccDrvrModuleAddress *moad; /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap; /* Module Memory map */
int                             rval; /* Return value */
int                             ps; /* Processor status word */
int                             i, j;
unsigned long                  *uary;

   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   uary = riob->UserArray;
   rval = OK;

   if (!recoset()) {         /* Catch bus errors */

      for (i=0; i<riob->Size; i++) {
	 j = riob->Offset+i;
	 if (flag) {
	    ((unsigned long *) mmap)[j] = uary[i];
	    if (debg >= 2) cprintf("RawIo: Write: %d:0x%x\n",j,uary[i]);
	 } else {
	    uary[i] = ((unsigned long *) mmap)[j];
	    if (debg >= 2) cprintf("RawIo: Read: %d:0x%x\n",j,uary[i]);
	 }
	 EIEIO;
	 SYNC;
      }
   } else {
      disable(ps);
      kkprintf("LlccDrvr: BUS-ERROR: Module:%d. VME Addr:%x Vect:%x Level:%x\n",
	       mcon->ModuleIndex+1,
	       moad->VMEAddress,
	       moad->InterruptVector,
	       moad->InterruptLevel);

      /* mcon->BusError = 1; */

      pseterr(ENXIO);        /* No such device or address */
      rval = SYSERR;
      restore(ps);
   }
   noreco();                 /* Remove local bus trap */
   riob->Size = i+1;
   return rval;
}

/*========================================================================*/
/* Request                                                     */
/*========================================================================*/

static void Request(mcon, val)
LlccDrvrModuleContext *mcon;
long		           val; {

volatile LlccDrvrMemoryMap	  	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile LlccDrvrIrq         	  * irq;     /* Module IRQ	   */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   irq   = &(mmap->Irq);
   //if (mcon->ToggleRequest == 0) {
   if (val > 0) {
   		irq->Control = (irq->Status & 0x01) | 0x04;
   		mcon->ToggleRequest = 1;
   		//cprintf("LlccDrvr :: REQUEST 1\n");
   }
   else {
   		irq->Control = (irq->Status & 0x01) & ~0x04;
   		mcon->ToggleRequest = 0;
   		//cprintf("LlccDrvr :: REQUEST 0\n");
   }
   return;
}

/*========================================================================*/
/* Saturation                                                     */
/*========================================================================*/

static void Saturation(mcon, val)
LlccDrvrModuleContext *mcon;
long		           val; {

volatile LlccDrvrMemoryMap	  	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile LlccDrvrIrq         	  * irq;     /* Module IRQ	   */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   irq   = &(mmap->Irq);
   //if (mcon->ToggleRequest == 0) {
   if (val > 0) {
   		irq->Control = (irq->Status & 0x01) | 0x08;
   		//mcon->ToggleRequest = 1;
   		//cprintf("LlccDrvr :: SATURATION 1\n");
   }
   else {
   		irq->Control = (irq->Status & 0x01) & ~0x08;
   		//mcon->ToggleRequest = 0;
   		//cprintf("LlccDrvr :: SATURATION 0\n");
   }
   return;
}

/*========================================================================*/
/* Set Switch Control                                                     */
/*========================================================================*/

static void SetSwitchCtrl(mcon, flag, wch)
LlccDrvrModuleContext *mcon;
unsigned long          flag;
unsigned long          wch; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
     switch (wch) {
   
   	case 0:
	  if (flag>0) {
	    mmap->SwitchCtrl |= LlccDrvrOUT_SWITCH;
	  } else {
	    mmap->SwitchCtrl &= ~LlccDrvrOUT_SWITCH;
	  }
	break;
	
	case 1:
	  if (flag>0) {
	    mmap->SwitchCtrl |= LlccDrvrREF_SWITCH;
	  } else {
	    mmap->SwitchCtrl &= ~LlccDrvrREF_SWITCH;
	  }
	break;
	
	case 2:
	  if (flag>0) {
	    mmap->SwitchCtrl |= LlccDrvrFWD_SWITCH;
	  } else {
	    mmap->SwitchCtrl &= ~LlccDrvrFWD_SWITCH;
	  }
	break;
	
	case 3:
	  if (flag>0) {
	    mmap->SwitchCtrl |= LlccDrvrCAV_SWITCH;
	  } else {
	    mmap->SwitchCtrl &= ~LlccDrvrCAV_SWITCH;
	  }
	break;
	
	default:
	break;
     }
   return;
}

/*========================================================================*/
/* Get Switch Control                                                     */
/*========================================================================*/

static unsigned short GetSwitchCtrl(mcon, wch)
LlccDrvrModuleContext *mcon;
unsigned long          wch; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
unsigned short			    swi;


   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
     switch (wch) {
   
   	case 0:
	  swi = mmap->SwitchCtrl & LlccDrvrOUT_SWITCH;
	  /* swi = swi >> 0; */
	break;
	
	case 1:
	  swi = mmap->SwitchCtrl & LlccDrvrREF_SWITCH;
	  /* swi = swi >> 1; */
	break;
	
	case 2:
	  swi = mmap->SwitchCtrl & LlccDrvrFWD_SWITCH;
	  /* swi = swi >> 2; */
	break;
	
	case 3:
	  swi = mmap->SwitchCtrl & LlccDrvrCAV_SWITCH;
	  /* swi = swi >> 3; */
	break;
	
	default:
	break;
     }
   return swi;
}

/*========================================================================*/
/* Set Software Switch Control                                                     */
/*========================================================================*/

static void SetSoftSwitchCtrl(mcon, flag, wch)
LlccDrvrModuleContext *mcon;
unsigned long          flag;
unsigned long          wch; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
     switch (wch) {
   
   	case 0:
	  if (flag>0) {
	    mmap->SoftSwitch |= LlccDrvrSOFT_MAINLOOP_SWITCH;
	  } else {
	    mmap->SoftSwitch &= ~LlccDrvrSOFT_MAINLOOP_SWITCH;
	  }
	break;
	
	default:
	break;
     }
   return;
}

/*========================================================================*/
/* Get Software Switch Control                                                     */
/*========================================================================*/

static unsigned short GetSoftSwitchCtrl(mcon, wch)
LlccDrvrModuleContext *mcon;
unsigned long          wch; {

volatile LlccDrvrMemoryMap	  * mmap;	 	/* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	/* Module Address */
unsigned short			    swi;


   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
     switch (wch) {
   
   	case 0:
	  swi = mmap->SoftSwitch & LlccDrvrSOFT_MAINLOOP_SWITCH;
	  /* swi = swi >> 0; */
	break;
	
	default:
	break;
     }
   return swi;
}


/*========================================================================*/
/* Set Ram Select                                                          */
/*========================================================================*/

static void SetRamSelect(mcon, wch)
LlccDrvrModuleContext *mcon;
unsigned long          wch; {

volatile LlccDrvrMemoryMap	  * mmap;	 	/* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	/* Module Address */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
     switch (wch) {
   
   	case 0:
	    mmap->RamSelect = LlccDrvrSETPOINTS;
	break;
	
	case 1:
	    mmap->RamSelect = LlccDrvrFEEDFORVARD;
	break;
	
	case 2:
	    mmap->RamSelect = LlccDrvrDIAG1;
	break;
	
	case 3:
	    mmap->RamSelect = LlccDrvrDIAG2;
	break;
	
	case 4:
	    mmap->RamSelect = LlccDrvrDIAG3;
	break;
	
	case 5:
	    mmap->RamSelect = LlccDrvrDIAG4;
	break;
	
	default:
	break;
     }
   return;
}

/*========================================================================*/
/* Get Ram Select                                                         */
/*========================================================================*/

static unsigned short GetRamSelect(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
unsigned short                  ram;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   ram = mmap->RamSelect; 

   return ram;
}

/*========================================================================*/
/* Set Diag Select                                                          */
/*========================================================================*/

static void SetDiagSelect(mcon, wch, of)
LlccDrvrModuleContext *mcon;
unsigned long          wch;
unsigned short          of; {

volatile LlccDrvrMemoryMap		* mmap;	/* Memory map     */
volatile LlccDrvrModuleAddress	* moad;	/* Module Address */
volatile unsigned short			* diag;	/* Pointer to Diag block */
    
   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   diag = &(mmap->Diag1Select);
   diag = diag + of;
   
     switch (wch) {
   
   	case 0:
	    *diag = LlccDrvrREFLECTED;
	break;
	
	case 1:
	    *diag = LlccDrvrFORWARD;
	break;
	
	case 2:
	    *diag = LlccDrvrCAVITY;
	break;
	
	case 3:
	    *diag = LlccDrvrCAVEERROR;
	break;
	
	case 4:
	    *diag = LlccDrvrOUTPUT;
	break;
	
	default:
	break;
     }
   return;
}

/*========================================================================*/
/* Get Diag Select                                                        */
/*========================================================================*/

static unsigned short GetDiagSelect(mcon, of)
LlccDrvrModuleContext *mcon;
unsigned long          of; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile unsigned short		  * diag;


   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   diag = &(mmap->Diag1Select) + of;
   
   return *diag;
     
}

/*========================================================================*/
/* Resonance Control                                                             */
/*========================================================================*/

static unsigned short ResControl(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */

unsigned short          res;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   res   = mmap->ResCtrl;
   /* res   = res & 0x0fff */

   return res;
}

/*========================================================================*/
/* Set Matrix                                                             */
/*========================================================================*/

static void SetMatrix(mcon, val, wch, coef)
LlccDrvrModuleContext *mcon;
long		           val;
unsigned long          wch;
unsigned long          coef; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile signed short             * mat;        /* Pointer to Diag block */
    
   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   mat  = &(mmap->RefMatrixA);
   //cprintf("LlccDrvr :: 1 Address cav matrix= %d\n", mat);
   mat  = mat + wch + coef;
   *mat = (short)val;
   //cprintf("LlccDrvr :: 2 Address cav matrix= %d\n", mat);
   //cprintf("LlccDrvr :: Address cav matrix= %d\n", wch);
   //cprintf("LlccDrvr :: Address cav matrix= %d\n", coef);
   return;
}

/*========================================================================*/
/* Get Matrix                                                             */
/*========================================================================*/

static signed short GetMatrix(mcon, wch, coef)
LlccDrvrModuleContext *mcon;
unsigned long          wch;
unsigned long          coef; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile signed short             * mat;        /* Pointer to Matrix Coef */
    
   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   mat  = &(mmap->RefMatrixA);
   mat  = mat + wch + coef;
   
   return *mat;
}

/*========================================================================*/
/* Set DiagTime                                                          */
/*========================================================================*/

static void SetDiagTime(mcon, nb)
LlccDrvrModuleContext *mcon;
long                     nb; {

volatile LlccDrvrMemoryMap	  * mmap;	 	/* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	/* Module Address */
    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
   mmap->DiagTime = (unsigned short)nb;
   return;
}

/*========================================================================*/
/* Get DiagTime                                                          */
/*========================================================================*/

static unsigned short GetDiagTime(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
unsigned short                  dTime;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   dTime = mmap->DiagTime; 

   return dTime;
}

/*========================================================================*/
/* Set Points into Ram                                                    */
/*========================================================================*/

static void SetPointsRam(mcon, setPt)
LlccDrvrModuleContext *mcon;
long                  *setPt; {

//volatile LlccDrvrMemoryMap	  * mmap;	 	/* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	/* Module Address */
short * mram;
int * mvme;
//char * s;
int i;
unsigned short stat;

   //Save the status of the card
   stat = GetStatus(mcon);
   //Disable the interrupts regardless of the status
   SetIrqEnable(mcon, (long)0);
   
   //Put the points into the Ram
   moad  = &(mcon->Address);
   mram  = (short *) moad->RamAddress;
   mvme  = moad->VMEAddress;
   //mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   
   //cprintf("LlccDrvr :: SetPointsRam mram %x\n", mram);
   //cprintf("LlccDrvr :: SetPointsRam mvme %x\n", mvme);
   //cprintf("LlccDrvr :: SetPointsRam start pointer %x\n", setPt);
   //cprintf("LlccDrvr :: SetPointsRam First Point %x\n", *setPt);
   for (i = 0; i < 20000; i++) {
   		//*(mram+2*i)= (short)*(setPt+i);
   		*(mram+2*i)= (short)(*(setPt+i) >> 16);
   		*(mram+2*i+1)= (short)(*(setPt+i) & 0x0000ffff);
   		//just for testing purposes
   		//*(mram + 2*i) = (short)1;
   		//*(mram + 2*i +1) = (short)0;
   }
   
   //If the status was on put it back
   if ((stat & 0x1) == 1) {
   		SetIrqEnable(mcon, (long)1);
   }
   
   return;
}

/*========================================================================*/
/* Get Diag                                                               */
/*========================================================================*/

static signed short GetDiag(mcon, wch)
LlccDrvrModuleContext *mcon;
unsigned long          wch; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile unsigned short        *diag;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   diag = &(mmap->RefDiagI); 
   diag = diag + wch;
   return *diag;
}

/*========================================================================*/
/* Set PI controller                                                          */
/*========================================================================*/

static void SetPI(mcon, val, wch)
LlccDrvrModuleContext *mcon;
long         val;
unsigned long          wch; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile unsigned short        *pi;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   pi = &(mmap->KP); 
   pi = pi + wch;
   *pi = (short)val;
   return;
}

/*========================================================================*/
/* Get PI controller                                                          */
/*========================================================================*/

static signed short GetPI(mcon, wch)
LlccDrvrModuleContext *mcon;
unsigned long          wch; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile signed short        *pi;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   pi = &(mmap->KP); 
   pi = pi + wch;
   return *pi;
}

/*========================================================================*/
/* Get RfOffTime                                                          */
/*========================================================================*/

static unsigned short GetRfOffTime(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
unsigned short                  rfTime;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   rfTime = mmap->RfOffTime; 

   return rfTime;
}

/*========================================================================*/
/* Set Klystron Gain Set Point                                            */
/*========================================================================*/

//static void SetKlysGainSP(mcon, val)
//LlccDrvrModuleContext *mcon;
//unsigned short          val; {
//
//volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
//volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
//volatile unsigned short           * klys;
//    
//   moad  = &(mcon->Address);
//   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
//   klys  = &(mmap->KlysGainSP);
//   *klys = val; 
//   
//   return;
//}

/*========================================================================*/
/* Get Klystron Gain Set Point                                            */
/*========================================================================*/

//static unsigned short GetKlysGainSP(mcon)
//LlccDrvrModuleContext *mcon; {
//
//volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
//volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
//volatile unsigned short           klys;
//
//   moad  = &(mcon->Address);
//   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
//   klys = mmap->KlysGainSP; 
//
//   return klys;
//}

/*========================================================================*/
/* Reset Interrupters                                                     */
/*========================================================================*/

static void ResetInterrupters(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrMemoryMap	  * mmap;	 /* Memory map     */
volatile LlccDrvrModuleAddress	  * moad;	 /* Module Address */
volatile LlccDrvrIrq		  * irq;	 /* Module IRQ */
unsigned short                      vec;

    
   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   irq   = &(mmap->Irq);
   vec   = irq->Vec;
   cprintf("LlccDrvr :: RESET interrupters 1   %d\n", vec);
   irq->Control = 0x2 | (irq->Status & 0x1);
   irq->Vec = vec;
   cprintf("LlccDrvr :: RESET interrupters 2   %d\n", vec);
   cprintf("LlccDrvr :: RESET interrupters\n");
   return;
}

/*========================================================================*/
/* Get vector                                                             */
/*========================================================================*/

static unsigned short GetVector(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile LlccDrvrIrq           *irq;         /* Module IRQ */

unsigned short          vec;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   irq   = &(mmap->Irq);
   vec = irq->Vec; 

   return vec;
}

/*========================================================================*/
/* Get status                                                             */
/*========================================================================*/

static unsigned short GetStatus(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile LlccDrvrIrq           *irq;         /* Module IRQ */

unsigned short          stat;

   moad  = &(mcon->Address);
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   irq   = &(mmap->Irq);
   stat = irq->Status;
   /*stat &= 0x7;*/

   return stat;
}

/*========================================================================*/
/* Test an address on a module for a bus error                            */
/*========================================================================*/

static int PingModule(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrModuleAddress *moad; /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap; /* Module Memory map */
unsigned short                  test; /* Test value read back */
int                             rval; /* Return value */
int                             ps;   /* Processor status word */
/*****************************************************************************/
   //cprintf("ping1\n");
/*****************************************************************************/
   moad = &(mcon->Address);
   mmap = (LlccDrvrMemoryMap *) moad->VMEAddress;
   rval = OK;
   if (!recoset()) {         /* Catch bus errors */

      /* Test first and last module addresses */
/*****************************************************************************/
   //cprintf("ping2\n");
/*****************************************************************************/
      test = ((unsigned short *) mmap)[1];
      EIEIO;
      SYNC;

   } else {
/*****************************************************************************/
   cprintf("ping3\n");
/*****************************************************************************/
      disable(ps);
      kkprintf("LlccDrvrPING: BUS-ERROR: VME Addr:%x Vect:%d \n",
	       moad->VMEAddress,
	       moad->InterruptVector);
	       /* moad->InterruptLevel); */

     /*  mcon->BusError = 1; */

      pseterr(ENXIO);        /* No such device or address */
      rval = SYSERR;
      enable(ps); 
   }
   noreco();                 /* Remove local bus trap */
/*****************************************************************************/
   //cprintf("ping4\n");
/*****************************************************************************/
   return rval;

}

/*========================================================================*/
/* Reset the module                                                       */
/*========================================================================*/

static void Reset(mcon,flag)
LlccDrvrModuleContext *mcon;
unsigned long          flag; {
return;
}


/*========================================================================*/
/* Add a module to the driver, called per module from install             */
/*========================================================================*/

static int AddModule(mcon,index)
LlccDrvrModuleContext *mcon;        /* Module Context */
int index; {                        /* Module index (0-based) */

volatile LlccDrvrModuleAddress *moad;        /* Module address, vector, level, ssch */
volatile LlccDrvrMemoryMap     *mmap;        /* Module Memory map */
volatile LlccDrvrIrq           *irq;         /* Interrupt block */
unsigned long                  addr;         /* VME base address */
unsigned long                  coco;         /* Completion code */

#if defined(__powerpc__) && !defined(CETIA_PPC)

struct pdparam_master param; void *vmeaddr; void *ramaddr;/* For CES PowerPC */

#endif

   /* Compute Virtual memory address as seen from system memory mapping */

   moad = &(mcon->Address);

#if defined(__powerpc__) && !defined(CETIA_PPC)

   
   /* CES: build an address window (64 kbyte) for VME A24-D16 accesses */

   addr = (unsigned long) moad->VMEAddress;
   addr &= 0x00ffffff;                      /* A24 */
   
   bzero((char *)&param, sizeof(struct pdparam_master)); /* RAZ des champs inutiles*/
   
   param.iack   = 1;     /* no iack */
   param.rdpref = 0;     /* no VME read prefetch option */
   param.wrpost = 0;     /* no VME write posting option */
   param.swap   = 1;     /* VME auto swap option */
   param.dum[0] = 0;     /* window is sharable */
   vmeaddr = (void *) find_controller(addr,    /* Vme base address */
				     (unsigned long) 0x10000,  /* Module address space */
				     (unsigned long) 0x39,     /* Address modifier A24 */
				     0,                        /* Offset */
				     2,                        /* Size is D16 */
				     &param);                  /* Parameter block */
   if (vmeaddr == (void *)(-1)) {
      cprintf("LlccDrvr: find_controller: ERROR: Module:%d. VME Addr:%x\n",
	       index+1,
	       moad->VMEAddress);

      pseterr(ENXIO);        /* No such device or address */
      return(SYSERR);
   }
   moad->VMEAddress = (int *) vmeaddr;
   
   /* Write a zero into the RAMSELECT register */
   mmap  = (LlccDrvrMemoryMap *) moad->VMEAddress;
   mmap->RamSelect = 0x0;
   
   
   
   
   /* Get the Ram  Address */
   ramaddr = (void *) find_controller((addr + 0x80000),    /* Vme base address */
				     (unsigned long) 0x80000,    /* Module address space */
				     (unsigned long) 0x39,     /* Address modifier A24 */
				     0,                        /* Offset */
				     2,                        /* Size is D16 */
				     &param);                  /* Parameter block */
   if (ramaddr == (void *)(-1)) {
      cprintf("LlccDrvr: find_controller: ERROR: Module:%d. RAM Addr:%x\n",
	       index+1,
	       moad->VMEAddress);

      pseterr(ENXIO);        /* No such device or address */
      return(SYSERR);
   }
   moad->RamAddress = (int *) ramaddr;
   
   
#else

   /* 68K or non-CES PowerPC */

   addr = (unsigned long) moad->VMEAddress;
   addr &= 0x00ffffff;                      /* A24 */
   moad->VMEAddress = (unsigned long *) (addr | VMEBASE);

#endif

   /* Now write Interrupt Vector and Level to module */

   mmap = (LlccDrvrMemoryMap *) (moad->VMEAddress);
   irq = &(mmap->Irq);

   irq->Control = 0x2;                      /* Disable all interrupts */
   cprintf("TESTING: LlccDrvr: RESET module \n");	/* Reset the module */
   irq->Vec    = moad->InterruptVector;    /* Set the interrupt Vector */
   cprintf("TESTING: LlccDrvr: VECTOR: %x\n", irq->Vec);
   
#if defined(__powerpc__) && !defined(CETIA_PPC)

   coco = vme_intset((moad->InterruptVector),          /* Vector */
		     (void *)IntrHandler,              /* Address of ISR */
		     (char *)mcon,                     /* Parameter to pass */
		     0);                               /* Don't save previous */
   if (coco < 0) {
      cprintf("LlccDrvr: vme_intset: ERROR %d, MODULE %d\n",coco,index+1);
      pseterr(EFAULT);                                 /* Bad address */
      return SYSERR;
   }

#else

   /* 68k or CETIA PPC */

   coco = iointset((char *) ((moad->InterruptVector) << 2),
		   IntrHandler,                        /* ISR address */
		   mcon);                              /* ISR gets module context as a parameter */
   if (coco < 0) {
      cprintf("LlccDrvr: iointset: ERROR %d, MODULE %d\n",coco,index+1);
      pseterr(EFAULT);                                 /* Bad address */
      return SYSERR;
   }
#endif

#ifdef CETIA_PPC

   /* Enable VME chip to let through interrupts */

   vmeintcontrol(1<<moad->InterruptLevel, 1);

#endif

/************************************************************/   
kkprintf("LlccDrvr(AddModule): Done adding new module\n");
/************************************************************/

   /* Reset the module, check for bus errors */

   coco = PingModule(mcon);       /* Check module */
   if (coco != OK) return coco;   /* Bus Error, exit */
   Reset(mcon,0);                 /* Reset */
   return OK;
}

/*========================================================================*/
/* Disconnect                                                             */
/*========================================================================*/

static int DisConnect(mcon, ccon)
LlccDrvrModuleContext *mcon;
LlccDrvrClientContext *ccon; {

long oi, i, ps;
unsigned long ci;

   ci = ccon->ClientIndex;

   if (ccon->InUse == 1) {

      /* Look for the given object ID and disconnect this client */

      oi = -1;                                    /* Object ID not found */
      for (i=0; i<LlccDrvrINTERRUPT_SOURCES; i++) {
	 if (mcon->Clients[i][ci] != NULL) {
	    oi = i;                               /* Save the cleared object index */
	    mcon->Clients[i][ci] = NULL;          /* Disconnect the client */
	 }
      }

      /* If there are still other connected clients do not proceed to */
      /* disable the hardware, just return. */

      if (oi != -1) {
	 for (ci=0; ci<LlccDrvrCLIENT_CONTEXTS; ci++) {
	    if (mcon->Clients[oi][ci]) {
	    	cprintf("Other clients still connected");
	       return OK;                         /* Other clients still connected */
	    }
	 }
      } else {

	 /* Client supplied a Junk object ID, or an ID he is */
	 /* not connected to. */
	 /* ENXIO = "No such device or address" */

	 pseterr(ENXIO); /* No such Object */
	 return SYSERR;
      }

      /* This is the last connected client to the hardware interrupt */
      /* so now we must disable the hardware. */
      restore(ps);

      return OK;
   }

   /* ENXIO = "No such device or address" */

   pseterr(ENXIO); /* No such Object ID = Zero */
   return SYSERR;
}


/*========================================================================*/
/* Connect                                                                */
/*========================================================================*/

static int Connect(ccon, mcon)
LlccDrvrClientContext *ccon;
LlccDrvrModuleContext *mcon; {

long i, ps;
unsigned long ci;

   if (ccon == NULL) {

      /* EINVAL = "Invalid argument" */

      pseterr(EINVAL);                           /* Caller error */
      return SYSERR;
   }


   ci = ccon->ClientIndex;
   mcon->NbInterrupts = 0;
   mcon->ToggleRequest = 0;


      

      /* Connect to device interrupts: Enable if not already */

   for (i=0; i<LlccDrvrINTERRUPT_SOURCES; i++) {
	 disable(ps);
	 {
	    mcon->Clients[i][ci] = ccon;         /* Connect client */
	 }
	 restore(ps);
	 return OK;
   }
 
   /* Client supplied a Junk object ID = zero */
   /* ENXIO = "No such device or address" */

   pseterr(ENXIO); /* No such Object */
   return SYSERR;
}

/*========================================================================*/
/* SetIrqEnable															  */
/*========================================================================*/
static void SetIrqEnable(mcon, flag)
LlccDrvrModuleContext *mcon;
long	flag;
{
	volatile LlccDrvrMemoryMap *mmap;
	mmap = (LlccDrvrMemoryMap *) mcon->Address.VMEAddress;
    mmap->Irq.Control = (short)((flag & 0x01) | (mmap->Irq.Control & 0xfc));
    //cprintf("LLCC drv::SetIrqEnable = %d\n", flag);
}

/***************************************************************************/
/* INTERRUPT HANDLER                                                       */
/***************************************************************************/

void IntrHandler(mcon)
LlccDrvrModuleContext *mcon; {

volatile LlccDrvrMemoryMap *mmap;
/* volatile LlccDrvrIrq       *irq; */

/* unsigned long              src; */
LlccDrvrClientContext      *ccon;
LlccDrvrClientQueue        *queue;
LlccDrvrReadBuf		   		rb;
int                        	i;
unsigned int               	iq,ci;

/***********************************************************************/	
	//kkprintf("LlccDrvr: InterruptHandler: INTERRUPT\n");
	mcon->NbInterrupts ++;
/***********************************************************************/

   mmap =  (LlccDrvrMemoryMap *) mcon->Address.VMEAddress;
   /* src = mmap->Src; */
   EIEIO;

   /* mmap  = (LlccDrvrMemoryMap *) irq; */

      /* Julian 25/08/05 Need to know which module interrupted */

      rb.Connection.Module = mcon->ModuleIndex + 1;

      for (i=0; i < LlccDrvrINTERRUPT_SOURCES; i++) {
	    for (ci=0; ci < LlccDrvrCLIENT_CONTEXTS; ci++) {
	       ccon = mcon->Clients[i][ci];
	       if (ccon != NULL) {
		   	queue = &(ccon->Queue); 
		  	if (queue->Size < LlccDrvrCLIENT_QUEUE_SIZE) {
		     queue->Size++;
		     iq = queue->Head;
		     queue->Queue[iq++] = rb; 
		     ssignal(&(ccon->Semaphore));
		     if (iq < LlccDrvrCLIENT_QUEUE_SIZE) queue->Head = iq;
		     else                                queue->Head = 0;
		  	} else {
		     queue->Missed++;
		  	} 
	       }
	    }
      }
}

/***************************************************************************/
/* INSTALL                                                                 */
/***************************************************************************/

char *LlccDrvrInstall(info)
LlccDrvrInfoTable *info; {         /* Driver info table */

int				i, count;
LlccDrvrModuleContext		*mcon;       /* Module context */
volatile LlccDrvrModuleAddress	*moad;       /* Modules address */

LlccDrvrWorkingArea *wa;

/****************************************************************************************/	
	kkprintf("LlccDrvr: START of INSTALL entry point\n");
/****************************************************************************************/

	/*Allocate the driver working area*/
	
	wa = (LlccDrvrWorkingArea *) sysbrk(sizeof(LlccDrvrWorkingArea));
	if (!wa) {
		cprintf("LlccDrvrInstall: NOT ENOUGH MEMORY(WorkingArea)\n");
		pseterr(ENOMEM);          /* Not enough core */
		return((char *) SYSERR);
	}
	Wa = wa;                     /* Global working area pointer */
	
	
	/****************************************/
	/* Initialize the driver's working area */
	/* and add the modules ISR into LynxOS  */
	/****************************************/

   bzero((void *) wa,sizeof(LlccDrvrWorkingArea));          /* Clear working area */


   count = 0;
   for (i=0; i<info->Modules; i++) {
      mcon = &(wa->ModuleContexts[i]);
      moad = &mcon->Address;
      *moad = info->Addresses[i];
      //cprintf("TESTING: LlccDrvr: VME Addr: %x\n", moad->VMEAddress);
      //cprintf("TESTING: LlccDrvr: RAM Addr: %x\n", moad->RamAddress);
      if (AddModule(mcon,count) != OK) {
	 cprintf("LlccDrvr: Module: %d Not Installed\n", i+1);
	 bzero((void *) moad,sizeof(LlccDrvrModuleAddress));     /* Wipe it out */
      } else {
	 count++;

	 /* Julian 25/08/2005 Need to return module from ISR */

	 mcon->ModuleIndex = count -1;

	 cprintf("LlccDrvr: Module %d. VME Addr: %x RAM Addr: %x Vect: %x Level: %x Installed OK\n",
		 count,
		 moad->VMEAddress,
		 moad->RamAddress,
		 moad->InterruptVector,
		 moad->InterruptLevel);
      }

   }
   wa->Modules = count;
   //ResetInterrupters(mcon);

   cprintf("LlccDrvr: Installed: %d LLCC Modules in Total\n",count);
   return((char*) wa);

/****************************************************************************************/	
	kkprintf("LlccDrvr: END of INSTALL entry point\n");
/****************************************************************************************/

}

/***************************************************************************/
/* UNINSTALL                                                               */
/***************************************************************************/

int LlccDrvrUninstall(wa)
LlccDrvrWorkingArea * wa; {     /* Drivers working area pointer */

   cprintf("LlccDrvr: ERROR: UnInstall capabilities are not supported\n");

   /* EPERM = "Operation not permitted" */

   pseterr(EPERM);             /* Not supported */
   return SYSERR;

}

/***************************************************************************/
/* OPEN                                                                    */
/***************************************************************************/

int LlccDrvrOpen(wa,dnm,flp)
LlccDrvrWorkingArea *wa;
int dnm;
struct file *flp; {

int cnum;                          /* Client number */
LlccDrvrClientContext * ccon;    /* Client context */

   /* We allow one client per minor device, we use the minor device */
   /* number as an index into the client contexts array. */

   cnum = minor(flp->dev) -1;
   if ((cnum < 0) || (cnum >= LlccDrvrCLIENT_CONTEXTS)) {

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

   bzero((void *) ccon,(long) sizeof(LlccDrvrClientContext));
   ccon->ClientIndex = cnum;
   /* ccon->Timeout     = LlccDrvrDEFAULT_TIMEOUT; */
   ccon->InUse       = 1;
   ccon->Pid         = getpid();
   return OK;
}

/***************************************************************************/
/* CLOSE                                                                   */
/***************************************************************************/

int LlccDrvrClose(wa, flp)
LlccDrvrWorkingArea * wa;          /* Working area */
struct file * flp; {               /* File pointer */

int cnum;                          /* Client number */
int i, j;

LlccDrvrModuleContext     * mcon;   /* Module context */
LlccDrvrClientContext     * ccon;   /* Client context */

   /* We allow one client per minor device, we use the minor device */
   /* number as an index into the client contexts array.            */

   cnum = minor(flp->dev) -1;
   if ((cnum < 0) || (cnum >= LlccDrvrCLIENT_CONTEXTS)) {

      /* EFAULT = "Bad address" */

      pseterr(EFAULT);
      return SYSERR;
   }
   ccon = &(Wa->ClientContexts[cnum]);

   /* Cancel any pending timeouts */


   /* Disconnect this client from events */

   for (i=0; i<LlccDrvrMODULE_CONTEXTS; i++) {
      for (j=0; j<LlccDrvrINTERRUPT_SOURCES; j++) {
	 mcon = &(Wa->ModuleContexts[i]);
	 if (mcon->Clients[j][cnum]) {
	   DisConnect(mcon, ccon);
	 }
      }
   }

   /* Wipe out the client context */

   bzero((void *) ccon,sizeof(LlccDrvrClientContext));
   return(OK);
}

/***************************************************************************/
/* IOCTL                                                                   */
/***************************************************************************/

int LlccDrvrIoctl(wa, flp, cm, arg)
LlccDrvrWorkingArea *wa;
struct file *flp;
LlccDrvrControlFunction cm;
char *arg; {

int cnum;               /* Client number */
int rcnt, wcnt;         /* Readable, Writable byte counts at arg address */
long lav, *lap;         /* Long Value pointed to by Arg */
/* int i, j; */

LlccDrvrClientContext     * ccon;        /* Client context */
LlccDrvrModuleContext     * mcon;        /* Module context */
LlccDrvrRawIoBlock        * riob;


   /* Check argument contains a valid address for reading or writing. */
   /* We can not allow bus errors to occur inside the driver due to   */
   /* the caller providing a garbage address in "arg". So if arg is   */
   /* not null set "rcnt" and "wcnt" to contain the byte counts which */
   /* can be read or written to without error. */

   if (arg != NULL) {
      rcnt = rbounds((int)arg);       /* Number of readable bytes without error */
      wcnt = wbounds((int)arg);       /* Number of writable bytes without error */
      if (rcnt < sizeof(long)) {      /* We at least need to read one long */
	 cprintf("LlccDrvrIoctl: ILLEGAL NON NULL ARG POINTER, RCNT=%d/%d\n",
		 rcnt,sizeof(long));
	 pseterr(EINVAL);        /* Invalid argument */
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
   if ((cnum < 0) || (cnum >= LlccDrvrCLIENT_CONTEXTS)) {
      pseterr(ENODEV);          /* No such device */
      return SYSERR;
   }

   /* We can't control a file which is not open. */

   ccon = &(wa->ClientContexts[cnum]);
   if (ccon->InUse == 0) {
      cprintf("LlccDrvrIoctl: DEVICE %2d IS NOT OPEN\n",cnum+1);
      pseterr(EBADF);           /* Bad file number */
      return SYSERR;
   }

   mcon = &(wa->ModuleContexts[ccon->ModuleIndex]); /* Default module selected */
   

   /*************************************/
   /* Decode callers command and do it. */
   /*************************************/

   switch (cm) {
      
      case LlccDrvrSET_DEBUG:            /* Set driver debug mode */
	 if (lav) {
	    ccon->DebugOn = 1;
	    cprintf("Debug Printing Enabled\n");
	 } else {
	    ccon->DebugOn = 0;
	    cprintf("Debug Printing Disabled\n");
	 }
	 return OK;
      
      case LlccDrvrSET_MODULE:              /* Select the module to work with */
	 if ((lav >= 1)
	 &&  (lav <= Wa->Modules)) {
	    ccon->ModuleIndex = lav -1;
	    return OK;
	 }
      break;
      
      case LlccDrvrGET_MODULE:
	 if (ccon->DebugOn) cprintf("Llcc-DEBUG: Ioctl GET_MODULE: OK\n");
	 *((int *) arg) = ccon->ModuleIndex +1;
	 return OK;
      
      case LlccDrvrCONNECT:
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 /* if (rcnt >= sizeof(LlccDrvrConnection)) { */
	    /* conx = (LlccDrvrConnection *) arg; */
	    return Connect(ccon, mcon);
	 /* } */
      break;
      
      case LlccDrvrDISCONNECT:
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 /* if (rcnt >= sizeof(LlccDrvrConnection)) { */
	    /* conx = (LlccDrvrConnection *) arg; */
	    return DisConnect(mcon, ccon);
	 /* } */
      break;
      
      case LlccDrvrGET_IRQ_VECTOR:                       /* Read module vector */
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	    cprintf("Vector of module : %x \n", GetVector(mcon));
      break;
      
      case LlccDrvrSET_IRQ_ENABLE:                       /* Read module vector */
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("llcc drv:: LlccDrvrSET_IRQ_ENABLE = %d\n", lav);
	 if (lav>0) {
	    SetIrqEnable(mcon, 1);
	    return OK;
	 } else {
	    SetIrqEnable(mcon, 0);
	    return OK;
	 }
      break;
      
      case LlccDrvrGET_IRQ_ENABLE:                       /* Read module vector */
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	    //cprintf("Status of module : %x \n", GetStatus(mcon));
	    return (GetStatus(mcon) & 0x1);
      break;
      
      case LlccDrvrRESET_ALL:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 ResetInterrupters(mcon);
	 cprintf("Reset done\n");
      break;
      
      case LlccDrvrREQUEST:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 Request(mcon, lav);
	 //cprintf("Request done\n");
      break;
      
      case LlccDrvrSATURATION:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 Saturation(mcon, lav);
	 //cprintf("Request done\n");
      break;
      
      case LlccDrvrINTERRUPTS:                       /* Read module vector */
	 if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	    cprintf("Number of interrupts : %d \n", mcon->NbInterrupts);
      break;
      
      /* SWITCH CONTROL */
      
      case LlccDrvrSET_OUT_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetSwitchCtrl(mcon, lav, 0);
      break;
      
      case LlccDrvrGET_OUT_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("OUT switch : %x \n", GetSwitchCtrl(mcon, 0));
	 return GetSwitchCtrl(mcon, 0);
      break;
      
      case LlccDrvrSET_REF_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetSwitchCtrl(mcon, lav, 1);
      break;
      
      case LlccDrvrGET_REF_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF switch : %x \n", GetSwitchCtrl(mcon, 1));
	 return GetSwitchCtrl(mcon, 1);
      break;
      
      case LlccDrvrSET_FWD_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetSwitchCtrl(mcon, lav, 2);
      break;
      
      case LlccDrvrGET_FWD_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("FWD switch : %x \n", GetSwitchCtrl(mcon, 2));
	 return GetSwitchCtrl(mcon, 2);
      break;
      
      case LlccDrvrSET_CAV_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetSwitchCtrl(mcon, lav, 3);
      break;
      
      case LlccDrvrGET_CAV_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("CAV switch : %x \n", GetSwitchCtrl(mcon, 3));
	 return GetSwitchCtrl(mcon, 3);
      break;
      
      /* SOFT SWITCH CONTROL */
      
      case LlccDrvrSET_MAINLOOP_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetSoftSwitchCtrl(mcon, lav, 0);
      break;
      
      case LlccDrvrGET_MAINLOOP_SWITCH:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("MAIN LOOP switch : %x \n", GetSoftSwitchCtrl(mcon, 0));
	 return GetSoftSwitchCtrl(mcon, 0);
      break;
      
      
      /* RAM SELECT */
      
      case LlccDrvrSET_RAM_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetRamSelect(mcon, lav);
      break;
      
      case LlccDrvrGET_RAM_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("RAM select : %d \n", GetRamSelect(mcon));
      break;
      
      /* DIAG SELECT */
      
      case LlccDrvrSET_DIAG1_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetDiagSelect(mcon, lav, 0);
      break;
      
      case LlccDrvrGET_DIAG1_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("DIAG1 select : %d \n", GetDiagSelect(mcon, 0));
      break;
      
      case LlccDrvrSET_DIAG2_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetDiagSelect(mcon, lav, 1);
      break;
      
      case LlccDrvrGET_DIAG2_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("DIAG1 select : %d \n", GetDiagSelect(mcon, 2));
      break;
      
      case LlccDrvrSET_DIAG3_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetDiagSelect(mcon, lav, 2);
      break;
      
      case LlccDrvrGET_DIAG3_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("DIAG1 select : %d \n", GetDiagSelect(mcon, 4));
      break;
      
      case LlccDrvrSET_DIAG4_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetDiagSelect(mcon, lav, 3);
      break;
      
      case LlccDrvrGET_DIAG4_SELECT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("DIAG1 select : %d \n", GetDiagSelect(mcon, 6));
      break;
      
      /* RESONANCE CONTROL */
      
      case LlccDrvrGET_RES_CONTROL:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("RESCTRL VALUE : %d \n", ResControl(mcon));
      break;
      
      /**************/
      /* REF_MATRIX */
      /**************/
      
      case LlccDrvrSET_REF_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrREF_MATRIX, LlccDrvrA_MATRIX);
      break;
      
      case LlccDrvrGET_REF_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX A : %d \n", GetMatrix(mcon, LlccDrvrREF_MATRIX, LlccDrvrA_MATRIX));
      break;
      
      case LlccDrvrSET_REF_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrREF_MATRIX, LlccDrvrB_MATRIX);
      break;
      
      case LlccDrvrGET_REF_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX B : %d \n", GetMatrix(mcon, LlccDrvrREF_MATRIX, LlccDrvrB_MATRIX));
      break;
      
      case LlccDrvrSET_REF_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrREF_MATRIX, LlccDrvrC_MATRIX);
      break;
      
      case LlccDrvrGET_REF_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX C : %d \n", GetMatrix(mcon, LlccDrvrREF_MATRIX, LlccDrvrC_MATRIX));
      break;
      
      case LlccDrvrSET_REF_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrREF_MATRIX, LlccDrvrD_MATRIX);
      break;
      
      case LlccDrvrGET_REF_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX D : %d \n", GetMatrix(mcon, LlccDrvrREF_MATRIX, LlccDrvrD_MATRIX));
      break;
      
      /**************/
      /* FWD_MATRIX */
      /**************/
      
      case LlccDrvrSET_FWD_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrFWD_MATRIX, LlccDrvrA_MATRIX);
      break;
      
      case LlccDrvrGET_FWD_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX A : %d \n", GetMatrix(mcon, LlccDrvrFWD_MATRIX, LlccDrvrA_MATRIX));
      break;
      
      case LlccDrvrSET_FWD_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrFWD_MATRIX, LlccDrvrB_MATRIX);
      break;
      
      case LlccDrvrGET_FWD_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX B : %d \n", GetMatrix(mcon, LlccDrvrFWD_MATRIX, LlccDrvrB_MATRIX));
      break;
      
      case LlccDrvrSET_FWD_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrFWD_MATRIX, LlccDrvrC_MATRIX);
      break;
      
      case LlccDrvrGET_FWD_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX C : %d \n", GetMatrix(mcon, LlccDrvrFWD_MATRIX, LlccDrvrC_MATRIX));
      break;
      
      case LlccDrvrSET_FWD_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrFWD_MATRIX, LlccDrvrD_MATRIX);
      break;
      
      case LlccDrvrGET_FWD_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX D : %d \n", GetMatrix(mcon, LlccDrvrFWD_MATRIX, LlccDrvrD_MATRIX));
      break;
      
      /**************/
      /* CAV_MATRIX */
      /**************/
      
      case LlccDrvrSET_CAV_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrCAV_MATRIX, LlccDrvrA_MATRIX);
	 //cprintf("llcc drv::LlccDrvrSET_CAV_MATRIX_A  = %d\n", lav);
      break;
      
      case LlccDrvrGET_CAV_MATRIX_A:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX A : %d \n", GetMatrix(mcon, LlccDrvrCAV_MATRIX, LlccDrvrA_MATRIX));
      break;
      
      case LlccDrvrSET_CAV_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrCAV_MATRIX, LlccDrvrB_MATRIX);
      break;
      
      case LlccDrvrGET_CAV_MATRIX_B:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX B : %d \n", GetMatrix(mcon, LlccDrvrCAV_MATRIX, LlccDrvrB_MATRIX));
      break;
      
      case LlccDrvrSET_CAV_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrCAV_MATRIX, LlccDrvrC_MATRIX);
      break;
      
      case LlccDrvrGET_CAV_MATRIX_C:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX C : %d \n", GetMatrix(mcon, LlccDrvrCAV_MATRIX, LlccDrvrC_MATRIX));
      break;
      
      case LlccDrvrSET_CAV_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetMatrix(mcon, lav, LlccDrvrCAV_MATRIX, LlccDrvrD_MATRIX);
      break;
      
      case LlccDrvrGET_CAV_MATRIX_D:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("REF MATRIX D : %d \n", GetMatrix(mcon, LlccDrvrCAV_MATRIX, LlccDrvrD_MATRIX));
      break;
      
      /* DIAGTIME */
      
      case LlccDrvrSET_DIAGTIME:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetDiagTime(mcon, lav);
      break;
      
      case LlccDrvrGET_DIAGTIME:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("DiagTime : %d \n", GetDiagTime(mcon));
      break;
      
      /* SNAPSHOTS */
      
      case LlccDrvrGET_REF_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("RefDiagI : %d \n", GetDiag(mcon, LlccDrvrREF_I));
	 return GetDiag(mcon, LlccDrvrREF_I);
      break;
      
      case LlccDrvrGET_REF_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("RefDiagQ : %d \n", GetDiag(mcon, LlccDrvrREF_Q));
	 return GetDiag(mcon, LlccDrvrREF_Q);
      break;
      
      case LlccDrvrGET_FWD_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("FwdDiagI : %d \n", GetDiag(mcon, LlccDrvrFWD_I));
	 return GetDiag(mcon, LlccDrvrFWD_I);
      break;
      
      case LlccDrvrGET_FWD_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("FwdDiagQ : %d \n", GetDiag(mcon, LlccDrvrFWD_Q));
	 return GetDiag(mcon, LlccDrvrFWD_Q);
      break;
      
      case LlccDrvrGET_CAV_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("CavDiagI : %d \n", GetDiag(mcon, LlccDrvrCAV_I));
	 return GetDiag(mcon, LlccDrvrCAV_I);
      break;
      
      case LlccDrvrGET_CAV_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("CavDiagQ : %d \n", GetDiag(mcon, LlccDrvrCAV_Q));
	 return GetDiag(mcon, LlccDrvrCAV_Q);
      break;
      
      case LlccDrvrGET_ERR_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("ErrDiagI : %d \n", GetDiag(mcon, LlccDrvrERR_I));
	 return GetDiag(mcon, LlccDrvrERR_I);
      break;
      
      case LlccDrvrGET_ERR_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("ErrDiagQ : %d \n", GetDiag(mcon, LlccDrvrERR_Q));
	 return GetDiag(mcon, LlccDrvrERR_Q);
      break;
      
      case LlccDrvrGET_LRN_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("LrnDiagI : %d \n", GetDiag(mcon, LlccDrvrLRN_I));
	 return GetDiag(mcon, LlccDrvrLRN_I);
      break;
      
      case LlccDrvrGET_LRN_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("LrnDiagQ : %d \n", GetDiag(mcon, LlccDrvrLRN_Q));
	 return GetDiag(mcon, LlccDrvrLRN_Q);
      break;
      
      case LlccDrvrGET_OUT_DIAG_I:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("OutDiagI : %d \n", GetDiag(mcon, LlccDrvrOUT_I));
	 return GetDiag(mcon, LlccDrvrOUT_I);
      break;
      
      case LlccDrvrGET_OUT_DIAG_Q:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("OutDiagQ : %d \n", GetDiag(mcon, LlccDrvrOUT_Q));
	 return GetDiag(mcon, LlccDrvrOUT_Q);
      break;
      
      /* PI CONTROLLER */
      
      case LlccDrvrSET_KP:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetPI(mcon, lav, LlccDrvrKP);
      break;
      
      case LlccDrvrGET_KP:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("Proportional Gain: %d \n", GetPI(mcon, LlccDrvrKP));
      break;
      
      case LlccDrvrSET_KI:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 SetPI(mcon, lav, LlccDrvrKI);
      break;
      
      case LlccDrvrGET_KI:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("Integral Gain: %d \n", GetPI(mcon, LlccDrvrKI));
      break;
      
      case LlccDrvrGET_IP_OUT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("IP OUT: %d \n", GetPI(mcon, LlccDrvrIP_OUT));
	 return GetPI(mcon, LlccDrvrIP_OUT);
      break;
      
      case LlccDrvrGET_QP_OUT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("QP OUT: %d \n", GetPI(mcon, LlccDrvrQP_OUT));
	 return GetPI(mcon, LlccDrvrQP_OUT);
      break;
      
      case LlccDrvrGET_II_OUT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("II OUT: %d \n", GetPI(mcon, LlccDrvrII_OUT));
	 return GetPI(mcon, LlccDrvrII_OUT);
      break;
      
      case LlccDrvrGET_QI_OUT:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 //cprintf("QI OUT: %d \n", GetPI(mcon, LlccDrvrQI_OUT));
	 return GetPI(mcon, LlccDrvrQI_OUT);
      break;
      
      /* RFOFFTIME */
      
      case LlccDrvrGET_RFOFFTIME:
         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
	 cprintf("RfOffTime : %d \n", GetRfOffTime(mcon));
      break;
      
      /* KLYSTRON GAIN SET POINT */
      
//      case LlccDrvrSET_KLYSGAINSP:
//         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
//	 SetKlysGainSP(mcon, lav);
//      break;
//      
//      case LlccDrvrGET_KLYSGAINSP:
//         if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
//	 cprintf("KLYS GAIN set point : %d \n", GetKlysGainSP(mcon));
//      break;
      
      
      
      /* READ WRITE */
      
      case LlccDrvrRAW_READ:
	 if (wcnt >= sizeof(LlccDrvrRawIoBlock)) {
	    riob = (LlccDrvrRawIoBlock *) arg;
	    if ((riob->UserArray != NULL)
	    &&  (wcnt > riob->Size * sizeof(unsigned long))) {
	       return RawIo(mcon,riob,0,ccon->DebugOn);
	    }
	 }
      break;

      case LlccDrvrRAW_WRITE:
      if (PingModule(mcon) != OK)  return SYSERR; /* Bus Error */
      cprintf("LlccDrvr :: Argument to write %x\n", lap);
      SetPointsRam(mcon, lap);
//	 if (rcnt >= sizeof(LlccDrvrRawIoBlock)) {
//	    riob = (LlccDrvrRawIoBlock *) arg;
//	    if ((riob->UserArray != NULL)
//	    &&  (rcnt > riob->Size * sizeof(unsigned long))) {
//	       return RawIo(mcon,riob,1,ccon->DebugOn);
//	    }
//	 }
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

int LlccDrvrRead(wa, flp, u_buf, cnt)
LlccDrvrWorkingArea *wa;
struct file *flp;
char *u_buf;
int cnt; {


LlccDrvrClientContext *ccon;    /* Client context */
LlccDrvrClientQueue   *queue;
LlccDrvrReadBuf       *rb;
int                    cnum;    /* Client number */
int                    wcnt;    /* Writable byte counts at arg address */
int                    ps;
unsigned int           iq;

/**************************************************************************/	
	//cprintf("LlccDrvr READ: Start\n");
/**************************************************************************/
   if (u_buf != NULL) {
      wcnt = wbounds((int)u_buf);           /* Number of writable bytes without error */
      if (wcnt < sizeof(LlccDrvrReadBuf)) {
	 pseterr(EINVAL);
	 cprintf("LlccDrvr READ: First Exit\n");
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
/**************************************************************************/	
	//cprintf("LlccDrvr READ: Second Exit\n");
/**************************************************************************/
      return 0;
   }



   rb = (LlccDrvrReadBuf *) u_buf;

   if (queue->Size) {
      disable(ps);
      {
	 iq = queue->Tail;
	 *rb = queue->Queue[iq++];
	 if (iq < LlccDrvrCLIENT_QUEUE_SIZE) queue->Tail = iq;
	 else                                queue->Tail = 0;
	 queue->Size--;
      }
      restore(ps);
      //cprintf("LlccDrvr READ: Third Exit\n");
      return sizeof(LlccDrvrReadBuf);
   }

   pseterr(EINTR);
   return 0;
/**************************************************************************/	
	//cprintf("LlccDrvr READ: End\n");
/**************************************************************************/
}

/***************************************************************************/
/* WRITE                                                                   */
/***************************************************************************/

int LlccDrvrWrite(wa, flp, u_buf, cnt)
LlccDrvrWorkingArea * wa;       /* Working area */
struct file * flp;              /* File pointer */
char * u_buf;                   /* Users buffer */
int cnt; {                      /* Byte count in buffer */

   pseterr(EPERM);              /* Not supported */
   return 0;
}

/***************************************************************************/
/* SELECT                                                                  */
/***************************************************************************/

int LlccDrvrSelect(wa, flp, wch, ffs)
LlccDrvrWorkingArea * wa;       /* Working area */
struct file * flp;              /* File pointer */
int wch;                        /* Read/Write direction */
struct sel * ffs; {             /* Selection structurs */

LlccDrvrClientContext * ccon;
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
   LlccDrvrOpen,
   LlccDrvrClose,
   LlccDrvrRead,
   LlccDrvrWrite,
   LlccDrvrSelect,
   LlccDrvrIoctl,
   LlccDrvrInstall,
   LlccDrvrUninstall,
};
