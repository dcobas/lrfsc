/***************************************************************************/
/* Llcc test program.                                                      */
/* Feb 2003                                                                */
/* Ioan Kozsar                                                             */
/***************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>                      /* Unix system error numbers */
#include <sys/file.h>
#include <a.out.h>
#include <sys/file.h>

#include <tgm/tgm.h>                    /* Telegram library definitions    */
#include <tgm/con.h>                    /* Controls stuff like TRUE/FALSE  */

#include <llccdrvr.h>                 /* Llcc module device driver stuff  */

extern int errno;                       /* System error number for perror  */

/***************************************************************************/
/* Some static variables                                                   */
/***************************************************************************/

static int              llcc    = 0;      /* Llcc driver file handel.    */
static unsigned int     mod     = 1;      /* Module number                 */
static unsigned int     chn     = 1;      /* Channel number                */
static unsigned int     deb     = 0;      /* Debug flag                    */



/***************************************************************************/
/* Command line interpreter: Standard definitions for commands.            */
/***************************************************************************/


#define PARAMS 2

typedef enum {
   CmdFIRST,CmdQUIT, CmdREAD, CmdCONNECT, CmdDISCONNECT,
   CmdVECTOR, CmdSTATUS, CmdRESET,
   CmdREQUEST,CmdINTERRUPTS,
   CmdOUT_SWITCH,CmdREF_SWITCH, CmdFWD_SWITCH, CmdCAV_SWITCH,
   CmdMAINLOOP_SWITCH, CmdRAM_SELECT,
   CmdDIAG1_SELECT, CmdDIAG2_SELECT, CmdDIAG3_SELECT, CmdDIAG4_SELECT,
   CmdRES_CONTROL,
   CmdREF_MATRIX_A, CmdREF_MATRIX_B, CmdREF_MATRIX_C, CmdREF_MATRIX_D,
   CmdFWD_MATRIX_A, CmdFWD_MATRIX_B, CmdFWD_MATRIX_C, CmdFWD_MATRIX_D,
   CmdCAV_MATRIX_A, CmdCAV_MATRIX_B, CmdCAV_MATRIX_C, CmdCAV_MATRIX_D,
   CmdDIAGTIME,
   CmdREF_DIAG_I, CmdREF_DIAG_Q,
   CmdFWD_DIAG_I, CmdFWD_DIAG_Q,
   CmdCAV_DIAG_I, CmdCAV_DIAG_Q,
   CmdERR_DIAG_I, CmdERR_DIAG_Q,
   CmdLRN_DIAG_I, CmdLRN_DIAG_Q,
   CmdOUT_DIAG_I, CmdOUT_DIAG_Q,
   CmdKP, CmdKI,
   CmdIP_OUT, CmdQP_OUT, CmdII_OUT, CmdQI_OUT,
   CmdRFOFFTIME,
/*   CmdKLYSGAINSP,*/
   CmdHELP,CmdSHELL,
   CmdVER  ,CmdMOD ,CmdTYPE,
   CmdGAT  ,CmdDEB ,CmdBAT,
   CmdLIST ,
   CmdLAST
 } Command;

char * commands[CmdLAST+1] = {
   "?","q","read","cn","discn","vec","status","reset",
   "request","nb",
   "outsw","refsw","fwdsw","cavsw","mainloopsw","ramsel",
   "diag1sel","diag2sel","diag3sel","diag4sel",
   "resctrl",
   "mrefa","mrefb","mrefc","mrefd",
   "mfwda","mfwdb","mfwdc","mfwdd",
   "mcava","mcavb","mcavc","mcavd",
   "diagtime",
   "refdiagi","refdiagq",
   "fwddiagi","fwddiagq",
   "cavdiagi","cavdiagq",
   "errdiagi","errdiagq",
   "lrndiagi","lrndiagq",
   "outdiagi","outdiagq",
   "kp","ki",
   "ipout","qpout","iiout","qiout",
   "rfofftime",
//   "klys",
   "h"," ",
   "ver","mo","ty",
   "gat","deb","bat","ls",
   "?" };

