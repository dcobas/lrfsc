/* ****************************************************************************** */
/* Lrfsc test program                                                             */
/* Julian Lewis Mon 9th June 2008. First Version implemented.                     */
/* ****************************************************************************** */

/* Environment: This test program expects read write access to a temp directory,  */
/*              and read access to a permanent directory containing binaries and  */
/*              configuration data. See TEMP_DIR & CONF_DIR below.                */

#ifndef COMPILE_TIME
#define COMPILE_TIME 0
#endif

#define LN 128
#define TEMP_DIR "/tmp"
#define CONF_DIR "/dsc/local/data/lrfsc"

/* ****************************************************************************** */

#include <lrfscdrvr.h>
#include <lrfscdrvrP.h>

static int module = 1;
static char *editor = "e";
static char *intr_names[LrfscDrvrINTERRUPTS] = { "RfPulse", "StartCycle" };

/* ================================== */
/* Launch a task                      */

static void Launch(char *txt) {
pid_t child;

   if ((child = fork()) == 0) {
      if ((child = fork()) == 0) {
	 close(lrfsc);
	 system(txt);
	 exit (127);
      }
      exit (0);
   }
}

/**************************************************************************/

char *defaultconfigpath = CONF_DIR "/lrfsctest.config";

char *configpath = NULL;
char localconfigpath[128];  /* After a CD */

static char path[128];

/**************************************************************************/
/* Look in the config file to get paths to data needed by this program.   */
/* If no config path has been set up by the user, then ...                */
/* First try the current working directory, if not found try TEMP, and if */
/* still not found try CONF                                               */


char *GetFile(char *name) {

FILE *gpath = NULL;
char txt[128];
int i, j;

   if (configpath) {
      gpath = fopen(configpath,"r");
      if (gpath == NULL) configpath = NULL;
   }

   if (configpath == NULL) {
      configpath = "./lrfsctest.config";
      gpath = fopen(configpath,"r");
      if (gpath == NULL) {
	 configpath = TEMP_DIR "/lrfsctest.config";
	 gpath = fopen(configpath,"r");
	 if (gpath == NULL) {
	    configpath = defaultconfigpath;
	    gpath = fopen(configpath,"r");
	    if (gpath == NULL) {
	       configpath = NULL;
	       sprintf(path,"./%s",name);
	       return path;
	    }
	 }
      }
   }

   bzero((void *) path,128);

   while (1) {
      if (fgets(txt,128,gpath) == NULL) break;
      if (strncmp(name,txt,strlen(name)) == 0) {
	 for (i=strlen(name); i<strlen(txt); i++) if (txt[i] != ' ') break;
	 j= 0;
	 while ((txt[i] != ' ') && (txt[i] != 0)) {
	    path[j] = txt[i];
	    j++; i++;
	 }
	 strcat(path,name);
	 fclose(gpath);
	 return path;
      }
   }
   fclose(gpath);
   return NULL;
}

/**************************************************************************/
/* This routine handles getting File Names specified optionally on the    */
/* command line where an unusual file is specified not handled in GetFile */

char *GetFileName(int *arg) {

ArgVal   *v;
AtomType  at;
char     *cp;
int       n, earg;

   /* Search for the terminator of the filename or command */

   for (earg=*arg; earg<pcnt; earg++) {
      v = &(vals[earg]);
      if ((v->Type == Close)
      ||  (v->Type == Terminator)) break;
   }

   n = 0;
   bzero((void *) path,128);

   v = &(vals[*arg]);
   at = v->Type;
   if ((v->Type != Close)
   &&  (v->Type != Terminator)) {

      bzero((void *) path, 128);

      cp = &(cmdbuf[v->Pos]);
      do {
	 at = atomhash[(int) (*cp)];
	 if ((at != Seperator)
	 &&  (at != Close)
	 &&  (at != Terminator))
	    path[n++] = *cp;
	 path[n] = 0;
	 cp++;
      } while ((at != Close) && (at != Terminator));
   }
   if (n) {
      *arg = earg;
      return path;
   }
   return NULL;
}

/* ======================================= */
/* GNU plot launcher                       */

void GnuPlot() {

char cmd[LN];
char tmp[LN];
char *cp;

   cp = GetFile("Plot");
   if (cp) {
      strcpy(tmp,cp);

      cp = GetFile("gnuplot");
      if (cp) {
	 sprintf(cmd,"%s %s",cp,tmp);
	 printf("Launching: %s\n",cmd);
	 Launch(cmd);
	 printf("Type <CR> to end display\n");
      }
   }
}

/**************************************************************************/
/* Print out driver IOCTL error messages.                                 */

static void IErr(char *name, int *value) {

   if (value != NULL)
      printf("LrfscDrvr: [%02d] ioctl=%s arg=%d :Error\n",
	     (int) lrfsc, name, (int) *value);
   else
      printf("LrfscDrvr: [%02d] ioctl=%s :Error\n",
	     (int) lrfsc, name);
   perror("LrfscDrvr");
}

/*****************************************************************/
/* News                                                          */

int News(int arg) {

char sys[128], npt[128];

   arg++;

   if (GetFile("lrfsc_news")) {
      strcpy(npt,path);
      sprintf(sys,"%s %s",GetFile(editor),npt);
      printf("\n%s\n",sys);
      system(sys);                  \
      printf("\n");
   }
   return(arg);
}

/**************************************************************************/

static int yesno=1;

static int YesNo(char *question, char *name) {

int yn, c;

   if (yesno == 0) return 1;

   printf("%s: %s\n",question,name);
   printf("Continue (Y/N):"); yn = getchar(); c = getchar();
   if ((yn != (int) 'y') && (yn != 'Y')) return 0;
   return 1;
}

/*****************************************************************/
/* Commands used in the test program.                            */
/*****************************************************************/

int ChangeEditor(int arg) {

static int eflg = 0;

   arg++;
   if (eflg++ > 4) eflg = 1;

   if      (eflg == 1) editor = "e";
   else if (eflg == 2) editor = "emacs";
   else if (eflg == 3) editor = "nedit";
   else if (eflg == 4) editor = "vi";

   printf("Editor: %s\n",GetFile(editor));
   return arg;
}

/******************************************************************/
/* Convert an IQ pair to a string                                 */
/******************************************************************/

char *IQToStr(LrfscDrvrIQPair iq) {
	
double phs, amp;
static char res[64];
	
   phs = 0; if (iq.I) phs = atan((float) (iq.Q/iq.I));
   amp = sqrt(iq.I*iq.I + iq.Q*iq.Q);

   bzero((void *) res, 64);
   sprintf(res,"I:%06d Q:%06d Phase:%6.3f Ampl:%6.3f",iq.I,iq.Q,phs,amp);
   return res;
}

