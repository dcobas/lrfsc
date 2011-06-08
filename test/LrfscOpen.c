/**************************************************************************/
/* Open the Lrfsc driver                                                  */
/**************************************************************************/

#include <lrfscdrvr.h>

int LrfscOpen() {

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
