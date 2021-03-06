/**
 * ======================================================================
 * Lowlevel RF module driver LRFC
 * First version: Julian Lewis Monday 5th May 2008
 *
 * This driver was originally written for LynxOS, I ported it to Linux in
 * late 2010. The device/module logical unit model has changed to one
 * node in /dev per module. Interrupt handling is now using event queues.
 * Client-Contexts have been abolished, a single node accomodates as many
 * clients as open the node. Macros to perform endian conversion added.
 * All floating point calculations removed.
 * Loads of other changes made to numerous to mention.
 *
 * Julian Lewis BE/CO/HT Version: Thu 2nd Dec 2010
 *
 * ======================================================================
 */

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <vmebus.h>

static char *major_name = "lrfsc";
static int lrfsc_major = 0;

MODULE_AUTHOR("Julian Lewis BE/CO/HT CERN");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Low Level RF cavity driver");
MODULE_SUPPORTED_DEVICE("LRFSC VME Module");

#include "lrfscdrvr.h"
#include "lrfscdrvrP.h"

#ifndef COMPILE_TIME
#define COMPILE_TIME 0
#endif

#define DEBUG 0
#define TIMEOUT 1000
#define MAX_BUF 2048

/*
 * ==============================
 * Module parameter storage area
 * Indexed by minor device number
 */

#define DRV_MAX_DEVICES LrfscDrvrMODULE_CONTEXTS

static long luns[DRV_MAX_DEVICES];	/* Logical unit numbers */
static long lvls[DRV_MAX_DEVICES];	/* Interrupt levels */
static long vecs[DRV_MAX_DEVICES];	/* Interrupt vectors */
static long vme1[DRV_MAX_DEVICES];	/* First VME base address */
static long vme2[DRV_MAX_DEVICES];	/* Second VME base address */

/* These parameter counts must equal the number of luns */
/* or be equal to zero if not used. */

static unsigned int luns_num;
static unsigned int lvls_num;
static unsigned int vecs_num;
static unsigned int vme1_num;
static unsigned int vme2_num;

module_param_array(luns, long, &luns_num, 0444);	/* Vector */
module_param_array(lvls, long, &lvls_num, 0444);	/* Vector */
module_param_array(vecs, long, &vecs_num, 0444);	/* Vector */
module_param_array(vme1, long, &vme1_num, 0444);	/* Vector */
module_param_array(vme2, long, &vme2_num, 0444);	/* Vector */

MODULE_PARM_DESC(luns, "Logical unit numbers");
MODULE_PARM_DESC(lvls, "Interrupt levels");
MODULE_PARM_DESC(vecs, "Interrupt vectors");
MODULE_PARM_DESC(vme1, "First map base addresses");
MODULE_PARM_DESC(vme2, "Second map base addresses");

/* ====================================================================== */
/* Declare a static working area global pointer.                          */

static LrfscDrvrWorkingArea *Wa = NULL;
struct file_operations lrfsc_fops;

/*
 * ====================================================================
 * Debug routines
 */

static char *ioctl_names[LrfscDrvrLAST_IOCTL_NUM] = {

	"Unknown IOCTL number",   /* Throw away zero */

	"RESET",                  /* NULL Reset the module and driver */
	"GET_STATUS",             /* LrfscDrvrStatus Get status */
	"SET_SW_DEBUG",           /* ulong Set software debug level */
	"GET_SW_DEBUG",           /* ulong Get software debug level */
	"RAW_READ",               /* LrfscDrvrRawIoBlock Read  */
	"RAW_WRITE",              /* LrfscDrvrRawIoBlock Write */
	"SET_TIMEOUT",            /* ulong Set read timeout */
	"GET_TIMEOUT",            /* ulong Get read timeout */
	"SET_STATE",              /* LrfscDrvrState Get Module state */
	"GET_STATE",
	"SET_DIAG_CHOICE",        /* LrfscDrvrDiagChoices Configure diagnostics channels */
	"GET_DIAG_CHOICE",
	"SET_RES_CTRL",           /* LrfscDrvrResCtrl Set Resonance control value */
	"GET_RES_CTRL",
	"SET_ANALOGUE_SWITCH",    /* LrfscDrvrAnalogSwitch */
	"GET_ANALOGUE_SWITCH",
	"SET_SOFT_SWITCH",        /* LrfscDrvrSoftSwitch */
	"GET_SOFT_SWITCH",
	"SET_COEFFICIENTS",       /* LrfscDrvrMatrixCoefficientsBuf */
	"GET_COEFFICIENTS",
	"SET_SNAP_SHOT_TIME",     /* ulong Snapshot time */
	"GET_SNAP_SHOTS",         /* LrfscDrvrDiagSnapShotBuf */
	"SET_PIC",                /* LrfscDrvrPicSetBuf PI Controller */
	"GET_PIC",
	"GET_PULSE_LENGTH",       /* ulong RF Pulse length */
	"SET_MAX_PULSE_LENGTH",   /* ulong maximum RF pulse length */
	"GET_MAX_PULSE_LENGTH",   /* ulong maximum RF pulse length */
	"SET_NEXT_CYCLE",         /* ulong Next cycle 1..32 */
	"GET_PRES_CYCLE",
	"SET_CYCLE_CONFIG",       /* LrfscDrvrConfigBuf Set a cycle configuration */
	"GET_CYCLE_CONFIG",       /* LrfscDrvrConfigBuf Get a cycle configuration */
	"SET_SKIP_COUNT",         /* ulong Set diagnostic skip count */
	"SET_SKIP_START",         /* ulong Set diagnostic skip count */
	"SET_PULSE",              /* LrfscDrvrPulse Set acquisition pulse */
	"GET_DIAGNOSTICS",        /* LrfscDrvrDiagBuf Get a diagnostic ram */
	"GET_VERSION"             /* LrfscDrvrVersion Get UTC version dates */
};

static void debug_ioctl(int ionr, int iodr, int iosz, void *arg, long num, int dlevel)
{
	int c;

	if (dlevel <= 0)
		return;

	printk("%s:debug_ioctl:ionr:%d", major_name, ionr);
	if (ionr <= LrfscDrvrFIRST_IOCTL_NUM || ionr >= LrfscDrvrLAST_IOCTL_NUM) {
		printk(" BAD:");
	} else {
		c = ionr - LrfscDrvrFIRST_IOCTL_NUM;
		printk(" %s:", ioctl_names[c]);
	}

	printk(" iodr:%d:", iodr);
	if (iodr & _IOC_WRITE)
		printk("WR:");
	if (iodr & _IOC_READ)
		printk("RD:");

	if (arg)
		c = *(int *) arg;
	else
		c = 0;
	printk(" iosz:%d arg:0x%p[%d] minor:%d\n", iosz, arg, c,
	       (int) num);
}