/*****************************************************************/
/* Set the config file path to user specified path               */

int ChangeDirectory(int arg) {

char txt[128], *cp;

   arg++;

   cp = GetFileName(&arg);
   if (cp) {
      strcpy(txt,cp);
      if (YesNo("Change lrfsctest config to:",txt)) {
	 configpath = localconfigpath;
	 strcpy(configpath,txt);
      }
   } else configpath = NULL;

   sprintf(txt,"%s %s",GetFile(editor),configpath);
   printf("\n%s\n",txt);
   system(txt);
   printf("\n");
   return(arg);
}

/*****************************************************************/
/* Select a LRFSC module.                                        */

int Module(int arg) {

ArgVal   *v;
AtomType  at;
int mod, cnt;
LrfscDrvrModuleAddress mad;

   arg++;

   v = &(vals[arg]);
   at = v->Type;
   if (at == Numeric) {
      arg++;
      if (v->Number) {
	 mod = v->Number;
	 if (ioctl(lrfsc,LrfscDrvrSET_MODULE,&mod) < 0) {
	    IErr("SET_MODULE",&mod);
	    return arg;
	 }
      }
      module = mod;
   }

   cnt = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_MODULE_COUNT,&cnt) < 0) {
      IErr("GET_MODULE_COUNT",NULL);
      return arg;
   }

   for (mod=1; mod<=cnt; mod++) {
      ioctl(lrfsc,LrfscDrvrSET_MODULE,&mod);
      ioctl(lrfsc,LrfscDrvrGET_MODULE_ADDRESS,&mad);
      printf("Module:%d Adr:0x%08X Ram:0x%08X Vec:0x%02X Lvl:0x%1X",
	     (int) mod,
	     (int) mad.VMEAddress,
	     (int) mad.RamAddress,
	     (int) mad.InterruptVector,
	     (int) mad.InterruptLevel);
      if (mod == module) printf(" <<==");
      printf("\n");
   }
   ioctl(lrfsc,LrfscDrvrSET_MODULE,&module);

   return arg;
}

/*****************************************************************/

int NextModule(int arg) {

int cnt;

   arg++;

   ioctl(lrfsc,LrfscDrvrGET_MODULE_COUNT,&cnt);
   if (module >= cnt) {
      module = 1;
      ioctl(lrfsc,LrfscDrvrSET_MODULE,&module);
   } else {
      module ++;
      ioctl(lrfsc,LrfscDrvrSET_MODULE,&module);
   }

   return arg;
}

/*****************************************************************/

int PrevModule(int arg) {

int cnt;

   arg++;

   ioctl(lrfsc,LrfscDrvrGET_MODULE_COUNT,&cnt);
   if (module > 1) {
      module --;
      ioctl(lrfsc,LrfscDrvrSET_MODULE,&module);
   } else {
      module = cnt;
      ioctl(lrfsc,LrfscDrvrSET_MODULE,&module);
   }

   return arg;
}

/*****************************************************************/

int SwDeb(int arg) { /* Arg!=0 => On, else Off */

ArgVal   *v;
AtomType  at;
int debug;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if (v->Number) debug = v->Number;
      else           debug = 0;
      if (ioctl(lrfsc,LrfscDrvrSET_SW_DEBUG,&debug) < 0) {
	 IErr("SET_SW_DEBUG",&debug);
	 return arg;
      }
   }
   debug = -1;
   if (ioctl(lrfsc,LrfscDrvrGET_SW_DEBUG,&debug) < 0) {
      IErr("GET_SW_DEBUG",NULL);
      return arg;
   }
   if (debug > 0) printf("Debug Level: [0x%X] On\n",debug);
   else           printf("Debug: Disabled\n");

   return arg;
}

/*****************************************************************/

int GetSetTmo(int arg) { /* Arg=0 => Timeouts disabled, else Arg = Timeout */

ArgVal   *v;
AtomType  at;
int timeout;

   arg++;
   v = &(vals[arg]);
   at = v->Type;
   if (at == Numeric) {
      arg++;
      timeout = v->Number;
      if (ioctl(lrfsc,LrfscDrvrSET_TIMEOUT,&timeout) < 0) {
	 IErr("SET_TIMEOUT",&timeout);
	 return arg;
      }
   }
   timeout = -1;
   if (ioctl(lrfsc,LrfscDrvrGET_TIMEOUT,&timeout) < 0) {
      IErr("GET_TIMEOUT",NULL);
      return arg;
   }
   if (timeout > 0) printf("Timeout: [%d] Enabled\n",timeout);
   else             printf("Timeout: [%d] Disabled\n",timeout);

   return arg;
}

/*****************************************************************/

#define MOD_FIELDS 54
static char *modflds[MOD_FIELDS] = {
   "IrqSrc","State","Control","VmeIrq","RamSelect",
   "Diag1Select","Diag2Select","Diag3Select","Diag4Select",
   "ResCtrlTime","ResCtrl",
   "ResFwdI","ResFwdQ","ResCavI","ResCavQ",
   "SwitchCtrl","SoftSwitch",
   "RefMatrixA","RefMatrixB","RefMatrixC","RefMatrixD",
   "FwdMatrixA","FwdMatrixB","FwdMatrixC","FwdMatrixD",
   "CavMatrixA","CavMatrixB","CavMatrixC","CavMatrixD",
   "DiagTime",
   "RefDiagI","RefDiagQ",
   "FwdDiagI","FwdDiagQ",
   "CavDiagI","CavDiagQ",
   "ErrDiagI","ErrDiagQ",
   "OutDiagI","OutDiagQ",
   "POutDiagI","POutDiagQ",
   "IOutDiagI","IOutDiagQ",
   "KP","KI",
   "RfOffTime",
   "PulseNumber","NextCycle","PresentCycle",
   "VhdlVerH","VhdlVerL",
   "Status","RfOnMaxLength" };

