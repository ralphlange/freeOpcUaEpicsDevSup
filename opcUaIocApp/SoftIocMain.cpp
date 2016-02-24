/* SoftIocMain.cpp */
/* Author:  Marty Kraimer Date:    17MAR2000 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "epicsThread.h"
#include "iocsh.h"

int main(int argc,char *argv[])
{
/* Check for PIDFILE in environment and create PID file if set */
    FILE* pidfile;
    char* pidfilename = getenv("PIDFILE");

    if (pidfilename) {
        pidfile = fopen(pidfilename, "w");
        if (pidfile) {
            fprintf(pidfile, "%u\n", getpid());
            fclose(pidfile);
        } else {
            perror("Can't open PID file:");
        }
    }
    
    if(argc>=2) {
        iocsh(argv[1]);
        epicsThreadSleep(.2);
    }
    iocsh(NULL);
    return(0);
}