/* ================= */

int check_minor(long num)
{
	if (num < 0 || num >= DRV_MAX_DEVICES) {
		printk("%s:minor:%d ", major_name, (int) num);
		printk("BAD not in [0..%d]\n", DRV_MAX_DEVICES - 1);
		return 0;
	}
	return 1;
}

/*
 * =========================================================
 * VMEIO with bus error handling
 */

#define CLEAR_BUS_ERROR ((int) 1)
#define BUS_ERR_PRINT_THRESHOLD 10

static int bus_error_count = 0;	/* For all modules */
static int isr_bus_error = 0;	/* Bus error in an ISR */
static int last_bus_error = 0;	/* Last printed bus error */

/* ==================== */

static void bus_err_handler(struct vme_bus_error *error)
{
	bus_error_count++;
}

/* ==================== */

static int get_clr_bus_errs(void)
{
	int res;

	res = bus_error_count;

	bus_error_count = 0;
	last_bus_error = 0;
	isr_bus_error = 0;

	return res;
}

/* ==================== */

static void chk4buser(char *dw, char *dir, void *x)
{
	if (bus_error_count > last_bus_error &&
	    bus_error_count <= BUS_ERR_PRINT_THRESHOLD) {
		printk("%s:BUS_ERROR:%s:%s-Address:0x%p\n",
			       major_name, dw, dir, x);
		if (isr_bus_error)
			printk("%s:BUS_ERROR:In ISR occured\n",
				       major_name);
		if (bus_error_count == BUS_ERR_PRINT_THRESHOLD)
			printk("%s:BUS_ERROR:PrintSuppressed\n",
				       major_name);
		isr_bus_error = 0;
		last_bus_error = bus_error_count;
	}
}

/* ==================== */
/* Used in ISR only     */

static short IHRd16(unsigned short *x)
{
	short res;

	isr_bus_error = 0;
	res = ioread16be(x);
	if (bus_error_count > last_bus_error)
		isr_bus_error = 1;
	return res;
}

/* ==================== */

static void IHWr16(unsigned short *x, unsigned short v)
{
	isr_bus_error = 0;
	iowrite16be(v, x);
	if (bus_error_count > last_bus_error)
		isr_bus_error = 1;
	return;
}

/* ==================== */

static short HRd16(unsigned short *x)
{
	short res;

	res = ioread16be(x);
	chk4buser("D16", "READ", x);
	return res;
}

/* ==================== */

static void HWr16(unsigned short *x, unsigned short v)
{
	iowrite16be(v, x);
	chk4buser("D16", "WRITE", x);
	return;
}

/* ==================== */

void *map_window(int vme, int win, int amd, int dwd)
{

	void *vmeaddr = NULL;
	struct pdparam_master param;

	if (!(vme && amd && win && dwd)) return NULL;

	param.iack = 1;                 /* no iack */
	param.rdpref = 0;               /* no VME read prefetch option */
	param.wrpost = 0;               /* no VME write posting option */
	param.swap = 1;                 /* VME auto swap option */
	param.dum[0] = VME_PG_SHARED;   /* window is sharable */
	param.dum[1] = 0;               /* XPC ADP-type */
	param.dum[2] = 0;               /* window is sharable */

	vmeaddr = (void *) find_controller((long) vme,win,amd,0,dwd,&param);

	printk("%s:Window:", major_name);
	if (vmeaddr == (void *) (-1)) {
		printk("ERROR:NotMapped");
		vmeaddr = NULL;
	} else
		printk("OK:Mapped");

	printk(":Address:0x%X Window:0x%X AddrMod:0x%X DWidth:0x%X\n",vme,win,amd,dwd);
	return vmeaddr;
}

/* ==================== */

struct vme_berr_handler *set_berr_handler(int vme, int win, int amd)
{

	struct vme_bus_error berr;
	struct vme_berr_handler *handler;

	if (!(vme && amd && win)) return NULL;

	berr.address = (long) vme;
	berr.am      = amd;
	handler      = vme_register_berr_handler(&berr, win, bus_err_handler);

	printk("%s:bus_err_handler:", major_name);

	if (IS_ERR(handler)) {
	   printk("ERROR:NotRegistered");
	   handler = NULL;
	} else {
	   printk("OK:Registered");
	}
	printk(":Address:0x%X Window:0x%X AddrMod:0x%X\n",vme,win,amd);
	return handler;
}

/* ==================== */

void unregister_module(LrfscDrvrModuleContext *mcon)
{

unsigned short vec;

	vec = mcon->Address.InterruptVector;

	if (vec)
		vme_intclr(vec, NULL);
	if (mcon->map1)
		return_controller((unsigned) mcon->map1, 0x10000);
	if (mcon->map2)
		return_controller((unsigned) mcon->map2, 0x80000);
	if (mcon->ber1)
		vme_unregister_berr_handler(mcon->ber1);
	if (mcon->ber2)
		vme_unregister_berr_handler(mcon->ber2);
}

/**
 * ====================================
 * Set a configuration on a module
 * N.B. Used from within the ISR
 * ====================================
 */

static int set_config(LrfscDrvrModuleContext *mcon, LrfscDrvrConfig cnf, int cyc)
{

	LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */
	LrfscDrvrVector        *dst;
	LrfscDrvrVector        *src;

	int i;
	unsigned short old;

	mmap  = (LrfscDrvrMemoryMap *) mcon->map1;

	old = IHRd16(&mmap->RamSelect);
	IHWr16(&mmap->RamSelect, (unsigned short) cnf);

	src = &((*mcon).Vectors[(int) cnf][(int) cyc][0]);

	dst = (LrfscDrvrVector *)
	      ( (unsigned long) mcon->map2
	    |   (unsigned long) ((cyc & 31) << 14) );

	for (i=0; i<LrfscDrvrCONFIG_POINTS; i++) {
		IHWr16(&dst->Next             ,src->Next);
		IHWr16(&dst->Ticks            ,src->Ticks);
		IHWr16(&dst->IncI.Shorts.High ,src->IncI.Shorts.High);
		IHWr16(&dst->IncI.Shorts.Low  ,src->IncI.Shorts.Low);
		IHWr16(&dst->IncQ.Shorts.High ,src->IncQ.Shorts.High);
		IHWr16(&dst->IncQ.Shorts.Low  ,src->IncQ.Shorts.Low);
		if (dst->Next == 0) break;
		dst++; src++;
	}
	IHWr16(&mmap->RamSelect, old);

	return 0;
}