void EditMemory(int address, int ramflg) {

LrfscDrvrRawIoBlock iob;
unsigned short array[2];
unsigned short addr, *data;
char c, *cp, str[128], *fp;
int n, radix, nadr;

   printf("EditMemory: [/]: Open, [CR]: Next, [.]: Exit [x]: Hex, [d]: Dec\n\n");

   data = array;
   addr = address;
   radix = 16;
   c = '\n';

   while (1) {
      iob.Size = 1;
      iob.Offset = addr;
      iob.RamFlag = ramflg;
      iob.UserArray = array;
      if (ioctl(lrfsc,LrfscDrvrRAW_READ,&iob) < 0) IErr("RAW_READ",(int *) &addr);

      if (ramflg) {
	 if (addr & 1) fp = "Q:";
	 else          fp = "I:";
      } else           fp = modflds[addr];

      if (radix == 16) {
	 if (c=='\n') printf("%16s:Addr:0x%02x[%03d]: 0x%04x ", fp, (int) addr*2, (int) addr*2,(int) *data);
      } else {
	 if (c=='\n') printf("%16s:Addr:%04d: %5d ", fp, (int) addr*2,(int) *data);
      }

      c = (char) getchar();

      if (c == '/') {
	 bzero((void *) str, 128); n = 0;
	 while ((c != '\n') && (n < 128)) c = str[n++] = (char) getchar();
	 nadr = strtoul(str,&cp,radix);
	 if (cp != str) {
	    if ((ramflg == 0) && (nadr >= MOD_FIELDS*2)) {
	       printf("\nBad address:0x%2X[%3d]\n\n",(int) nadr, (int) nadr);
	       nadr = addr*2;
	    }
	    addr = nadr/2;
	 }
      }

      else if (c == 'x')  {radix = 16; addr--; continue; }
      else if (c == 'd')  {radix = 10; addr--; continue; }
      else if (c == '.')  { c = getchar(); printf("\n"); break; }
      else if (c == '\n') {
	 addr ++;
	 if (ramflg == 0) {
	    if (addr >= MOD_FIELDS) {
	       printf("\n");
	       addr = 0;
	    }
	 }
      }

      else {
	 bzero((void *) str, 128); n = 0;
	 str[n++] = c;
	 while ((c != '\n') && (n < 128)) c = str[n++] = (char) getchar();
	 *data = strtoul(str,&cp,radix);
	 if (cp != str) {
	    if (ioctl(lrfsc,LrfscDrvrRAW_WRITE,&iob) < 0) IErr("RAW_WRITE",(int *) &addr);
	 }
      }
   }
}

/*****************************************************************/

int ModIo(int arg) { /* Address */

ArgVal   *v;
AtomType  at;

   printf("RawIo on module address map\n");

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      EditMemory(v->Number/2,0);
   } else
      EditMemory(0,0);

   return arg;
}

/*****************************************************************/

int RamIo(int arg) { /* Address */

ArgVal   *v;
AtomType  at;

   printf("RawIo on module address map\n");

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      EditMemory(v->Number,1);
   } else
      EditMemory(0,1);

   return arg;
}

/*****************************************************************/

int GetSetQue(int arg) { /* Arg=Flag */

   ArgVal   *v;
   AtomType  at;
   long qflag, qsize, qover;

   arg++;

   v = &(vals[arg]);
   at = v->Type;
   if (at == Numeric) {
      arg++;
      qflag = v->Number;

      if (ioctl(lrfsc,LrfscDrvrSET_QUEUE_FLAG,&qflag) < 0) {
	 IErr("SET_QUEUE_FLAG",(int *) &qflag);
	 return arg;
      }
   }
   qflag = -1;
   if (ioctl(lrfsc,LrfscDrvrGET_QUEUE_FLAG,&qflag) < 0) {
      IErr("GET_QUEUE_FLAG",NULL);
      return arg;
   }
   if (ioctl(lrfsc,LrfscDrvrGET_QUEUE_SIZE,&qsize) < 0) {
      IErr("GET_QUEUE_SIZE",NULL);
      return arg;
   }
   if (ioctl(lrfsc,LrfscDrvrGET_QUEUE_OVERFLOW,&qover) < 0) {
      IErr("GET_QUEUE_OVERFLOW",NULL);
      return arg;
   }

   if (qflag == -1) {
      IErr("GET_QUEUE_FLAG",NULL);
      return arg;
   }

   if (qflag == 1) printf("NoQueueFlag: Set,   Queuing is Off\n");
   else { printf("NoQueueFlag: ReSet, Queuing is On\n");
	  printf("QueueSize: %d\n",    (int) qsize);
	  printf("QueueOverflow: %d\n",(int) qover);
   }
   return arg;
}

/*****************************************************************/

int ListClients(int arg) {

LrfscDrvrClientConnections cons;
LrfscDrvrConnection *con;
LrfscDrvrClientList clist;
int i, j, k, pid, mypid;

   arg++;

   mypid = getpid();

   if (ioctl(lrfsc,LrfscDrvrGET_CLIENT_LIST,&clist) < 0) {
      IErr("GET_CLIENT_LIST",NULL);
      return arg;
   }

   for (i=0; i<clist.Size; i++) {
      pid = clist.Pid[i];

      bzero((void *) &cons, sizeof(LrfscDrvrClientConnections));
      cons.Pid = pid;

      if (ioctl(lrfsc,LrfscDrvrGET_CLIENT_CONNECTIONS,&cons) < 0) {
	 IErr("GET_CLIENT_CONNECTIONS",NULL);
	 return arg;
      }

      if (pid == mypid) printf("Pid: %d (lrfsctest) ",pid);
      else              printf("Pid: %d ",pid);

      if (cons.Size) {
	 for (j=0; j<cons.Size; j++) {
	    con = &(cons.Connections[j]);
	    printf("[Mod:%d Interrupt:0x%X]:",(int) con->Module,(int) con->Interrupt);
	    for (k=0; k<LrfscDrvrINTERRUPTS; k++) {
	       if ((1<<k) & con->Interrupt) {
		  if (intr_names[k] != NULL)
		     printf(" %s",intr_names[k]);
		  else
		     printf(" Bit%02d",k);
	       }
	    }
	 }
      } else printf("No connections");
      printf("\n");
   }
   return arg;
}

/*****************************************************************/

int Reset(int arg) {

int module;

   arg++;

   if (ioctl(lrfsc,LrfscDrvrRESET,NULL) < 0) {
      IErr("RESET",NULL);
      return arg;
   }

   module = -1;
   if (ioctl(lrfsc,LrfscDrvrGET_MODULE,&module) < 0) {
      IErr("GET_MODULE",NULL);
      return arg;
   }

   printf("Reset Module: %d\n",module);

   return arg;
}

/*****************************************************************/

#define STAT_STR_SIZE 128

static char *statnames[LrfscDrvrSTATAE] = {
   "OVER_REF",
   "OVER_FWD",
   "OVER_CAV",
   "RF_TOO_LONG",
   "NO_FAST_PROTECT",
   "MISSING_TICK" };

