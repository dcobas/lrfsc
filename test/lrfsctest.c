/**************************************************************************/
/* Lrfsc test                                                             */
/* Julian Lewis Mon 9th June 2008                                         */
/**************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>        /* Error numbers */
#include <sys/file.h>
#include <a.out.h>
#include <ctype.h>
#include <tgm/tgm.h>
#include <math.h>

#include <lrfscdrvr.h>
#include <lrfscdrvrP.h>

#ifndef True
#define True 1
#endif

#ifndef False
#define False 0
#endif

/**************************************************************************/
/* Code section from here on                                              */
/**************************************************************************/

/* Print news on startup if not zero */

#define NEWS 1

#define HISTORIES 24
#define CMD_BUF_SIZE 128

static char history[HISTORIES][CMD_BUF_SIZE];
static char *cmdbuf = &(history[0][0]);
static int  cmdindx = 0;
static char prompt[32];
static char *pname = NULL;
static int  lrfsc;

#include "Cmds.h"
#include "GetAtoms.c"
#include "PrintAtoms.c"
#include "DoCmd.c"
#include "Cmds.c"
#include "LrfscOpen.c"
#include "LrfscCmds.c"

/**************************************************************************/
/* Prompt and do commands in a loop                                       */
/**************************************************************************/

int main(int argc,char *argv[]) {

	char *cp;
	char host[49];
	char tmpb[CMD_BUF_SIZE];

#if NEWS
	printf("lrfsctest: See <news> command\n");
	printf("lrfsctest: Type h for help\n");
#endif

	pname = argv[0];
	printf("%s: Compiled %s %s\n",pname,__DATE__,__TIME__);

	lrfsc = LrfscOpen();
	if (lrfsc == 0) {
		printf("\nWARNING: Could not open driver");
		printf("\n\n");
	} else {
		printf("Driver opened OK: Using LRFSC module: 1\n\n");
	}
	printf("\n");

	bzero((void *) host,49);
	gethostname(host,48);

	while (True) {

		if (lrfsc) sprintf(prompt,"%s:Lrfsc[%02d]",host,cmdindx+1);
		else     sprintf(prompt,"%s:NoDriver:Lrfsc[%02d]",host,cmdindx+1);

		cmdbuf = &(history[cmdindx][0]);
		if (strlen(cmdbuf)) printf("{%s} ",cmdbuf);
		printf("%s",prompt);

		bzero((void *) tmpb,CMD_BUF_SIZE);
		if (gets(tmpb)==NULL) exit(1);

		cp = &(tmpb[0]);
		if (*cp == '!') {
			cmdindx = strtoul(++cp,&cp,0) -1;
			if (cmdindx >= HISTORIES) cmdindx = 0;
			cmdbuf = &(history[cmdindx][0]);
			continue;
		} else if (*cp == '.') {
			printf("Execute:%s\n",cmdbuf); fflush(stdout);
		} else if ((*cp == '\n') || (*cp == '\0')) {
			cmdindx++;
			if (cmdindx >= HISTORIES) { printf("\n"); cmdindx = 0; }
			cmdbuf = &(history[cmdindx][0]);
			continue;
		} else if (*cp == '?') {
			printf("History:\n");
			printf("\t!<1..24> Goto command\n");
			printf("\tCR       Goto next command\n");
			printf("\t.        Execute current command\n");
			printf("\this      Show command history\n");
			continue;
		} else {
			cmdindx++; if (cmdindx >= HISTORIES) { printf("\n"); cmdindx = 0; }
			strcpy(cmdbuf,tmpb);
		}
		bzero((void *) val_bufs,sizeof(val_bufs));
		GetAtoms(cmdbuf);
		DoCmd(0);
	}
}