char * hlptxt =
" q          ------- Quit prorgram\n"
" h          ------- Help\n"
" ver        ------- Version\n"
" mo  [mod]  ------- Get/Set module\n"
" cn         ------- Connect to module\n"
" read       ------- Read\n"
" vec        ------- Vector\n"
" status     ------- Status\n"
" reset      ------- Reset\n"
" request    ------- IO Request\n"
" nb         ------- Number of interrupts\n"
" outsw      ------- Set/Get Out Switch\n"
" refsw      ------- Set/Get Ref Switch\n"
" fwdsw      ------- Set/Get Fwd Switch\n"
" cavsw      ------- Set/Get Cav Switch\n"
" mainloopsw ------- Set/Get MainLoop Switch\n"
" ramsel     ------- Set/Get Out Switch\n"
" diag[n]sel ------- Set/Get Diag[n] Switch\n"
" resctrl    ------- Get Resctrl value\n"
" m+mat[a/b/c/d] --- Set/Get Diag1 Switch\n"
" diagtime   ------- Set/Get DiagTime, nb of ticks after the ON pulse\n"
" [wch]diag[i/q] --- Get the snapshots\n"
" rfofftime  ------- Get acquired nb of ticks during RFON pulse\n"
//" klys       ------- Set/Get Klystron Gain set point\n"
" ty         ------- Get module type\n"
" gat [0/1/2/4]  --- Get/Set gate mode\n"
" ls         ------- List module\n"
" deb [deb]  ------- Get/Set debug\n"
" bat        ------- Edit llccinit.bat file used to initialize on reboot\n"
" <dot> = Repeat <space> = shell\n"
"\n"
"\n"
;

/*************************************************************************/
/* Main: Get command and execute them.                                   */
/*       All commands are context free and do all work required each time*/
/*       they are called.                                                */
/*************************************************************************/