char *StatusToStr(unsigned long stat) {

static char res[STAT_STR_SIZE];
int i, msk;

   bzero((void *) res,STAT_STR_SIZE);

   for (i=0; i<LrfscDrvrSTATAE; i++) {
      msk = 1 << i;
      if (stat & msk) {
	 strcat(res,statnames[i]);
	 strcat(res,":");
      }
   }
   return res;
}

/*****************************************************************/

volatile char *TimeToStr(time_t tod) {

static char tbuf[128];

char tmp[128];
char *yr, *ti, *md, *mn, *dy;

   bzero((void *) tbuf, 128);
   bzero((void *) tmp, 128);

   if (tod) {
      ctime_r (&tod, tmp);

      tmp[3] = 0;
      dy = &(tmp[0]);
      tmp[7] = 0;
      mn = &(tmp[4]);
      tmp[10] = 0;
      md = &(tmp[8]);
      if (md[0] == ' ')
	      md[0] = '0';
      tmp[19] = 0;
      ti = &(tmp[11]);
      tmp[24] = 0;
      yr = &(tmp[20]);

      sprintf (tbuf, "%s-%s/%s/%s %s"  , dy, md, mn, yr, ti);

   } else sprintf(tbuf, "--- Zero ---");

   return (tbuf);
}

/*****************************************************************/

int GetVersion(int arg) {

LrfscDrvrVersion version;
time_t tod;

   arg++;

   bzero((void *) &version, sizeof(LrfscDrvrVersion));
   if (ioctl(lrfsc,LrfscDrvrGET_VERSION,&version) < 0) {
      IErr("GET_VERSION",NULL);
      return arg;
   }

   tod = version.Driver;
   printf("Drvr: [%u] %s\n",(int) tod,TimeToStr(tod));

   tod = version.Firmware;
   printf("VHDL: [%u] %s\n",(int) tod,TimeToStr(tod));

   tod = COMPILE_TIME;
   printf("Test: [%u] %s\n",(int) tod,TimeToStr(tod));

   return arg;
}

/*****************************************************************/

static int connected = 0;

int WaitInterrupt(int arg) { /* msk, pulses */
ArgVal   *v;
AtomType  at;

LrfscDrvrConnection con;
int i, cc, qflag, qsize, nowait, interrupt, cnt;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   nowait = 0;
   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("WaitInterrupt: [-] Optional No Wait, Connect only\n");
	 printf("WaitInterrupt: Sources are:\n");
	 for (i=0; i<LrfscDrvrINTERRUPTS; i++) {
	    if (intr_names[i] != NULL) {
	       printf("0x%04X %s\n",(1<<i),intr_names[i]);
	    }
	 }
	 return arg;
      }
      if (v->OId == OprMI) {    /* A - means don't wait, just connect */
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 nowait = 1;
      }
   }

   interrupt = 0;
   if (at == Numeric) {
      arg++;

      if ((v->Number > 0) && (v->Number <= LrfscDrvrINTERUPT_MASK)) {
	 interrupt     = v->Number;
	 con.Module    = module;
	 con.Interrupt = interrupt;
	 con.Pulse     = LrfscDrvrPulseALL;

	 v = &(vals[arg]);
	 at = v->Type;
	 if (at == Numeric) {
	    arg++;

	    if (v->Number <= LrfscDrvrPULSES) {
	       arg++;

	       con.Pulse = v->Number;
	    }
	 }
	 if (ioctl(lrfsc,LrfscDrvrCONNECT,&con) < 0) {
	    IErr("CONNECT",&interrupt);
	    return arg;
	 }

	 connected = 1;
      } else {
	 con.Module     = module;
	 con.Interrupt  = 0;
	 if (ioctl(lrfsc,LrfscDrvrDISCONNECT,&con) < 0) {
	    IErr("DISCONNECT",&interrupt);
	    return arg;
	 }
	 connected = 0;
	 printf("Disconnected from all interrupts\n");
	 return arg;
      }
   }

   if (connected == 0) {
      printf("WaitInterrupt: Error: No connections to wait for\n");
      return arg;
   }

   if (nowait) {
      printf("WaitInterrupt: Connect: No Wait: OK\n");
      return arg;
   }

   cnt = 0;
   do {
      cc = read(lrfsc,&con,sizeof(LrfscDrvrConnection));
      if (cc <= 0) {
	 printf("Time out or Interrupted call\n");
	 return arg;
      }
      if (interrupt != 0) {
	 if (con.Interrupt & interrupt) break;
      } else break;
      if (cnt++ > 64) {
	 printf("Aborted wait loop after 64 iterations\n");
	 return arg;
      }
   } while (True);

   if (ioctl(lrfsc,LrfscDrvrGET_QUEUE_FLAG,&qflag) < 0) {
      IErr("GET_QUEUE_FLAG",NULL);
      return arg;
   }

   if (ioctl(lrfsc,LrfscDrvrGET_QUEUE_SIZE,&qsize) < 0) {
      IErr("GET_QUEUE_SIZE",NULL);
      return arg;
   }

   printf("Mod[%d] Int[0x%X] Pls[0x%X]\n",
	  (int) con.Module,
	  (int) con.Interrupt,
	  (int) con.Pulse);

   if (qflag == 0) {
      if (qsize) printf(" Queued: %d",(int) qsize);
   }
   printf("\n");

   return arg;
}

/*****************************************************************/

int ReadStatus(int arg) { /* msk */

LrfscDrvrStatus stat;
	
   arg++;

   stat = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_STATUS,&stat) <0) {
      IErr("GET_STATUS",NULL);
      return arg;
   }
   printf("Status:[0x%2X]%s\n",(int) stat, StatusToStr((unsigned long) stat));
   return arg;
}

/******************************************************************/

static char *statenames[LrfscDrvrSTATES] = {"Config", "ProdLocal", "ProdRemote"};

int GetSetState(int arg) {

ArgVal   *v;
AtomType  at;

LrfscDrvrState stat;
int i;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("State Values:");
	 for (i=0; i<LrfscDrvrSTATES; i++) {
	    printf("%d=%s ",i,statenames[i]);
	 }
	 printf("\n");
	 return arg;
      }
   }

   if (at == Numeric) {
      arg++;

      if ((v->Number >= 0) && (v->Number < LrfscDrvrSTATES)) {
	 stat = v->Number;
	 if (ioctl(lrfsc,LrfscDrvrSET_STATE,&stat) < 0) {
	    IErr("SET_STATE",NULL);
	    return arg;
	 }
      } else printf("Illegal state value\n");
   }

   stat = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_STATE,&stat) < 0) {
      IErr("GET_STATE", NULL);
      return arg;
   }

   printf("State:%d=%s\n",(int) stat, statenames[(int) stat]);
   return arg;
}

/**********************************************************************/

