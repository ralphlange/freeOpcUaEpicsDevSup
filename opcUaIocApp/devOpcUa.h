/*************************************************************************\
* Copyright (c) 2016 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH (HZB), Berlin, Germany.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
\*************************************************************************/

#ifndef INCdevOpcUaH
#define INCdevOpcUaH

//#include "ClientSession.h"
//#include "DataValue.h"
#include "dbScan.h"
#include "callback.h"
#include "epicsMutex.h"

#define ITEMPATHLEN 128
typedef struct OPCUA_Item {

  int NdIdx;                // no need in freeopcua
  int debug;                // debug level
  char ItemPath[ITEMPATHLEN];
  epicsBoolean isBool;
  epicsType itemDataType;   //OPCUA Datatype
  epicsType recDataType;    /* Data type of the records VAL/RVAL field */

  void *pRecVal;            /* point to records val field */
  int isCallback;           /* is IN-record with SCAN=IOINTR or any type of OUT record*/
  int isOutRecord;
  int noOut;            /* flag for OUT-records: prevent write back of incomming values */
  IOSCANPVT ioscanpvt;  /* in-records scan request.*/
  CALLBACK callback;    /* out-records callback request.*/

  dbCommon *prec;
  struct OPCUA_Item *next;
} OPCUA_ItemINFO;

#ifdef __cplusplus
extern "C" {
#endif
OPCUA_ItemINFO *getHead();
void setHead(OPCUA_ItemINFO *);
#ifdef __cplusplus
}
#endif

#endif
