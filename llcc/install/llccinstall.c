/***************************************************************************/
/* Llcc installation program.                                              */
/* Feb 2003                                                                */
/* Ioan Kozsar                                                             */
/***************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <a.out.h>
#include <errno.h>

extern int dr_install(char *path, int type);
extern int cdv_install(char *path, int driver_ID, char *extra);

#include <tgm/con.h>

#include <llccdrvr.h>
#include <llccdrvrP.h>

#include <drvr_parser.h>

static int			modl_index;
static LlccDrvrInfoTable	info;

/***************************************************************************/
/* Main                                                                    */
/***************************************************************************/

int main(argc,argv)
int argc;
char **argv; {

char                   * object     = "./llccdrvr.o";
char                   * progname   = "llccinstall";
char                   * info_file  = "/dev/llccdrvr.info";
char                   * extra      = "Llcc";
char                     node_name[128];
int                      driver_id, major, minor;
int                      uid, fd, i;
DrvrModuleAddresses      addresses;

   /* If we are not root, and no args supplied give help text and exit. */

   if (argc < 2) {
      printf("Usage: llccinstall options are:\n");
      printf("       -object <path> :: Override default object path.\n");
      printf("\n");
      printf("       -M <address>   :: Module VME address.\n");
      printf("       -V <vector>    :: Module VME interrupt vector.\n");
      printf("       -L <level>     :: Module VME interrupt  level.\n");
      exit(0);
   }

   for (i=1; i<argc; i++)
      if (strcmp(argv[i],"-object" ) == 0) object    = argv[i+1];

   printf("llccinstall: Installing driver from: %s\n",object);

   uid = geteuid();
   if (uid != 0) {
      printf("%s: User ID is not ROOT.\n",progname);
      exit(1);
   }
   
   /***********************************/
   /* Build the info table in memory. */
   /***********************************/

   bzero((void *)&info,sizeof(LlccDrvrInfoTable));       /* Clean table */

   DrvrParser(argc,argv,&addresses);            /* Parse command line */
   if (addresses.Size < 1) {
      printf("%s: No module addresses supplied\n",progname);
      exit(1);
   }
   modl_index = addresses.Size;
   info.Modules = modl_index;
   for (i=0; i<modl_index; i++) {
      info.Addresses[i].VMEAddress	= (int *) addresses.Addresses[i].Address;
      info.Addresses[i].InterruptVector	= (unsigned short *) addresses.Addresses[i].InterruptVector;
      info.Addresses[i].InterruptLevel	= (unsigned short *) addresses.Addresses[i].InterruptLevel;
      printf("%s: Module: %d Address: %x Vec: %x L: %x\n",
      	     progname,
	     i+1,
	     (int) info.Addresses[i].VMEAddress,
	     (int) info.Addresses[i].InterruptVector,
	     (int) info.Addresses[i].InterruptLevel);
   }

   /* Open the info file and write the table to it. */

   if ((fd = open(info_file,(O_WRONLY | O_CREAT),0644)) < 0) {
      printf("%s: Can't create Llcc driver information file: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }

   if (write(fd,&info,sizeof(info)) != sizeof(LlccDrvrInfoTable)) {
      printf("%s: Can't write to Llcc driver information file: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }
   close(fd);
   
   printf("llccinstall: Done the info file\n");

   /* Install the driver object code in the system and call its install */
   /* procedure to initialize memory and hardware. */

   if ((driver_id = dr_install(object,CHARDRIVER)) < 0) {
      printf("%s: Error calling dr_install with: %s\n",progname,object);
      perror(progname);
      exit(1);
   }
   
   printf("llccinstall: Done the dr_install routine\n");
   
   if ((major = cdv_install(info_file, driver_id, extra)) < 0) {
      printf("%s: Error calling cdv_install with: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }
   
   printf("llccinstall: Done the cdv_install routine\n");

   /* Create the device nodes. */

   umask(0);
   for (minor=1; minor<=LlccDrvrCLIENT_CONTEXTS; minor++) {
      sprintf(node_name,"/dev/llcc.%1d",minor);
      unlink (node_name);
      if (mknod(node_name,(S_IFCHR | 0666),((major << 8) | minor)) < 0) {
	 printf("%s: Error making node: %s\n",progname,node_name);
	 perror(progname);
	 exit(1);
      }
   }
   
   printf("llccinstall: Done the node\n");

   exit(0);
}