static char *signames[LrfscDrvrDiagSIGNALS] = {"Reflected","Forward","Cavity","CavityError","Output"};

int GetSetDiagChoice(int arg) {	/* Diag_number, choice */
	
ArgVal   *v;
AtomType  at;

LrfscDrvrDiagChoices dchoice;
LrfscDrvrDiagSignalChoice schoice;
int i, diag, sval;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("Signals:");
	 for (i=0; i<LrfscDrvrDiagSIGNALS; i++) {
		 printf("%d=%s ",i,signames[i]);
	 }
	 printf("\n");
	 return arg;
      }
   }

   diag = -1;
   if (at == Numeric) {
      arg++;
      if ((v->Number >=0) && (v->Number < LrfscDrvrDiagSIGNAL_CHOICES)) {
	 diag = v->Number;
	 v = &(vals[arg]);
	 at = v->Type;
      } else {
	 printf("Illegal Diagnostic Index:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrDiagSIGNAL_CHOICES);
	 return arg;
      }
   }

   sval = -1;
   schoice = LrfscDrvrDiagREFLECTED;
   if (at == Numeric) {
      arg++;
      if ((v->Number >= 0) && (v->Number < LrfscDrvrDiagSIGNALS)) {
	 schoice = (LrfscDrvrDiagSignalChoice) v->Number;
	 sval = (int) schoice;
      } else {
	 printf("Illegal Signal choice:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrDiagSIGNALS);
	 return arg;
      }
   }

   bzero((void *) &dchoice, sizeof(LrfscDrvrDiagChoices));
   if (ioctl(lrfsc,LrfscDrvrGET_DIAG_CHOICE,&dchoice) < 0) {
      IErr("GET_DIAG_CHOICE", NULL);
      return arg;
   }

   if (sval != -1) {
      dchoice[diag] = schoice;
      if (ioctl(lrfsc,LrfscDrvrSET_DIAG_CHOICE,&dchoice) < 0) {
	 IErr("SET_DIAG_CHOICE",(int *) &diag);
	 return arg;
      }
   }

   for (i=0; i<LrfscDrvrDiagSIGNAL_CHOICES; i++)
      printf("Diag:%d %d=%s\n",i,dchoice[i],signames[dchoice[i]]);

   return arg;
}

/***************************************************************************/

int GetSetResCtrl(int arg) { /* Time, Value */

ArgVal   *v;
AtomType  at;
LrfscDrvrResCtrl resc;

   arg++;

   if (at == Numeric) {
      arg++;
      if (v->Number < 0xFFFF) {

	 if (ioctl(lrfsc,LrfscDrvrGET_RES_CTRL, &resc) < 0) {
	    IErr("GET_RES_CTRL", NULL);
	    return arg;
	 }
	 resc.Time = v->Number;

	 v = &(vals[arg]);
	 at = v->Type;
	 if (at == Numeric) {
	    arg++;
	    if (v->Number < 0xFFFF) {
	       resc.Value = v->Number;
	    } else {
	       printf("Illegal Control Value:%d Not in range[0..0xFFFF]\n",(int) v->Number);
	       return arg;
	    }
	 }

	 if (ioctl(lrfsc,LrfscDrvrSET_RES_CTRL, &resc) < 0) {
	    IErr("SET_RES_CTRL", NULL);
	    return arg;
	 }

      } else {
	 printf("Illegal Control Time:%d Not in range[0..0xFFFF]\n",(int) v->Number);
	 return arg;
      }
   }

   if (ioctl(lrfsc,LrfscDrvrGET_RES_CTRL, &resc) < 0) {
      IErr("GET_RES_CTRL", NULL);
      return arg;
   }

   printf("ResCtrlTime:%d ResCtrlValue:%d\n",(int) resc.Time, (int) resc.Value);
   printf("ResFwd:%s ResCav:%s\n",IQToStr(resc.Fwd),IQToStr(resc.Cav));
   return arg;
}

/*******************************************************************************/

char *anlsnames[LrfscDrvrAnalogSWITCHES] = {"RefTest","FwdTest","CavTest","OutTest"};

int GetSetAnalogueSwitch(int arg) {
	
ArgVal   *v;
AtomType  at;

LrfscDrvrAnalogSwitch anls;
int i, msk;
	
   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("BITS:AnalogueSwitch:");
	 for (i=0; i<LrfscDrvrAnalogSWITCHES; i++) {
	    msk = 1<<i;
	    printf("0x%02X=%s ",msk,anlsnames[i]);
	 }
	 printf("\n");
	 return arg;
      }
   }

   if (at == Numeric) {
      arg++;
      if (v->Number <= 0x0F) {
	 anls = v->Number;
	 if (ioctl(lrfsc,LrfscDrvrSET_ANALOGUE_SWITCH,&anls) < 0) {
	    IErr("SET_ANALOGUE_SWITCH",(int *) &anls);
	    return arg;
	 }
      }
   }

   anls = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_ANALOGUE_SWITCH,&anls) < 0) {
      IErr("GET_ANALOGUE_SWITCH",NULL);
      return arg;
   }
   printf("BITS:AnalogueSwitch:0x%02X\n",(int) anls);
   for (i=0; i<LrfscDrvrAnalogSWITCHES; i++) {
      msk = 1<<i;
      printf("0x%02X=%s =>",msk,anlsnames[i]);
      if (msk & anls) printf("Test\n");
      else            printf("Normal\n");
   }
   return arg;
}
	
/*******************************************************************************/
	
int GetSetSoftSwitch(int arg) {

	
ArgVal   *v;
AtomType  at;

LrfscDrvrSoftSwitch sfts;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if (v->Number < LrfscDrvrSoftSWITCHES) {
	 sfts = v->Number;
	 if (ioctl(lrfsc,LrfscDrvrSET_SOFT_SWITCH,&sfts) < 0) {
	    IErr("SET_SOFT_SWITCH",(int *) &sfts);
	    return arg;
	 }
      }
   }

   sfts = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_SOFT_SWITCH,&sfts) < 0) {
      IErr("GET_SOFT_SWITCH", NULL);
      return arg;
   }

   printf("SoftSwitch:");
   if (sfts == LrfscDrvrSotfSwitchMAIN_CLOSED) printf("Closed\n");
   else                                        printf("Open\n");

   return arg;
}

/**************************************************************************/

char *matnames[LrfscDrvrMatrixMATRICES] = {"Ref","Fwd","Cav"};

