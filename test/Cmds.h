/**************************************************************************/
/* Command line stuff                                                     */
/**************************************************************************/

int Illegal();              /* llegal command */

int Quit();                 /* Quit test program  */
int Help();                 /* Help on commands   */
int News();                 /* Show GMT test news */
int History();              /* History            */
int Shell();                /* Shell command      */
int Sleep();                /* Sleep seconds      */
int Pause();                /* Pause keyboard     */
int Atoms();                /* Atom list commands */

int ChangeEditor();
int ChangeDirectory();
int Module();
int NextModule();
int PrevModule();
int SwDeb();
int GetSetTmo();
int ModIo();
int RamIo();
int Reset();
int GetVersion();
int WaitInterrupt();

int ReadStatus();
int GetSetState();          /* ? :State */
int GetSetDiagChoice();     /* ? :Diag_number, choice */
int GetSetResCtrl();        /* ResonanceControl:Time, Value */
int GetSetAnalogueSwitch(); /* ? Analogue Switch: mask */
int GetSetSoftSwitch();     /* Soft switch: OnOff */

int GetSetCoefficients();  /* ? Matrix, A, B, C, D */
int GetSetSnapShot();      /* time */
int GetSetPic();           /* KP KI */
int GetSetPulseLength();   /* MaxPulseLen */
int GetSetCycle();         /* NextCycle */
int NextCycle();           /* Increment Cycle */
int PrevCycle();           /* Decrement Cycle */
int WriteSetFile();        /* cyc [filename] */
int WriteFwdFile();        /* cyc [filename] */
int EditSetFile();         /* cyc [filename] */
int EditFwdFile();         /* cyc [filename] */
int ReadSetFile();         /* cyc [filename] */
int ReadFwdFile();         /* cyc [filename] */
int SetSampling();         /* Set up ISR sampling */
int PlotDiag();            /* Write diagnostics file and plot */

static void IErr();

FILE *inp;

typedef enum {

   CmdNOCM,    /* llegal command */

   CmdQUIT,    /* Quit test program  */
   CmdHELP,    /* Help on commands   */
   CmdNEWS,    /* Show GMT test news */
   CmdHIST,    /* History            */
   CmdSHELL,   /* Shell command      */
   CmdSLEEP,   /* Sleep seconds      */
   CmdPAUSE,   /* Pause keyboard     */
   CmdATOMS,   /* Atom list commands */

   CmdCHNGE,   /* Change editor to next program */
   CmdCD,      /* Change working directory */
   CmdMODULE,  /* Module selection */
   CmdNEXT,    /* Next module */
   CmdPREV,    /* Previous module */
   CmdDEBUG,   /* Get Set debug mode */
   CmdTMO,     /* Timeout control */

   CmdMODIO,   /* Moduls register IO */
   CmdRAMIO,   /* Ram IO */
   CmdRESET,   /* Reset module */
   CmdVER,     /* Get versions */
   CmdWINT,    /* Wait for interrupt */

   CmdRST,     /* Read Status */
   CmdSTATE,   /* Get/Set state */
   CmdDIAGC,   /* Get/Set diag Choice */
   CmdRESC,    /* Get/Set resonance control */
   CmdANSW,    /* Get/Set analogue switch */
   CmdSOFTSW,  /* Get/Set soft whitch */

   CmdMATCO,   /* Get/Set Matrix coefficients */
   CmdSNAP,    /* Get/Set Snap shot */
   CmdPIC,     /* Get/Set PI controller */
   CmdPLEN,    /* Get/Set pulse length */
   CmdCYC,     /* Get/Set cycle */
   CmdNCYC,    /* Increment cycle */
   CmdPCYC,    /* Decrement cycle */
   CmdSETWF,   /* Write setpoints file */
   CmdFWDWF,   /* Write feed forward file */
   CmdSETED,   /* Edit setpoints file */
   CmdFWDED,   /* Edit feed forward file */
   CmdSETRF,   /* Read setpoints file */
   CmdFWDRF,   /* Read feed forward file */

   CmdSAMP,    /* Set ISR sampling */
   CmdPLOT,    /* Plot diagnostics */

   CmdCMDS } CmdId;

