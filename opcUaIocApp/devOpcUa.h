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