int GetSetCoefficients(int arg) { /* ? Matrix, A, B, C, D */

ArgVal   *v;
AtomType  at;

LrfscDrvrMatrixCoefficientsBuf cobuf;
LrfscDrvrMatrixCoefficients    *matp;

int i;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("Matrix:");
	 for (i=0; i<LrfscDrvrMatrixMATRICES; i++) {
	    printf("%d=%s ",i,matnames[i]);
	 }
	 printf("\n");
	 return arg;
      }
   }


   bzero((void *) &cobuf, sizeof(LrfscDrvrMatrixCoefficientsBuf));
   matp = &(cobuf.Coeficients);

   if (at == Numeric) {
      arg++;
      if ((v->Number >=0) && (v->Number < LrfscDrvrMatrixMATRICES)) {
	 cobuf.Matrix = (LrfscDrvrMatrix) v->Number;

	 if (ioctl(lrfsc,LrfscDrvrGET_COEFFICIENTS,&cobuf) < 0) {
	    IErr("GET_COEFFICIENTS",NULL);
	    return arg;
	 }

	 v = &(vals[arg]);
	 at = v->Type;

	 if (at == Numeric) {
	    arg++;

	    matp->MatrixA = v->Number;

	    v = &(vals[arg]);
	    at = v->Type;

	    if (at == Numeric) {
	       arg++;

	       matp->MatrixB = v->Number;

	       v = &(vals[arg]);
	       at = v->Type;

	       if (at == Numeric) {
		  arg++;

		  matp->MatrixC = v->Number;

		  v = &(vals[arg]);
		  at = v->Type;

		  if (at == Numeric) {
		     arg++;

		     matp->MatrixD = v->Number;

		     v = &(vals[arg]);
		     at = v->Type;
		  }
	       }
	    }

	    if (ioctl(lrfsc,LrfscDrvrSET_COEFFICIENTS,&cobuf) < 0) {
	       IErr("SET_COEFFICIENTS",NULL);
	       return arg;
	    }
	 }
      }
   }

   for (i=0; i<LrfscDrvrMatrixMATRICES; i++) {
      cobuf.Matrix = (LrfscDrvrMatrix) i;
      if (ioctl(lrfsc,LrfscDrvrGET_COEFFICIENTS,&cobuf) < 0) {
	 IErr("GET_COEFFICIENTS",NULL);
	 return arg;
      }
      printf("Matrix:%06d:%4ss A:%06d B:%06d C:%06d D:%06d\n",
	     i,
	     matnames[i],
	     matp->MatrixA,
	     matp->MatrixB,
	     matp->MatrixC,
	     matp->MatrixD);
   }
   return arg;
}

/*****************************************************************************/

int GetSetSnapShot(int arg) { /* time */

ArgVal   *v;
AtomType  at;

LrfscDrvrDiagSnapShot snap;


   arg++;
   v = &(vals[arg]);
   at = v->Type;

   bzero((void *) &snap, sizeof(LrfscDrvrDiagSnapShot));

   if (at == Numeric) {
      snap.DiagTime = (unsigned short) v->Number;
      if (ioctl(lrfsc,LrfscDrvrSET_SNAP_SHOT_TIME,&snap) < 0) {
	 IErr("SET_SNAP_SHOT_TIME",(int *) &(v->Number));
	 return arg;
      }
   }

   if (ioctl(lrfsc,LrfscDrvrGET_SNAP_SHOTS,&snap) < 0) {
      IErr("GET_SNAP_SHOT_TIME",(int *) &(v->Number));
      return arg;
   }

   printf("SnapShotTime:%d\n",(int) snap.DiagTime);
   printf("RefDiag:%s\n", IQToStr(snap.RefDiag));
   printf("FwdDiag:%s\n", IQToStr(snap.FwdDiag));
   printf("CavDiag:%s\n", IQToStr(snap.CavDiag));
   printf("ErrDiag:%s\n", IQToStr(snap.ErrDiag));
   printf("OutDiag:%s\n", IQToStr(snap.OutDiag));

   printf("POutPic:%s\n", IQToStr(snap.POutDiag));
   printf("IOutPic:%s\n", IQToStr(snap.IOutDiag));

   return arg;
}

/*****************************************************************************/

int GetSetPic(int arg) { /* KP KI */

ArgVal   *v;
AtomType  at;

LrfscDrvrPicSetBuf pic;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   bzero((void *) &pic, sizeof(LrfscDrvrPicSetBuf));

   if (ioctl(lrfsc,LrfscDrvrGET_PIC,&pic) < 0) {
      IErr("GET_PIC",NULL);
      return arg;
   }

   if (at == Numeric) {
      arg++;
      pic.KP = (signed short) v->Number;
      v = &(vals[arg]);
      at = v->Type;
      if (at == Numeric) {
	 arg++;
	 pic.KI = (signed short) v->Number;
      }
      if (ioctl(lrfsc,LrfscDrvrSET_PIC,&pic) < 0) {
	 IErr("SET_PIC",NULL);
	 return arg;
      }
   }

   if (ioctl(lrfsc,LrfscDrvrGET_PIC,&pic) < 0) {
      IErr("GET_PIC",NULL);
      return arg;
   }

   printf("Pic:KI:%d KP:%d\n",(int) pic.KI, (int) pic.KP);
   return arg;
}

/**************************************************************************/

int GetSetPulseLength(int arg) { /* MaxPulseLen */

ArgVal   *v;
AtomType  at;

unsigned long pl, plmx;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      plmx =  v->Number;
      if (ioctl(lrfsc,LrfscDrvrSET_MAX_PULSE_LENGTH,&plmx) < 0) {
	 IErr("SET_MAX_PULSE_LENGTH",(int *) &plmx);
	 return arg;
      }
   }

   if (ioctl(lrfsc,LrfscDrvrGET_MAX_PULSE_LENGTH,&plmx) < 0) {
      IErr("GET_MAX_PULSE_LENGTH",(int *) &plmx);
      return arg;
   }

   if (ioctl(lrfsc,LrfscDrvrGET_PULSE_LENGTH,&plmx) < 0) {
      IErr("GET_MAX_PULSE_LENGTH",(int *) &pl);
      return arg;
   }

   printf("PulseLength:Max:%d RFOff:%d\n",(int) plmx, (int) pl);

   return arg;
}

/***********************************************************************/

static long cyc = 0;