/**
 * ====================================
 */

void copy2hw(unsigned short *dst, unsigned short *src, int bytes)
{

	int i, wds;

	wds = bytes/sizeof(unsigned short);
	for (i=0; i<wds; i++)
		HWr16(&dst[i], src[i]);
}

/**
 * ====================================
 * Set coefficients on a module
 * ====================================
 */

static int set_mat_coefficient(LrfscDrvrModuleContext *mcon, LrfscDrvrMatrix mnm)
{

	LrfscDrvrMemoryMap          *mmap;        /* Module Memory map */
	LrfscDrvrMatrixCoefficients *mat;

	mmap = (LrfscDrvrMemoryMap *) mcon->map1;
	mat  = &(mcon->Coefficients[(int) mnm]);

	copy2hw((unsigned short *) &(mmap->Matrix[(int) mnm]),
		      (unsigned short *) mat,
		      sizeof(LrfscDrvrMatrixCoefficients));
	return 0;
}


/**
 * ====================================
 * Set signal choice on a module
 * ====================================
 */

static int set_sig_choice(LrfscDrvrModuleContext *mcon, int sgn)
{

	LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */

	mmap = (LrfscDrvrMemoryMap *) mcon->map1;
	HWr16(&mmap->SignalChoices[sgn], (unsigned short) mcon->SignalChoices[sgn]);
	return 0;
}

/**
 * ====================================
 * Reset the module to known state
 * ====================================
 */

static int reset_module(LrfscDrvrModuleContext *mcon)
{

	LrfscDrvrModuleAddress *moad;        /* Module address, vector, level */
	LrfscDrvrMemoryMap     *mmap;        /* Module Memory map */
	int                              i, j, cnt;

	cnt = get_clr_bus_errs();

	moad = &(mcon->Address);
	mmap  = (LrfscDrvrMemoryMap *) mcon->map1;

	HWr16(&mmap->State, (unsigned short) LrfscDrvrStateCONFIG);

	for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++)
		set_sig_choice(mcon, i);
	for (i=0; i<LrfscDrvrMatrixMATRICES; i++)
		set_mat_coefficient(mcon, (LrfscDrvrMatrix) i);
	for (i=0; i<LrfscDrvrCONFIGS; i++)
		HWr16(&mmap->RamSelect, (unsigned short) i);
	for (j=0; j<LrfscDrvrCYCLES; j++)
		set_config(mcon, (LrfscDrvrConfig) i, j);

	HWr16(&mmap->Control           , (unsigned short) mcon->Control);
	HWr16(&mmap->Vector            , (unsigned short) moad->InterruptVector);
	HWr16(&mmap->RamSelect         , (unsigned short) mcon->RamSelect);
	HWr16(&mmap->ResCtrl.Time      , (unsigned short) mcon->ResCtrl.Time);
	HWr16(&mmap->ResCtrl.Value     , (unsigned short) mcon->ResCtrl.Value);
	HWr16(&mmap->SwitchCtrl        , (unsigned short) mcon->SwitchCtrl);
	HWr16(&mmap->SoftSwitch        , (unsigned short) mcon->SoftSwitch);
	HWr16(&mmap->SnapShot.DiagTime , (unsigned short) mcon->DiagTime);
	HWr16(&mmap->Pic.KP            , (  signed short) mcon->Pic.KP);
	HWr16(&mmap->Pic.KI            , (  signed short) mcon->Pic.KI);
	HWr16(&mmap->NextCycle         , (unsigned short) 0);
	HWr16(&mmap->RfOnMaxLen        , (unsigned short) mcon->RfOnMaxLen);
	HWr16(&mmap->State             , (unsigned short) mcon->State);

	if (get_clr_bus_errs())
		return -EIO;
	return 0;
}

/**
 * =====================================================
 * Interrupt handler
 * =====================================================
 */

static irqreturn_t lrfsc_irq(void *arg)
{
	LrfscDrvrModuleContext    *mcon;
	LrfscDrvrMemoryMap        *mmap;
	unsigned int               i, j, si, vd, skpstr, skpcnt;
	unsigned short             isrc, pnum, cnum, cntrl;
	LrfscDrvrDiagSignalChoice  sigc;
	LrfscDrvrIQPair           *ram;
	LrfscDrvrIQPair           *src, *dst;

	mcon = (LrfscDrvrModuleContext *) arg;
	mmap =  (LrfscDrvrMemoryMap *) mcon->map1;
	ram  =  (LrfscDrvrIQPair    *) mcon->map2;

	isrc = IHRd16(&mmap->IrqSource);
	cnum = IHRd16(&mmap->PresCycle);
	pnum = IHRd16(&mmap->PulseNumber);

	mcon->isrc = isrc;
	mcon->cnum = cnum;
	mcon->pnum = pnum;

	mcon->icnt++;

	if (isrc & LrfscDrvrInterruptPULSE) {

		cntrl = IHRd16(&mmap->Control);
		cntrl |= LrfscDrvrControlDO_AQN;
		IHWr16(&mmap->Control, cntrl);

		for (si=0; si<LrfscDrvrDiagSIGNAL_CHOICES; si++) {

			sigc = (LrfscDrvrDiagSignalChoice) IHRd16(&mmap->SignalChoices[si]);

			skpstr = mcon->SkipStart;
			skpcnt = mcon->SkipCount;
			if (skpcnt == 0)
				skpcnt = IHRd16(&mmap->RfOffTime) / LrfscDrvrBUF_IQ_ENTRIES;
			if ((mcon->Pulse == pnum) && (mcon->State == LrfscDrvrStatePROD_REMOTE)) {

				IHWr16(&mmap->RamSelect, (unsigned short) (LrfscDrvrRamSelection) si + LrfscDrvrRamDIAG1);

				for (i=skpstr, j=0; j<LrfscDrvrBUF_IQ_ENTRIES; i+=skpcnt, j++) {
					dst = &((*mcon).Diags[sigc][cnum][j]);
					src = &(ram[i]);

					dst->I = IHRd16(&src->I);
					dst->Q = IHRd16(&src->Q);

					vd = 1;
				}
			} else
				vd = 0;

			mcon->ValidDiags[sigc][cnum] = vd;
		}

		for (i=0; i<LrfscDrvrCONFIGS; i++) {
			for (j=0; j<LrfscDrvrCYCLES; j++) {
				if (mcon->ValidVecs[i][j]) {
					mcon->ValidVecs[i][j] = 0;

					IHWr16(&mmap->State ,0);
					IHWr16(&mmap->RamSelect ,(unsigned short) i);
					set_config(mcon, (LrfscDrvrConfig) i, j);
					IHWr16(&mmap->State, (unsigned short) mcon->State);
				}
			}
		}
	}
	wake_up(&mcon->queue);
	return IRQ_HANDLED;
}