typedef struct {
   CmdId  Id;
   char  *Name;
   char  *Help;
   char  *Optns;
   int  (*Proc)(); } Cmd;

static Cmd cmds[CmdCMDS] = {

   { CmdNOCM,   "???",    "Illegal command"          ,""           ,Illegal },

   { CmdQUIT,    "q" ,    "Quit test program"        ,""           ,Quit  },
   { CmdHELP,    "h" ,    "Help on commands"         ,""           ,Help  },
   { CmdNEWS,    "news",  "Show GMT test news"       ,""           ,News  },
   { CmdHIST,    "his",   "History"                  ,""           ,History},
   { CmdSHELL,   "sh",    "Shell command"            ,"UnixCmd"    ,Shell },
   { CmdSLEEP,   "s" ,    "Sleep seconds"            ,"Seconds"    ,Sleep },
   { CmdPAUSE,   "z" ,    "Pause keyboard"           ,""           ,Pause },
   { CmdATOMS,   "a" ,    "Atom list commands"       ,""           ,Atoms },

   { CmdCHNGE,   "ce",    "Change text editor used"  ,""           ,ChangeEditor },
   { CmdCD,      "cd",    "Change working directory" ,""           ,ChangeDirectory },
   { CmdMODULE,  "mo",    "Select/Show modules"      ,"Module"     ,Module },
   { CmdNEXT,    "nm",    "Next module"              ,""           ,NextModule },
   { CmdPREV,    "pm",    "Previous module"          ,""           ,PrevModule },
   { CmdDEBUG,   "deb",   "Get/Set driver debug mode","1|2|4/0"    ,SwDeb },
   { CmdTMO,     "tmo",   "Timeout control"          ,"Timeout"    ,GetSetTmo },

   { CmdMODIO,   "mio",   "Module Registers IO"      ,"Address"     ,ModIo},
   { CmdRAMIO,   "rio",   "RAM IO"                   ,"Address"     ,RamIo},
   { CmdRESET,   "reset", "Reset module"             ,""            ,Reset },
   { CmdVER,     "ver",   "Get versions"             ,""            ,GetVersion },
   { CmdWINT,    "wi",    "Wait for interrupt"       ,""            ,WaitInterrupt },

   { CmdRST,     "rst",   "Read status"               ,""           ,ReadStatus },
   { CmdSTATE,   "state", "Get/Set state"             ,"?|state"    ,GetSetState },
   { CmdDIAGC,   "diagc", "Get/Set diagnostic Choice" ,"?|diag,chce",GetSetDiagChoice },
   { CmdRESC,    "resc",  "Get/Set resonance control" ,"time,value" ,GetSetResCtrl },
   { CmdANSW,    "answ",  "Get/Set analogue switch"   ,"?|msk"      ,GetSetAnalogueSwitch },
   { CmdSOFTSW,  "sftsw", "Get/Set soft switch"       ,"0|1"        ,GetSetSoftSwitch },

   { CmdMATCO,   "matco", "Get/Set Matrix coefficients" ,"?|0|1|2|3" ,GetSetCoefficients },
   { CmdSNAP,    "snap",  "Get/Set Snap shot"           ,"time"      ,GetSetSnapShot },
   { CmdPIC,     "pic",   "Get/Set PI controller"       ,"KI,KP"     ,GetSetPic },
   { CmdPLEN,    "plen",  "Get/Set pulse length"        ,"Max"       ,GetSetPulseLength },
   { CmdCYC,     "cyc",   "Get/Set cycle"               ,"Cyc"       ,GetSetCycle },
   { CmdNCYC,    "incyc", "Increment cycle"             ,""          ,NextCycle },
   { CmdPCYC,    "decyc", "Decrement cycle"             ,""          ,PrevCycle },
   { CmdSETWF,   "setwf", "Write setpoints file"        ,"Cyc,Path"  ,WriteSetFile },
   { CmdFWDWF,   "fwdwf", "Write feed forward file"     ,"Cyc,Path"  ,WriteFwdFile },
   { CmdSETED,   "setef", "Edit setpoints file"         ,"Cyc,Path"  ,EditSetFile },
   { CmdFWDED,   "fwdef", "Edit feed forward file"      ,"Cyc,Path"  ,EditFwdFile },
   { CmdSETRF,   "setrf", "Read setpoints file"         ,"Cyc,Path"  ,ReadSetFile },
   { CmdFWDRF,   "fwdrf", "Read feed forward file"      ,"Cyc,Path"  ,ReadFwdFile },

   { CmdSAMP,    "samp",  "Set ISR sampling parameters" ,"Str,Skp,Pls",SetSampling },
   { CmdPLOT,    "pwf",   "Plot write file"             ,"?|sig,xl,xh",PlotDiag }

 };

