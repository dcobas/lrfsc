/***************************************************************************/
/* Lrfsc installation program.                                             */
/* Julian Lewis Mon 9th June 2008                                          */
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

#include <lrfscdrvr.h>
#include <lrfscdrvrP.h>

#include <drvr_parser.h>

static int               modl_index;
static LrfscDrvrInfoTable info;

/***************************************************************************/
/* Main                                                                    */
/***************************************************************************/

int main(argc,argv)
int argc;
char **argv; {

char                   * object     = "lrfscdrvr.o";
char                   * progname   = "lrfscinstall";
char                   * info_file  = "/tmp/lrfscdrvr.info";
char                   * extra      = "Lrfsc";
char                     node_name[128];
int                      driver_id, major, minor;
int                      uid, fd, i;
DrvrModuleAddresses      madr;

   /* If we are not root, and no args supplied give help text and exit. */

   if (argc < 2) {
      printf("Usage: lrfscinstall options are:\n");
      printf("       -object <path> :: Override default object  path.\n");
      printf("\n");
      printf("       -M <address>   :: Module VME base A24 address.\n");
      printf("       -X <address>   :: Module VME JTAG A16 address.\n");
      printf("       -L <level>     :: Module VME interrupt  level.\n");
      printf("       -V <vector>    :: Module VME interrupt vector.\n");
      exit(0);
   }

   for (i=1; i<argc; i++) {
      if (strcmp(argv[i],"-object"  ) == 0) object = argv[i+1];
   }
   printf("lrfscinstall: Installing driver from: %s\n",object);

   uid = geteuid();
   if (uid != 0) {
      printf("%s: User ID is not ROOT.\n",progname);
      exit(1);
   }
   
   /***********************************/
   /* Build the info table in memory. */
   /***********************************/

   bzero((void *) &info,sizeof(LrfscDrvrInfoTable));       /* Clean table */

   DrvrParser(argc,argv,&madr);                  /* Parse command line */
   if (madr.Size < 1) {
      printf("%s: No module addresses supplied\n",progname);
      exit(1);
   }
   modl_index = madr.Size;
   info.Modules = modl_index;
   for (i=0; i<modl_index; i++) {
      info.Addresses[i].VMEAddress      = (unsigned short *) madr.Addresses[i].Address;
      info.Addresses[i].RamAddress      = (unsigned short *) (madr.Addresses[i].Address | LrfscDrvrSTART_OF_RAM);
      info.Addresses[i].InterruptVector = madr.Addresses[i].InterruptVector;
      info.Addresses[i].InterruptLevel  = madr.Addresses[i].InterruptLevel;
      printf("%s: Module: %d Address: %x Ram: %x Vec: %x L: %x\n",progname,
	     i+1,
	     (int) info.Addresses[i].VMEAddress,
	     (int) info.Addresses[i].RamAddress,
	     (int) info.Addresses[i].InterruptVector,
	     (int) info.Addresses[i].InterruptLevel);
   }

   /* Open the info file and write the table to it. */

   if ((fd = open(info_file,(O_WRONLY | O_CREAT),0644)) < 0) {
      printf("%s: Can't create Lrfsc driver information file: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }

   if (write(fd,&info,sizeof(info)) != sizeof(LrfscDrvrInfoTable)) {
      printf("%s: Can't write to Lrfsc driver information file: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }
   close(fd);

   /* Install the driver object code in the system and call its install */
   /* procedure to initialize memory and hardware. */

   if ((driver_id = dr_install(object,CHARDRIVER)) < 0) {
      printf("%s: Error calling dr_install with: %s\n",progname,object);
      perror(progname);
      exit(1);
   }
   if ((major = cdv_install(info_file, driver_id, extra)) < 0) {
      printf("%s: Error calling cdv_install with: %s\n",progname,info_file);
      perror(progname);
      exit(1);
   }

   /* Create the device nodes. */

   umask(0);
   for (minor=1; minor<=LrfscDrvrCLIENT_CONTEXTS; minor++) {
      sprintf(node_name,"/dev/lrfsc.%1d",minor);
      unlink (node_name);
      if (mknod(node_name,(S_IFCHR | 0666),((major << 8) | minor)) < 0) {
	 printf("%s: Error making node: %s\n",progname,node_name);
	 perror(progname);
	 exit(1);
      }
   }

   exit(0);
}