/*
 * =====================================================
 * Install
 * =====================================================
 */

int lrfsc_install(void)
{
	int i, cc, cy;

	LrfscDrvrWorkingArea   *wa;
	LrfscDrvrModuleContext *mcon;
	LrfscDrvrModuleAddress *moad;
	LrfscDrvrVector        *vec;  /* Vector */
	LrfscDrvrVectorArray   *va;   /* Vector array */

	wa = (LrfscDrvrWorkingArea *) kmalloc(sizeof(LrfscDrvrWorkingArea), GFP_KERNEL);
	if (!wa) {
		printk("%s:NOT ENOUGH MEMORY(WorkingArea)\n",
		       major_name);
		return -ENOMEM;
	}
	Wa = wa;
	memset(wa, 0, sizeof(LrfscDrvrWorkingArea));

	if (luns_num <= 0 || luns_num > DRV_MAX_DEVICES) {
		printk("%s:Fatal:No logical units defined.\n",
		       major_name);
		return -EACCES;
	}

	/* Vector parameters must all be the same size or zero */

	if (lvls_num != luns_num && lvls_num != 0) {
		printk("%s:Fatal:Missing interrupt level.\n",
		       major_name);
		return -EACCES;
	}

	if (vecs_num != luns_num && vecs_num != 0) {
		printk("%s:Fatal:Missing interrupt vector.\n",
		       major_name);
		return -EACCES;
	}

	if (vme1_num != luns_num && vme1_num != 0) {
		printk("%s:Fatal:Missing first base address.\n",
		       major_name);
		return -EACCES;
	}
	if (vme2_num != luns_num && vme2_num != 0) {
		printk("%s:Fatal:Missing second base address.\n",
		       major_name);
		return -EACCES;
	}

	/* Register driver */

	cc = register_chrdev(lrfsc_major, major_name, &lrfsc_fops);
	if (cc < 0) {
		printk("%s:Fatal:Error from register_chrdev [%d]\n",
		       major_name, cc);
		return cc;
	}
	if (lrfsc_major == 0)
		lrfsc_major = cc;       /* dynamic */

	/* Create VME mappings and register ISRs */

	for (i = 0; i < luns_num; i++) {

		mcon = &Wa->ModuleContexts[i];
		memset(mcon, 0, sizeof(LrfscDrvrModuleContext));
		moad = &mcon->Address;

		Wa->Modules = i +1;

		init_waitqueue_head(&mcon->queue);

		mcon->debug = DEBUG;
		mcon->timeout = msecs_to_jiffies(TIMEOUT);
		mcon->icnt = 0;

		printk("\n%s:Mapping:Logical unit:%d\n",
		     major_name, (int) luns[i]);

		mcon->map1 = map_window(vme1[i],0x10000,0x39,2);
		mcon->ber1 = set_berr_handler(vme1[i],0x10000,0x39);

		mcon->map2 = map_window(vme2[i],0x80000,0x39,2);
		mcon->ber2 = set_berr_handler(vme2[i],0x80000,0x39);

		moad->VMEAddress = mcon->map1;
		moad->RamAddress = mcon->map2;

		cc = vme_intset(vecs[i],
				(int (*)(void *)) lrfsc_irq,
				(void *) mcon,
				0);

		printk("%s:ISR:", major_name);

		if (cc < 0)
			printk("ERROR:NotRegistered");
		else
			printk("OK:Registered");

		printk(":Level:0x%X Vector:0x%X\n", (unsigned int) lvls[i], (unsigned int) vecs[i]);

		moad->InterruptVector = vecs[i];
		moad->InterruptLevel = lvls[i];


		mcon->ModuleIndex             = i;
		mcon->State                   = LrfscDrvrStateCONFIG;
		mcon->Control                 = LrfscDrvrControlNONE;
		mcon->RamSelect               = LrfscDrvrRamSETPOINTS;
		mcon->ResCtrl.Time            = 0xFF;
		mcon->SwitchCtrl              = 0;
		mcon->SoftSwitch              = LrfscDrvrSotfSwitchMAIN_CLOSED;
		mcon->DiagTime                = 0x2000;
		mcon->RfOnMaxLen              = 0x8000;
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

		for (cy=0; cy<LrfscDrvrCYCLES; cy++) {

			va              = &((*mcon).Vectors[LrfscDrvrConfigSETPOINTS][cy]);
			vec             = &((*va)[0]);
			vec->Next       = sizeof(LrfscDrvrVector)/2;
			vec->Ticks      = 1;
			vec->IncI.Long  = 4000 << 16;
			vec->IncQ.Long  = 0;

			vec             = &((*va)[1]);
			vec->Next       = 0;
			vec->Ticks      = 0xFFFF;
			vec->IncI.Long  = 0;
			vec->IncQ.Long  = 0;

			va              = &((*mcon).Vectors[LrfscDrvrConfigFEEDFORWARD][cy]);
			vec             = &((*va)[0]);
			vec->Next       = 0;
			vec->Ticks      = 0xFFFF;
			vec->IncI.Long  = 0;
			vec->IncQ.Long  = 0;
		}
		if (reset_module(mcon) < 0)
			return -EIO;
	}
	return 0;
}

/**
 * =====================================================
 * Raw IO
 * =====================================================
 */

#define READ_FLAG 0
#define WRITE_FLAG 1