int main(argc,argv)
int argc; char **argv; {

unsigned int i,     /* General purpose loop variable                 */
	     cc;        /* Driver completion code -1 is an error 0 is OK */
LlccDrvrReadBuf *rbf;

char dummy;

/* Variables used by the test program command line interpreter           */

char    s[128],     /* Users input command line                      */
	strsv[128],     /* Previous saved command line used in repeat (.)*/
	host[32],       /* Host name                                     */
	ver[64],        /* Driver version                                */
	*cp,            /* Start char pointer used in strtoul and others */
	*ep;            /* End char pointer updated by strtoul           */

int     param_count,    /* Number of parameters on the command line      */
	params[PARAMS], /* Array of command parameters                   */
	*param;         /* Pointer to params[0], redundant but used      */



Command command;        /* The current command used in command switch    */

   /* Open the Llcc minor device */

   for (i=1; i<=LlccDrvrCLIENT_CONTEXTS;i++) {
      sprintf(s,"/dev/llcc.%1d",i);
      if ((llcc = open(s,O_RDWR,0)) > 0) 
      {
      	printf("llcctest::main: opened file handle: %s\n", s);
		break;
      }
   };
   if (llcc <= 0) printf("WARNING: Can't open Llcc device driver\n");

   printf("llcctest: Compiled: %s %s\n",__DATE__,__TIME__);

   /* Main program loop issues a prompt, accepts new command and decodes */
   /* it into command and parameters, and then jumps to specific command */
   /* code in the command switch.                                        */

   while(True) {

      gethostname(host,32);
      printf("Llcc: %s [%d(%d)]: ",host,mod,chn);

      /* Read in users command string and deal with special characters */

      if (gets(s)==NULL) exit(1);
      if (*s == '\0') continue;                 /* Blank command line */
      if (*s == '.') {                          /* Repeat last command */
	 strcpy(s,strsv);                       /* restore command from saved command */
	 printf("Repeat command: \"%s\"\n",s);
      } else                                    /* Save the command string for repeat */
	 strcpy(strsv,s);

      /* Get the command number from the input string */

     for (command = CmdFIRST; command < CmdLAST; command++)
	 if (strncmp(s, commands[(int) command],
		     strlen(commands[(int) command]))
	 == 0) break;

      /* Get any parameters supplied */

      param_count = 0;                          /* Number of parameters initialize to zero */
      bzero(params,PARAMS*sizeof(int));         /* clear parameter values array */
      cp = &(s[strlen(commands[(int) command])]);

      while (param_count < PARAMS) {            /* Fill parameters array from command line */
	 params[param_count] = strtoul(cp,&ep,0);
	 if (cp == ep) break;
	 param_count++;
	 cp = ep;
      };
      param = &(params[0]);

      /*******************************/
      /* The main switch on command. */
      /* Go do the command           */

      switch ((Command) command) {

	 case CmdQUIT:
	    printf("QUIT\n");
	    exit(0);
	 break;
	 
	 case CmdREAD:
	    //cc = read(llcc,&rbf,sizeof(LlccDrvrReadBuf));
	    cc = read(llcc,&dummy,1);
	    if (cc == 0) printf("INTERRUPT !!!\n");
	 break;
	 
	 case CmdCONNECT:
	    cc = ioctl(llcc,LlccDrvrCONNECT,NULL);
	 break;
	 
	 case CmdDISCONNECT:
	    cc = ioctl(llcc,LlccDrvrDISCONNECT,NULL);
	 break;
	 
	 case CmdVECTOR:
	    cc = ioctl(llcc,LlccDrvrGET_IRQ_VECTOR,NULL);
	 break;
	 
	 case CmdSTATUS:
	    if (param_count) {
	    	i = *param;
	       	cc = ioctl(llcc,LlccDrvrSET_IRQ_ENABLE,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_IRQ_ENABLE,NULL);
	 break;
	 
	 case CmdRESET:
	    cc = ioctl(llcc,LlccDrvrRESET_ALL,NULL);
	 break;
	 
	 case CmdREQUEST:
	 	cc = ioctl(llcc, LlccDrvrREQUEST, NULL);
	 break;
	 
	 case CmdINTERRUPTS:
	 	cc = ioctl(llcc, LlccDrvrINTERRUPTS, NULL);
	 break;
	 
	 case CmdREF_SWITCH:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_REF_SWITCH,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_REF_SWITCH,NULL);
	 break;
	 
	 case CmdFWD_SWITCH:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_FWD_SWITCH,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_FWD_SWITCH,NULL);
	 break;
	 
	 case CmdCAV_SWITCH:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_CAV_SWITCH,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_CAV_SWITCH,NULL);
	 break;
	 
	 case CmdOUT_SWITCH:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_OUT_SWITCH,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_OUT_SWITCH,NULL);
	 break;
	 
	 case CmdMAINLOOP_SWITCH:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_MAINLOOP_SWITCH,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_MAINLOOP_SWITCH,NULL);
	 break;
	 
	 case CmdRAM_SELECT:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_RAM_SELECT,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_RAM_SELECT,NULL);
	 break;
	 
	 case CmdDIAG1_SELECT:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_DIAG1_SELECT,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_DIAG1_SELECT,NULL);
	 break;
	 
	 case CmdDIAG2_SELECT:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_DIAG2_SELECT,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_DIAG2_SELECT,NULL);
	 break;
	 
	 case CmdDIAG3_SELECT:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_DIAG3_SELECT,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_DIAG3_SELECT,NULL);
	 break;
	 
	 case CmdDIAG4_SELECT:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_DIAG4_SELECT,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_DIAG4_SELECT,NULL);
	 break;
	 
	 case CmdRES_CONTROL:
	    cc = ioctl(llcc,LlccDrvrGET_RES_CONTROL,NULL);
	 break;
	 
	 /* REF MATRIX */
	 
	 case CmdREF_MATRIX_A:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_REF_MATRIX_A,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_REF_MATRIX_A,NULL);
	 break;
	 
	 case CmdREF_MATRIX_B:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_REF_MATRIX_B,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_REF_MATRIX_B,NULL);
	 break;
	 
	 case CmdREF_MATRIX_C:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_REF_MATRIX_C,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_REF_MATRIX_C,NULL);
	 break;
	 
	 case CmdREF_MATRIX_D:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_REF_MATRIX_D,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_REF_MATRIX_D,NULL);
	 break;
	 
	 /* FWD MATRIX */
	 
	 case CmdFWD_MATRIX_A:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_FWD_MATRIX_A,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_FWD_MATRIX_A,NULL);
	 break;
	 
	 case CmdFWD_MATRIX_B:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_FWD_MATRIX_B,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_FWD_MATRIX_B,NULL);
	 break;
	 
	 case CmdFWD_MATRIX_C:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_FWD_MATRIX_C,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_FWD_MATRIX_C,NULL);
	 break;
	 
	 case CmdFWD_MATRIX_D:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_FWD_MATRIX_D,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_FWD_MATRIX_D,NULL);
	 break;
	 
	 /* CAV MATRIX */
	 
	 case CmdCAV_MATRIX_A:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_CAV_MATRIX_A,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_CAV_MATRIX_A,NULL);
	 break;
	 
	 case CmdCAV_MATRIX_B:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_CAV_MATRIX_B,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_CAV_MATRIX_B,NULL);
	 break;
	 
	 case CmdCAV_MATRIX_C:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_CAV_MATRIX_C,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_CAV_MATRIX_C,NULL);
	 break;
	 
	 case CmdCAV_MATRIX_D:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_CAV_MATRIX_D,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_CAV_MATRIX_D,NULL);
	 break;
	 
	 /* DIAGTIME */
	 
	 case CmdDIAGTIME:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_DIAGTIME,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_DIAGTIME,NULL);
	 break;
	 
	 /* SNAPSHOTS */
	 
	 case CmdREF_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_REF_DIAG_I,NULL);
	 break;
	 
	 case CmdREF_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_REF_DIAG_Q,NULL);
	 break;
	 
	 case CmdFWD_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_FWD_DIAG_I,NULL);
	 break;
	 
	 case CmdFWD_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_FWD_DIAG_Q,NULL);
	 break;
	 
	 case CmdCAV_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_CAV_DIAG_I,NULL);
	 break;
	 
	 case CmdCAV_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_CAV_DIAG_Q,NULL);
	 break;
	 
	 case CmdERR_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_ERR_DIAG_I,NULL);
	 break;
	 
	 case CmdERR_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_ERR_DIAG_Q,NULL);
	 break;
	 
	 case CmdLRN_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_LRN_DIAG_I,NULL);
	 break;
	 
	 case CmdLRN_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_LRN_DIAG_Q,NULL);
	 break;
	 
	 case CmdOUT_DIAG_I:
	    cc = ioctl(llcc,LlccDrvrGET_OUT_DIAG_I,NULL);
	 break;
	 
	 case CmdOUT_DIAG_Q:
	    cc = ioctl(llcc,LlccDrvrGET_OUT_DIAG_Q,NULL);
	 break;
	 
	 /* PI controller */
	 
	 case CmdKP:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_KP,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_KP,NULL);
	 break;
	 
	 case CmdKI:
	    if (param_count) {
	    	i = *param;
	    	cc = ioctl(llcc,LlccDrvrSET_KI,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_KI,NULL);
	 break;
	 
	 case CmdIP_OUT:
	    cc = ioctl(llcc,LlccDrvrGET_IP_OUT,NULL);
	 break;
	 
	 case CmdQP_OUT:
	    cc = ioctl(llcc,LlccDrvrGET_QP_OUT,NULL);
	 break;
	 
	 case CmdII_OUT:
	    cc = ioctl(llcc,LlccDrvrGET_II_OUT,NULL);
	 break;
	 
	 case CmdQI_OUT:
	    cc = ioctl(llcc,LlccDrvrGET_QI_OUT,NULL);
	 break;
	 
	 /* RFOFFTIME */
	 
	 case CmdRFOFFTIME:
	    cc = ioctl(llcc,LlccDrvrGET_DIAGTIME,NULL);
	 break;
	 
	 /* KLYSTRON GAIN SET POINT */
	 
//	 case CmdKLYSGAINSP:
//	    if (param_count) {
//	    	i = *param;
//	    	cc = ioctl(llcc,LlccDrvrSET_KLYSGAINSP,&i);
//	    }
//	    cc = ioctl(llcc,LlccDrvrGET_KLYSGAINSP,NULL);
//	 break;

	 case CmdSHELL:
	    printf("SHELL: %s\n",&(s[1]));
	    system(&(s[1]));
	 break;

	 case CmdFIRST:
	 case CmdLAST:
	 case CmdHELP:
	   printf("HELP: Commands are:\n%s",hlptxt);
	 break;

	 case CmdDEB:
	    if (param_count) {
	       if (*param) deb = 1;
	       else        deb = 0;
	       cc = ioctl(llcc,LlccDrvrSET_DEBUG,&deb);
	    }
	    if (deb) cp = "ON";
	    else     cp = "OFF";
	    printf("DEB: %s\n",cp);
	 break;

	 case CmdBAT:
	    system("e /dsc/local/data/llccinit.bat");
	    system("llcctest < /dsc/local/data/llccinit.bat");
	    break;

	 case CmdVER:
	    cc = ioctl(llcc,LlccGET_DRIVER_VERSION,ver);
	    printf("VER: Driver Version: %s\n",ver);
	 break;

	 case CmdMOD:
	    if (param_count) {
	       i = *param;
	       cc = ioctl(llcc,LlccDrvrSET_MODULE,&i);
	    }
	    cc = ioctl(llcc,LlccDrvrGET_MODULE,&mod);
	    printf("MOD: %d\n",mod);
	 break;

	 case CmdTYPE:
	    printf("\n");
	 break;

	 case CmdLIST:
	       printf("\n");
	 break;

	 case CmdGAT:
	    printf("\n");
	    break;

	default:
	   printf("%s\n",s);
	 break;

      }
   }
}