int GetSetCycle(int arg) { /* NextCycle */

ArgVal   *v;
AtomType  at;

unsigned long pcy, ncy;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if ((v->Number >= 0) && (v->Number < LrfscDrvrCYCLES)) {
	 cyc = ncy = v->Number;
	 if (ioctl(lrfsc,LrfscDrvrSET_NEXT_CYCLE,&ncy) < 0) {
	    IErr("LrfscDrvrSET_NEXT_CYCLE",(int *) &ncy);
	    return arg;
	 }

	 printf("Cycle:Next:%d\n",(int) ncy);

      } else {
	 printf("Illegal cycle number:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrCYCLES);
	 return arg;
      }
   }

   if (ioctl(lrfsc,LrfscDrvrGET_PRES_CYCLE,&pcy) < 0) {
      IErr("GET_PRES_CYCLE",NULL);
      return arg;
   }

   printf("Cycle:Pres:%d\n",(int) pcy);

   return arg;
}

/********************************************************************************/

/* Not called by CLI directly */

static LrfscDrvrConfigBuf cbuf;
#define SET_POINTS "ConfigSetPoints"
#define FEED_FORWARD "ConfigFeedFwd"

static int WriteConfig(int arg) { /* cyc [filename] */

ArgVal   *v;
AtomType  at;

FILE *fp;
char *cp;

char fnam[LN];
int i;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if ((v->Number >= 0) && (v->Number < LrfscDrvrCYCLES)) {
	 cyc = v->Number;

	 cbuf.Cycle = cyc;
	 cbuf.Points = LrfscDrvrCONFIG_POINTS;

	 if (ioctl(lrfsc,LrfscDrvrGET_CYCLE_CONFIG,&cbuf) < 0) {
	    IErr("GET_CYCLE_CONFIG",(int *) &cyc);
	    return arg;
	 }

      } else {
	 printf("Illegal cycle number:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrCYCLES);
	 return arg;
      }
   }

   cp = GetFileName(&arg);
   if (cp == NULL) {
      if (cbuf.Which == LrfscDrvrConfigSETPOINTS) cp = GetFile(SET_POINTS);
      else                                        cp = GetFile(FEED_FORWARD);
      if (cp == NULL) {
	 printf("Couldn't locate file: %s\n",cp);
	 return arg;
      }
   }

   sprintf(fnam,"%s.%02d", cp, (int) cyc);
   umask(0);

   fp = fopen(fnam,"w");
   if (fp) {

      for (i=0; i<LrfscDrvrCONFIG_POINTS; i++) {
	 fprintf(fp,"%05d %05d %05d\n",
		 cbuf.Array[i].Ticks,
		 cbuf.Array[i].IQ.I,
		 cbuf.Array[i].IQ.Q);
      }
   }
   fclose(fp);

   printf("Written data to:%s\n",fnam);

   return arg;
}

/********************************************************************************/

int WriteSetFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigSETPOINTS;
   return WriteConfig(arg);
}

/********************************************************************************/

int WriteFwdFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigFEEDFORWARD;
   return WriteConfig(arg);
}

/*****************************************************************/

/* Not called by CLI directly */

static int EditConfig(int arg) { /* cyc [filename] */

ArgVal   *v;
AtomType  at;

char *cp;
char cmbuf[LN], tmbuf[LN];

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if ((v->Number >= 0) && (v->Number < LrfscDrvrCYCLES)) {
	 cyc = v->Number;
      } else {
	 printf("Illegal cycle number:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrCYCLES);
	 return arg;
      }
   }

   sprintf(tmbuf,"%s ",GetFile(editor));

   cp = GetFileName(&arg);
   if (cp == NULL) {
      if (cbuf.Which == LrfscDrvrConfigSETPOINTS) cp = GetFile(SET_POINTS);
      else                                        cp = GetFile(FEED_FORWARD);
      if (cp == NULL) {
	 printf("Couldn't locate file: %s\n",cp);
	 return arg;
      }
   }

   sprintf(cmbuf,"%s %s.%02d",tmbuf , cp, (int) cyc);

   system(cmbuf);
   printf("\n");
   return arg;
}

/********************************************************************************/

int EditSetFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigSETPOINTS;
   return EditConfig(arg);
}

/********************************************************************************/

int EditFwdFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigFEEDFORWARD;
   return EditConfig(arg);
}

/********************************************************************************/

/* Not called by CLI directly */

static int ReadConfig(int arg) { /* cyc [filename] */

ArgVal   *v;
AtomType  at;

FILE *fp;
char *cp;

char fnam[LN];
int i;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      if ((v->Number >= 0) && (v->Number < LrfscDrvrCYCLES)) {
	 cyc = v->Number;
	 cbuf.Cycle = cyc;
	 cbuf.Points = LrfscDrvrCONFIG_POINTS;
      } else {
	 printf("Illegal cycle number:%d Not in range[0..%d]\n",(int) v->Number, LrfscDrvrCYCLES);
	 return arg;
      }
   }

   cp = GetFileName(&arg);
   if (cp == NULL) {
      if (cbuf.Which == LrfscDrvrConfigSETPOINTS) cp = GetFile(SET_POINTS);
      else                                        cp = GetFile(FEED_FORWARD);
      if (cp == NULL) {
	 printf("Couldn't locate file: %s\n",cp);
	 return arg;
      }
   }

   sprintf(fnam,"%s.%02d", cp, (int) cyc);
   umask(0);

   fp = fopen(fnam,"r");
   if (fp) {

      for (i=0; i<LrfscDrvrCONFIG_POINTS; i++) {
	 fscanf(fp,
		"%d %d %d\n",
		(int *) (unsigned short *) &(cbuf.Array[i].Ticks),
		(int *) (signed   short *) &(cbuf.Array[i].IQ.I ),
		(int *) (signed   short *) &(cbuf.Array[i].IQ.Q ));
      }
   }
   fclose(fp);

   if (YesNo(fnam, "Write Hardware")) {
      if (ioctl(lrfsc,LrfscDrvrSET_CYCLE_CONFIG,&cbuf) < 0) {
	 IErr("SET_CYCLE_CONFIG",(int *) &cyc);
	 return arg;
      }
   }
   printf("Done\n");

   return arg;
}

/********************************************************************************/

int ReadSetFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigSETPOINTS;
   return ReadConfig(arg);
}

/********************************************************************************/

int ReadFwdFile(int arg) { /* cyc [filename] */

   bzero((void *) &cbuf, sizeof(LrfscDrvrConfigBuf));
   cbuf.Which = LrfscDrvrConfigFEEDFORWARD;
   return ReadConfig(arg);
}

/********************************************************************************/

int NextCycle(int arg) {

   arg++;
   cyc++;
   if (cyc >= LrfscDrvrCYCLES) cyc = 0;
   printf("Cycle:%02d\n",(int) cyc);
   return arg;
}

/********************************************************************************/

int PrevCycle(int arg) {

   arg++;
   cyc--;
   if (cyc < 0) cyc = LrfscDrvrCYCLES;
   printf("Cycle:%02d\n",(int) cyc);
   return arg;
}