static int RawIo(LrfscDrvrModuleContext *mcon,
		 LrfscDrvrRawIoBlock    *riob,
		 int rwflg) {

	unsigned short         *mmap; /* Module Memory map */
	unsigned short         *uary;
	int                    i, j, cnt;

	uary = riob->UserArray;

	if (riob->RamFlag)
		mmap = mcon->map2;
	else
		mmap = mcon->map1;

	cnt = get_clr_bus_errs();
	for (i=0; i<riob->Size; i++) {
		j = riob->Offset+i;
		if (rwflg)
			HWr16(&mmap[j],uary[i]);
		else
			uary[i] = HRd16(&mmap[j]);

		if (get_clr_bus_errs())
			return -EIO;
	}
	return riob->Size;
}

/**
 * =====================================================
 * Convert Time IQ value to Time IQ increment
 * Result is stored in the module context
 * =====================================================
 */

int iq2inc(LrfscDrvrModuleContext *mcon, LrfscDrvrConfigBuf *buf) {

	LrfscDrvrConfigArray *mcdst;        /* Module Context Configuration array */
	LrfscDrvrConfigPoint  pi, *p1, *p2; /* Initial, (n)th, (n+1)th */

	LrfscDrvrVector      *vec;          /* Vector */
	LrfscDrvrVectorArray *va;           /* Vector array */

	int i;

	long long fI, fQ;
	long fT;

	if ((buf->Points > LrfscDrvrCONFIG_POINTS)
	||  (buf->Which  > LrfscDrvrCONFIGS)
	||  (buf->Cycle  > LrfscDrvrCYCLES))
		return -EINVAL;

	/* Keep a copy of the unconverted user array in the module context */

	mcdst = &((*mcon).Configs[(int) buf->Which][(int) buf->Cycle]);
	memcpy((void *) mcdst, (void *) buf->Array, sizeof(LrfscDrvrConfigArray));
	mcon->ValidVecs[buf->Which][buf->Cycle] = 0;

	/* Target vector array to receive conversion */

	va =&((*mcon).Vectors[(int) buf->Which][(int) buf->Cycle]);
	vec = &((*va)[0]);

	p2 = &(buf->Array[0]);                /* First array entry */
	pi = *p2;                             /* Initial value */

	vec->Next  = sizeof(LrfscDrvrVector)/2;
	vec->Ticks = 1;                       /* Initial increment */
	vec->IncI.Long = (int) pi.IQ.I << 16;
	vec->IncQ.Long = (int) pi.IQ.Q << 16;

	for (i=1; i<buf->Points; i++) {

		vec = &((*va)[i]);

		p1 = p2;
		p2 = &(buf->Array[i]);

		fT = p2->Ticks - p1->Ticks;
		if (fT > 0) {
			fI = ((p2->IQ.I - p1->IQ.I) * 0x10000);
			do_div(fI, fT);
			fQ = ((p2->IQ.Q - p1->IQ.Q) * 0x10000);
			do_div(fQ, fT);

			vec->Next      = (i+1) * (sizeof(LrfscDrvrVector)/2);
			vec->IncI.Long = (int) fI;
			vec->IncQ.Long = (int) fQ;
			vec->Ticks     = (int) fT;
		} else
			return -EINVAL;
	}

	if (buf->Flag == LrfscDrvrFunctionREPEAT) {
		vec            = &((*va)[i]);
		vec->Next      = 0;
		vec->IncI.Long = (int) (pi.IQ.I - p2->IQ.I) << 16;
		vec->IncQ.Long = (int) (pi.IQ.Q - p2->IQ.Q) << 16;
		vec->Ticks     = 1;
		i++;
	}

	vec = &((*va)[i]);
	vec->Next      = 0;
	vec->Ticks     = 0xFFFF;
	vec->IncI.Long = 0;
	vec->IncQ.Long = 0;

	mcon->ValidVecs[buf->Which][buf->Cycle] = 1;

	return 0;
}

/**
 * =====================================================
 * Convert Time IQ increment to Time IQ value
 * Result is stored in the module config buf
 * =====================================================
 */

int inc2iq(LrfscDrvrModuleContext *mcon, LrfscDrvrConfigBuf *buf) {

	LrfscDrvrConfigArray *mcsrc;        /* Module Context Configuration array */

	if ((buf->Points > LrfscDrvrCONFIG_POINTS)
	||  (buf->Which  > LrfscDrvrCONFIGS)
	||  (buf->Cycle  > LrfscDrvrCYCLES))
		return -EINVAL;

	mcsrc = &((*mcon).Configs[(int) buf->Which][(int) buf->Cycle]);
	memcpy((void *) buf->Array, (void *) mcsrc, sizeof(LrfscDrvrConfigArray));

	return 0;
}

/**
 * =====================================================
 * OPEN
 * =====================================================
 */

int lrfsc_open(struct inode *inode, struct file *filp)
{
	long num;

	num = MINOR(inode->i_rdev);
	if (!check_minor(num))
		return -EACCES;

	return 0;
}

/**
 * =====================================================
 * Close
 * =====================================================
 */

int lrfsc_close(struct inode *inode, struct file *filp)
{
	long num;

	num = MINOR(inode->i_rdev);
	if (!check_minor(num))
		return -EACCES;

	return 0;
}

/*
 * =====================================================
 * Uninstall the driver
 * =====================================================
 */

void lrfsc_uninstall(void)
{
	int i;
	LrfscDrvrModuleContext *mcon;

	for (i = 0; i < luns_num; i++) {
		mcon = &Wa->ModuleContexts[i];
		unregister_module(mcon);
	}
	unregister_chrdev(lrfsc_major, major_name);
}

/*
 * =====================================================
 * Read
 * =====================================================
 */