typedef enum {

   OprNOOP,

   OprNE,  OprEQ,  OprGT,  OprGE,  OprLT , OprLE,   OprAS,
   OprPL,  OprMI,  OprTI,  OprDI,  OprAND, OprOR,   OprXOR,
   OprNOT, OprNEG, OprLSH, OprRSH, OprINC, OprDECR, OprPOP,
   OprSTM,

   OprOPRS } OprId;

typedef struct {
   OprId  Id;
   char  *Name;
   char  *Help; } Opr;

static Opr oprs[OprOPRS] = {
   { OprNOOP, "?"  ,"Not an operator"       },
   { OprNE,   "#"  ,"Not equal"             },
   { OprEQ,   "="  ,"Equal"                 },
   { OprGT,   ">"  ,"Greater than"          },
   { OprGE,   ">=" ,"Greater than or equal" },
   { OprLT,   "<"  ,"Less than"             },
   { OprLE,   "<=" ,"Less than or equal"    },
   { OprAS,   ":=" ,"Becomes equal"         },
   { OprPL,   "+"  ,"Add"                   },
   { OprMI,   "-"  ,"Subtract"              },
   { OprTI,   "*"  ,"Multiply"              },
   { OprDI,   "/"  ,"Divide"                },
   { OprAND,  "&"  ,"AND"                   },
   { OprOR,   "!"  ,"OR"                    },
   { OprXOR,  "!!" ,"XOR"                   },
   { OprNOT,  "##" ,"Ones Compliment"       },
   { OprNEG,  "#-" ,"Twos compliment"       },
   { OprLSH,  "<<" ,"Left shift"            },
   { OprRSH,  ">>" ,"Right shift"           },
   { OprINC,  "++" ,"Increment"             },
   { OprDECR, "--" ,"Decrement"             },
   { OprPOP,  ";"  ,"POP"                   },
   { OprSTM,  "->" ,"PUSH"                  } };

static char atomhash[256] = {
  10,9,9,9,9,9,9,9,9,0,0,9,9,0,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  0 ,1,9,1,9,4,1,9,2,3,1,1,0,1,11,1,5,5,5,5,5,5,5,5,5,5,1,1,1,1,1,1,
  10,6,6,6,6,6,6,6,6,6,6,6,6,6,6 ,6,6,6,6,6,6,6,6,6,6,6,6,7,9,8,9,6,
  9 ,6,6,6,6,6,6,6,6,6,6,6,6,6,6 ,6,6,6,6,6,6,6,6,6,6,6,6,9,9,9,9,9,
  9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
  9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9 ,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9 };

typedef enum {
   Seperator=0,Operator=1,Open=2,Close=3,Comment=4,Numeric=5,Alpha=6,
   Open_index=7,Close_index=8,Illegal_char=9,Terminator=10,Bit=11,
 } AtomType;

#define MAX_ARG_LENGTH  32
#define MAX_ARG_COUNT   16
#define MAX_ARG_HISTORY 16

typedef struct {
   int      Pos;
   int      Number;
   AtomType Type;
   char     Text[MAX_ARG_LENGTH];
   CmdId    CId;
   OprId    OId;
} ArgVal;

static int pcnt = 0;
static ArgVal val_bufs[MAX_ARG_HISTORY][MAX_ARG_COUNT];
static ArgVal *vals = val_bufs[0];

#if 0
#define True 1
#define False 0
#endif