/********************************************************************************/

static unsigned long SampStart  = 0;
static unsigned long SampSkip   = 0;
static unsigned long SampPulse  = LrfscDrvrPulse1;

int SetSampling(int arg) { /* Start, Skip, Pulse */

ArgVal   *v;
AtomType  at;

unsigned long plen;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Numeric) {
      arg++;
      SampStart = v->Number;
      v = &(vals[arg]);
      at = v->Type;
      if (at == Numeric) {
	 arg++;
	 SampSkip = v->Number;
	 v = &(vals[arg]);
	 at = v->Type;
	 if (at == Numeric) {
	    arg++;
	    SampPulse = v->Number;
	    v = &(vals[arg]);
	    at = v->Type;
	 }
      }
   }

   if (ioctl(lrfsc,LrfscDrvrSET_SKIP_START,&SampStart) < 0) {
      IErr("LrfscDrvrSET_SKIP_START",(int *) &SampStart);
      return arg;
   }

   if (SampSkip == 0) {
      if (ioctl(lrfsc,LrfscDrvrGET_PULSE_LENGTH,&plen) < 0) {
	 IErr("GET_PULSE_LENGTH",NULL);
	 return arg;
      }
      SampSkip = plen / (LrfscDrvrBUF_IQ_ENTRIES * LrfscDrvrTICK_TO_PAIR);
   }

   if (ioctl(lrfsc,LrfscDrvrSET_SKIP_COUNT,&SampSkip) < 0) {
      IErr("LrfscDrvrSET_SKIP_COUNT",(int *) &SampSkip);
      return arg;
   }

   if (ioctl(lrfsc,LrfscDrvrSET_PULSE,&SampPulse) < 0) {
      IErr("SET_PULSE",(int *) &SampPulse);
      return arg;
   }

   printf("Sample start:%d set\n", (int) SampStart);
   printf("Sample skip :%d set\n", (int) SampSkip);
   printf("Sample pulse:%d set\n", (int) SampPulse);
   printf("RFPulse len :%d    \n", (int) plen);

   return arg;
}

/********************************************************************************/

int ConvertDacValue(short val) {

int upr, lwr;

   upr = ((val & 0x03FF) - 512) << 4;
   lwr = ((val & 0x3C00) >> 10);
   return (upr | lwr);
}

/********************************************************************************/

int PlotDiag(int arg) { /* ? Signal xrange */

ArgVal   *v;
AtomType  at;

LrfscDrvrDiagBuf dbuf;

FILE *fpI, *fpQ, *fpp;
char *cp;

char fnamI[LN], fnamQ[LN], plot[3*LN], tmpb[LN];
int i, simulate, xl, xh;
unsigned int tm;

   arg++;
   v = &(vals[arg]);
   at = v->Type;

   if (at == Operator) {
      if (v->OId == OprNOOP) {
	 arg++;
	 v = &(vals[arg]);
	 at = v->Type;

	 printf("Signals:");
	 for (i=0; i<LrfscDrvrDiagSIGNALS; i++) {
	    printf("%d=%s ",i,signames[i]);
	 }
	 printf("\n");
	 return arg;
      }
   }

   dbuf.Choice = LrfscDrvrDiagCAVITY;
   if (at == Numeric) {
      arg++;

      if (v->Number >= LrfscDrvrDiagSIGNALS) {
	 printf("No such signal:%d\n",(int) v->Number);
	 return arg;
      }
      dbuf.Choice = v->Number;

      v = &(vals[arg]);
      at = v->Type;
   }

   xl = -1;
   if (at == Numeric) {
      arg++;
      xl = v->Number;
      v = &(vals[arg]);
      at = v->Type;
   }
   xh = -1;
   if (at == Numeric) {
      arg++;
      xh = v->Number;
      v = &(vals[arg]);
      at = v->Type;
   }

   dbuf.Size   = LrfscDrvrBUF_IQ_ENTRIES;
   dbuf.Pulse  = SampPulse;
   dbuf.Choice = v->Number;
   dbuf.Cycle  = cyc;
   dbuf.Valid  = 1;

   simulate = 0;
   if (ioctl(lrfsc,LrfscDrvrGET_DIAGNOSTICS,&dbuf) < 0) {
      IErr("GET_DIAGNOSTICS",NULL);
      dbuf.SkipStart = 0;
      dbuf.SkipCount = 1;
      dbuf.Valid     = 1;
      simulate       = 1;
   }

   printf("Diagnostic: Size:%d Pulse:%d Choice:%s Cycle:%d Valid:%d\n",
	  (int) dbuf.Size,
	  (int) dbuf.Pulse,
		signames[dbuf.Choice],
	  (int) dbuf.Cycle,
	  (int) dbuf.Valid);

   cp = GetFile(signames[dbuf.Choice]);

   if (dbuf.Valid) {
      sprintf(fnamI,"%sI.%02d.%1d",cp, (int) dbuf.Cycle, (int) dbuf.Pulse);
      sprintf(fnamQ,"%sQ.%02d.%1d",cp, (int) dbuf.Cycle, (int) dbuf.Pulse);
      umask(0);
      fpI = fopen(fnamI,"w");
      fpQ = fopen(fnamQ,"w");
      if (fpI) {
	 for (i=0; i<dbuf.Size; i++) {
	    tm = dbuf.SkipStart + i*dbuf.SkipCount;
	    if (simulate) {
	       fprintf(fpI,"%06d %06d\n",tm,i>>1);
	       fprintf(fpQ,"%06d %06d\n",tm,i>>2);
	    } else {
	       fprintf(fpI,"%06d %06d\n",tm,ConvertDacValue(dbuf.Array[i].I));
	       fprintf(fpQ,"%06d %06d\n",tm,ConvertDacValue(dbuf.Array[i].Q));
	    }
	 }
	 fclose(fpI);
	 fclose(fpQ);
	 printf("Updated:%s\n",fnamI);
	 printf("Updated:%s\n",fnamQ);

	 bzero((void *) tmpb, LN);
	 if (xh != -1) {
	    sprintf(tmpb,"set xrange[%d:%d]\n",xl,xh);
	 }

	 sprintf(plot,"%splot \"%s\" with line,\"%s\" with line\npause -1\n",tmpb,fnamI,fnamQ);
	 printf("%s",plot);
	 cp = GetFile("Plot");
	 if (cp) {
	    fpp = fopen(cp,"w");
	    if (fpp) {
	       fprintf(fpp,"%s",plot);
	       fclose(fpp);
	       GnuPlot();
	    }
	 }

      }
   } else
      printf("No valid diagnostics available\n");
   return arg;
}