ssize_t lrfsc_read(struct file * filp, char *buf, size_t count,
		   loff_t * f_pos)
{

	int cc;
	long num;
	struct inode *inode;

	LrfscDrvrInterruptBuffer rbuf;
	LrfscDrvrModuleContext  *mcon;
	int icnt;

	inode = filp->f_dentry->d_inode;
	num = MINOR(inode->i_rdev);
	if (!check_minor(num))
		return -EACCES;
	mcon = &Wa->ModuleContexts[num];

	if (mcon->debug) {
		printk("%s:read:count:%d minor:%d\n", major_name,
		       count, (int) num);
		if (mcon->debug > 1) {
			printk("%s:read:timout:%d\n", major_name,
			       mcon->timeout);
		}
	}

	if (count < sizeof(LrfscDrvrInterruptBuffer)) {
		if (mcon->debug) {
			printk("%s:read:Access error buffer too small\n",
			       major_name);
		}
		return -EACCES;
	}

	icnt = mcon->icnt;
	if (mcon->timeout) {
		cc = wait_event_interruptible_timeout(mcon->queue,
						      icnt != mcon->icnt,
						      mcon->timeout);
	} else {
		cc = wait_event_interruptible(mcon->queue,
					      icnt != mcon->icnt);
	}

	if (mcon->debug > 2) {
		printk("%s:wait_event:returned:%d\n", major_name,
		       cc);
	}

	if (cc == -ERESTARTSYS) {
		printk("%s:lrfsc_read:interrupted by signal\n",
		       major_name);
		return cc;
	}
	if (cc == 0 && mcon->timeout)
		return -ETIME;	/* Timer expired */
	if (cc < 0)
		return cc;	/* Error */

	rbuf.ICount    = mcon->icnt;
	rbuf.Interrupt = mcon->isrc;
	rbuf.Pulse     = mcon->pnum;
	rbuf.PCycle    = mcon->cnum;

	cc = copy_to_user(buf, &rbuf, sizeof(LrfscDrvrInterruptBuffer));
	if (cc != 0) {
		printk("%s:Can't copy to user space:cc=%d\n", major_name, cc);
		return -EACCES;
	}
	return sizeof(LrfscDrvrInterruptBuffer);

}

/**
 * =====================================================
 * IOCTL
 * =====================================================
 */

static inline int ioctl_err(int er, void *p, void *q)
{
	if (p) kfree(p);
	if (q) kfree(q);
	return er;
}

int lrfsc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	void  *arb; /* Argument buffer area */
	short *iob;

	LrfscDrvrModuleContext          *mcon;
	LrfscDrvrMemoryMap              *mmap;
	LrfscDrvrRawIoBlock             *riob;
	LrfscDrvrDiagChoices            *diagch;
	LrfscDrvrResCtrl                *resctl;
	LrfscDrvrAnalogSwitch           *anlgsw;
	LrfscDrvrSoftSwitch             *softsw;
	LrfscDrvrMatrixCoefficientsBuf  *coefbuf;
	LrfscDrvrDiagSnapShot           *snapsht;
	LrfscDrvrPicSetBuf              *picbuff;
	LrfscDrvrConfigBuf              *confbuf;
	LrfscDrvrDiagBuf                *diagbuf;
	LrfscDrvrVersion                *verbuf;
	LrfscDrvrModuleAddress          *moadbuf;
	LrfscDrvrIQPair                 *src, *dst;

	LrfscDrvrDiagSignalChoice        sigch;
	LrfscDrvrMatrix                  matrx;
	LrfscDrvrRawIoBlock              triob;

	int iodr;		/* Io Direction */
	int iosz;		/* Io Size in bytes */
	int ionr;		/* Io Number */

	short sav;
	int i, cc, err, cynum, lav, *lap, cnt;

	long num;

	ionr = _IOC_NR(cmd);
	iodr = _IOC_DIR(cmd);
	iosz = _IOC_SIZE(cmd);

	num = MINOR(inode->i_rdev);
	if (!check_minor(num))
		return -EACCES;
	mcon = &Wa->ModuleContexts[num];
	mmap =  (LrfscDrvrMemoryMap     *) mcon->Address.VMEAddress;

	if (ionr >= LrfscDrvrLAST_IOCTL_NUM || ionr <= LrfscDrvrFIRST_IOCTL_NUM)
		return -ENOTTY;

	if ((arb = kmalloc(iosz, GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if (iodr & _IOC_WRITE) {
		if (copy_from_user(arb, (void *)arg, iosz) != 0)
			ioctl_err(-EACCES, arb, NULL);
	}
	debug_ioctl(ionr, iodr, iosz, arb, num, mcon->debug);

	lap = (int *) arb;
	lav = *lap;

	cnt = get_clr_bus_errs();
	err = 0;

	switch (ionr) {

		case LrfscDrvrGET_MODULE_ADDRESS:       /* LrfscDrvrModuleAddress Read */
			moadbuf = (LrfscDrvrModuleAddress *) arb;
			*moadbuf = mcon->Address;
		break;

		case LrfscDrvrRESET:
			if (reset_module(mcon) < 0)
				return ioctl_err(-EIO, arb, NULL);

		break;

		case LrfscDrvrGET_STATUS:               /* LrfscDrvrStatus Get status */
			sav = HRd16(&mmap->Status);
			*lap = (unsigned long) sav;
		break;

		case LrfscDrvrSET_SW_DEBUG:             /* ulong Set software debug level */
			mcon->debug = lav;
			if (lav)
				printk("lrfsc driver:Debug level:%d on\n", (int) lav);
			else
				printk("lrfsc driver:Debug off\n");
		break;

		case LrfscDrvrGET_SW_DEBUG:             /* ulong Get software debug level */
			*lap = mcon->debug;
		break;

		case LrfscDrvrRAW_READ:         /* LrfscDrvrRawIoBlock Read  */
			riob = (LrfscDrvrRawIoBlock *) arb;
			if (riob->Size > MAX_BUF)
				return -E2BIG;
			iob = kmalloc(riob->Size, GFP_KERNEL);
			if (!iob)
				return ioctl_err(-ENOMEM, arb, NULL);
			triob = *riob;
			triob.UserArray = iob;

			if (mcon->debug > 1) {
				printk("RAW:READ:flag:%d offs:0x%X len:%d\n",
				       riob->RamFlag, riob->Offset, riob->Size);
			}

			if (RawIo(mcon, &triob, READ_FLAG) < 0)
				return ioctl_err(-EIO, arb, iob);

			cc = copy_to_user(riob->UserArray, iob, riob->Size);
			kfree(iob);
			if (cc)
				return ioctl_err(-EACCES, arb, NULL);
		break;

		case LrfscDrvrRAW_WRITE:        /* LrfscDrvrRawIoBlock Write */
			riob = (LrfscDrvrRawIoBlock *) arg;
			if (riob->Size > MAX_BUF)
				return -E2BIG;
			iob = kmalloc(riob->Size, GFP_KERNEL);
			if (!iob)
				return ioctl_err(-ENOMEM, arb, NULL);
			triob = *riob;
			triob.UserArray = iob;

			if (mcon->debug > 1) {
				printk("RAW:WRITE:flag:%d offs:0x%X len:%d\n",
				       riob->RamFlag, riob->Offset, riob->Size);
			}

			cc = copy_from_user(iob, riob->UserArray, riob->Size);
			if (cc)
				return ioctl_err(-EACCES, arb, iob);

			if (RawIo(mcon, &triob, WRITE_FLAG) < 0)
				return ioctl_err(-EIO, arb, iob);
			kfree(iob);
		break;

		case LrfscDrvrSET_TIMEOUT:              /* ulong Set read timeout */
			mcon->timeout = msecs_to_jiffies(lav);
		break;

		case LrfscDrvrGET_TIMEOUT:              /* ulong Get read timeout */
			*lap = jiffies_to_msecs(mcon->timeout);
		break;

		case LrfscDrvrSET_STATE:                        /* LrfscDrvrState Get Module state */
			sav = *lap;
			HWr16(&mmap->State,sav);
			mcon->State = (LrfscDrvrState) sav;
		break;

		case LrfscDrvrGET_STATE:
			sav = HRd16(&mmap->State);
			*lap = sav;
		break;

		case LrfscDrvrSET_DIAG_CHOICE:          /* Configure diagnostics channels */
			diagch = (LrfscDrvrDiagChoices *) arb;
			for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++) {
				sigch = (*diagch)[i];
				mcon->SignalChoices[i] = sigch;
				set_sig_choice(mcon,i);
			}
		break;

		case LrfscDrvrGET_DIAG_CHOICE:
			diagch = (LrfscDrvrDiagChoices *) arb;
			for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++) {
				sigch = mcon->SignalChoices[i];
				(*diagch)[i] = sigch;
			}
		break;

		case LrfscDrvrSET_RES_CTRL:                     /* LrfscDrvrResCtrl Set Resonance control value */
			resctl = (LrfscDrvrResCtrl *) arb;
			mcon->ResCtrl = *resctl;
			HWr16(&mmap->ResCtrl.Time,  (unsigned short) mcon->ResCtrl.Time);
			HWr16(&mmap->ResCtrl.Value, (unsigned short) mcon->ResCtrl.Value);
		break;

		case LrfscDrvrGET_RES_CTRL:
			resctl = (LrfscDrvrResCtrl *) arb;
			*resctl = mcon->ResCtrl;
			resctl->Cav.I = HRd16(&mmap->ResCtrl.Cav.I);
			resctl->Cav.Q = HRd16(&mmap->ResCtrl.Cav.Q);
			resctl->Fwd.I = HRd16(&mmap->ResCtrl.Fwd.I);
			resctl->Fwd.Q = HRd16(&mmap->ResCtrl.Fwd.Q);
		break;

		case LrfscDrvrSET_ANALOGUE_SWITCH:      /* LrfscDrvrAnalogSwitch */
			anlgsw = (LrfscDrvrAnalogSwitch *) arb;
			mcon->SwitchCtrl = *anlgsw;
			HWr16(&mmap->SwitchCtrl, (unsigned short) mcon->SwitchCtrl);
		break;

		case LrfscDrvrGET_ANALOGUE_SWITCH:
			anlgsw = (LrfscDrvrAnalogSwitch *) arb;
			*anlgsw = mcon->SwitchCtrl;
		break;

		case LrfscDrvrSET_SOFT_SWITCH:          /* LrfscDrvrSoftSwitch */
			softsw = (LrfscDrvrSoftSwitch *) arb;
			mcon->SoftSwitch = *softsw;
			HWr16(&mmap->SoftSwitch, (unsigned short) mcon->SoftSwitch);
		break;

		case LrfscDrvrGET_SOFT_SWITCH:
			softsw = (LrfscDrvrSoftSwitch *) arb;
			*softsw = mcon->SoftSwitch;
		break;

		case LrfscDrvrSET_COEFFICIENTS:         /* LrfscDrvrMatrixCoefficientsBuf */
			coefbuf = (LrfscDrvrMatrixCoefficientsBuf *) arb;
			matrx = coefbuf->Matrix;
			mcon->Coefficients[matrx] = coefbuf->Coeficients;
		break;

		case LrfscDrvrGET_COEFFICIENTS:
			coefbuf = (LrfscDrvrMatrixCoefficientsBuf *) arb;
			matrx = coefbuf->Matrix;
			coefbuf->Coeficients = mcon->Coefficients[matrx];
		break;

		case LrfscDrvrSET_SNAP_SHOT_TIME:       /* ulong Snapshot time */
			mcon->DiagTime = lav;
			HWr16(&mmap->SnapShot.DiagTime, (unsigned short) mcon->DiagTime);
		break;

		case LrfscDrvrGET_SNAP_SHOTS:           /* LrfscDrvrDiagSnapShot */
			snapsht = (LrfscDrvrDiagSnapShot *) arb;
			snapsht->DiagTime       = HRd16(&mmap->SnapShot.DiagTime);
			snapsht->RefDiag.I      = HRd16(&mmap->SnapShot.RefDiag.I);
			snapsht->RefDiag.Q      = HRd16(&mmap->SnapShot.RefDiag.Q);
			snapsht->FwdDiag.I      = HRd16(&mmap->SnapShot.FwdDiag.I);
			snapsht->FwdDiag.Q      = HRd16(&mmap->SnapShot.FwdDiag.Q);
			snapsht->CavDiag.I      = HRd16(&mmap->SnapShot.CavDiag.I);
			snapsht->CavDiag.Q      = HRd16(&mmap->SnapShot.CavDiag.Q);
			snapsht->ErrDiag.I      = HRd16(&mmap->SnapShot.ErrDiag.I);
			snapsht->ErrDiag.Q      = HRd16(&mmap->SnapShot.ErrDiag.Q);
			snapsht->OutDiag.I      = HRd16(&mmap->SnapShot.OutDiag.I);
			snapsht->OutDiag.Q      = HRd16(&mmap->SnapShot.OutDiag.Q);
			snapsht->POutDiag.I     = HRd16(&mmap->SnapShot.POutDiag.I);
			snapsht->POutDiag.Q     = HRd16(&mmap->SnapShot.POutDiag.Q);
			snapsht->IOutDiag.I     = HRd16(&mmap->SnapShot.IOutDiag.I);
			snapsht->IOutDiag.Q     = HRd16(&mmap->SnapShot.IOutDiag.Q);
		break;

		case LrfscDrvrSET_PIC:                          /* LrfscDrvrPicSetBuf PI Controller */
			picbuff = (LrfscDrvrPicSetBuf *) arb;
			mcon->Pic.KI = picbuff->KI;
			mcon->Pic.KP = picbuff->KP;
			HWr16(&mmap->Pic.KI, (short) mcon->Pic.KI);
			HWr16(&mmap->Pic.KP, (short) mcon->Pic.KP);
		break;

		case LrfscDrvrGET_PIC:
			picbuff = (LrfscDrvrPicSetBuf *) arb;
			picbuff->KI = mcon->Pic.KI;
			picbuff->KP = mcon->Pic.KP;
		break;

		case LrfscDrvrGET_PULSE_LENGTH:         /* ulong RF Pulse length */
		    *lap = HRd16(&mmap->RfOffTime);
		break;

		case LrfscDrvrSET_MAX_PULSE_LENGTH: /* ulong maximum RF pulse length */
			mcon->RfOnMaxLen = lav;
			HWr16(&mmap->RfOnMaxLen, (unsigned short) mcon->RfOnMaxLen);
		break;

		case LrfscDrvrGET_MAX_PULSE_LENGTH: /* ulong maximum RF pulse length */
			*lap = (unsigned long) mcon->RfOnMaxLen;
		break;

		case LrfscDrvrSET_NEXT_CYCLE:           /* ulong Next cycle 1..32 */
			HWr16(&mmap->NextCycle, (unsigned short) lav);
		break;

		case LrfscDrvrGET_PRES_CYCLE:
			sav = HRd16(&mmap->PresCycle);
			*lap = sav;
		break;

		case LrfscDrvrSET_CYCLE_CONFIG:                         /* LrfscDrvrConfigBuf Set a cycle configuration */
		    confbuf = (LrfscDrvrConfigBuf *) arb;
		    err = iq2inc(mcon, confbuf);
		break;

		case LrfscDrvrGET_CYCLE_CONFIG:                         /* LrfscDrvrConfigBuf Get a cycle configuration */
			confbuf = (LrfscDrvrConfigBuf *) arb;
			inc2iq(mcon, confbuf);
		break;

		case LrfscDrvrSET_SKIP_COUNT:           /* ulong Set diagnostic skip count */
			mcon->SkipCount = lav;
		break;

		case LrfscDrvrSET_SKIP_START:           /* ulong Set diagnostic skip count */
			mcon->SkipStart = lav;
		break;

		case LrfscDrvrSET_PULSE:                /* LrfscDrvrPulse Set acquisition pulses */
			if ((lav > 0) && (lav <= LrfscDrvrPULSES))
				mcon->Pulse = lav;
		break;

		case LrfscDrvrGET_DIAGNOSTICS:          /* LrfscDrvrDiagBuf Get a diagnostic ram */
			diagbuf = (LrfscDrvrDiagBuf *) arb;
			diagbuf->SkipStart = mcon->SkipStart;
			diagbuf->SkipCount = mcon->SkipCount;
			cynum = diagbuf->Cycle;
			sigch = diagbuf->Choice;
			diagbuf->Valid = mcon->ValidDiags[sigch][cynum];
			for (i=0; i<diagbuf->Size; i++) {
				dst  = &((*diagbuf).Array[i]);
				src  = &((*mcon).Diags[sigch][cynum][i]);
				*dst = *src;
			}
		break;

		case  LrfscDrvrGET_VERSION:          /* LrfscDrvrVersion Get driver and VHDL versions */
			verbuf = (LrfscDrvrVersion *) arb;
			sav = HRd16(&mmap->VhdlVerH);
			verbuf->Firmware = sav << 16;
			sav = HRd16(&mmap->VhdlVerL);
			verbuf->Firmware |= sav;
			verbuf->Driver = COMPILE_TIME;
		break;

		default:
			return ioctl_err(-ENOENT, arb, NULL);
		break;
	}

	if (iodr & _IOC_READ) {
		if (copy_to_user((void *) arg, arb, iosz) != 0)
			return ioctl_err(-EACCES, arb, NULL);
	}
	if (get_clr_bus_errs()) err = -EIO;
	return ioctl_err(err, arb, NULL);
}

/**
 * =====================================================
 * Write
 * Used to simulate interrupts
 * =====================================================
 */

ssize_t lrfsc_write(struct file * filp, const char *buf, size_t count,
		    loff_t * f_pos)
{
	long num;
	LrfscDrvrModuleContext *mcon;
	struct inode *inode;
	LrfscDrvrInterruptBuffer *rbuf;
	int cc;

	inode = filp->f_dentry->d_inode;
	num = MINOR(inode->i_rdev);
	if (!check_minor(num))
		return -EACCES;
	mcon = &Wa->ModuleContexts[num];

	if (mcon->debug)
		printk("%s:write:count:%d minor:%d\n", major_name,
		       count, (int) num);

	if (count < sizeof(LrfscDrvrInterruptBuffer)) {
		if (mcon->debug) {
			printk("%s:write:Access error buffer too small\n",
			       major_name);
		}
		return -EACCES;
	}

	rbuf = (LrfscDrvrInterruptBuffer *) buf;

	cc = copy_from_user(&rbuf, buf, sizeof(LrfscDrvrInterruptBuffer));
	if (cc != 0) {
		printk("%s:write:Error:%d could not copy from user\n",
		       major_name, cc);
		return -EACCES;
	}

	if (mcon->debug) {
		printk("%s:write:count:%d minor:%d mask:0x%X\n",
		       major_name, count, (int) num, rbuf->Interrupt);
	}

	mcon->icnt++;
	mcon->isrc = rbuf->Interrupt;
	mcon->pnum = rbuf->Pulse;
	mcon->cnum = rbuf->PCycle;

	wake_up(&mcon->queue);

	return sizeof(LrfscDrvrInterruptBuffer);
}

/**
 * =====================================================
 */

static DEFINE_MUTEX(driver_mutex);

long lrfsc_ioctl64(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int res;
	mutex_lock(&driver_mutex);
	res = lrfsc_ioctl(filp->f_dentry->d_inode, filp, cmd, arg);
	mutex_unlock(&driver_mutex);
	return res;
}

int lrfsc_ioctl32(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	int res;
	mutex_lock(&driver_mutex);
	res = lrfsc_ioctl(inode, filp, cmd, arg);
	mutex_unlock(&driver_mutex);
	return res;
}

struct file_operations lrfsc_fops = {
	.read = lrfsc_read,
	.write = lrfsc_write,
	.ioctl = lrfsc_ioctl32,
	.compat_ioctl = lrfsc_ioctl64,
	.open = lrfsc_open,
	.release = lrfsc_close,
};

