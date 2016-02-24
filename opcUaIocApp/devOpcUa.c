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

#include <stdio.h>
#include <string.h>
#include <errlog.h>

// #EPICS LIBS
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "epicsExport.h"
#include <epicsTypes.h>
#include "devSup.h"
#include "recSup.h"
#include "recGbl.h"
#include "aiRecord.h"
#include "aaiRecord.h"
#include "aoRecord.h"
#include "aaoRecord.h"
#include "biRecord.h"
#include "boRecord.h"
#include "longinRecord.h"
#include "longoutRecord.h"
#include "stringinRecord.h"
#include "stringoutRecord.h"
#include "mbbiRecord.h"
#include "mbboRecord.h"
#include "mbbiDirectRecord.h"
#include "mbboDirectRecord.h"
#include "asTrapWrite.h"
#include "alarm.h"
#include "asDbLib.h"
#include "cvtTable.h"
#include "menuFtype.h"
#include "menuAlarmSevr.h"
#include "menuAlarmStat.h"
//#define GEN_SIZE_OFFSET
//#include "waveformRecord.h"
//#undef  GEN_SIZE_OFFSET

#include <devOpcUa.h>
#include <drvOpcUa.h>

int debug_level(dbCommon *prec) {
    OPCUA_ItemINFO * p = (OPCUA_ItemINFO *) prec->dpvt;
    if(p)
        return p->debug;
    else
        return 0;
}
//#define DEBUG_LEVEL debug
#define DEBUG_LEVEL debug_level((dbCommon*)prec)
//#define DEBUG_LEVEL printf( ":  dpvt %p\n",prec->dpvt )
static OPCUA_ItemINFO *head = NULL;
int onceFlag  = 0;
static  long         read(dbCommon *prec);
static  long         write(dbCommon *prec);
static  void         outRecordCallback(CALLBACK *pcallback);
static  long         get_ioint_info(int cmd, dbCommon *prec, IOSCANPVT * ppvt);

//extern int OpcUaInitItem(char *OpcUaName, dbCommon* pRecord, OPCUA_ItemINFO** pOPCUA_ItemINFO);
//extern void checkOpcUaVariables(void);

/*+**************************************************************************
 *		DSET functions
 **************************************************************************-*/
long init (int after);

static long init_longout  (struct longoutRecord* plongout);
static long write_longout (struct longoutRecord* plongout);

static long init_longin (struct longinRecord* pmbbid);
static long read_longin (struct longinRecord* pmbbid);
static long init_mbbiDirect (struct mbbiDirectRecord* pmbbid);
static long read_mbbiDirect (struct mbbiDirectRecord* pmbbid);
static long init_mbboDirect (struct mbboDirectRecord* pmbbid);
static long write_mbboDirect (struct mbboDirectRecord* pmbbid);
static long init_mbbi (struct mbbiRecord* pmbbid);
static long read_mbbi (struct mbbiRecord* pmbbid);
static long init_mbbo (struct mbboRecord* pmbbod);
static long write_mbbo (struct mbboRecord* pmbbod);
static long init_bi  (struct biRecord* pbi);
static long read_bi (struct biRecord* pbi);
static long init_bo  (struct boRecord* pbo);
static long write_bo (struct boRecord* pbo);
static long init_ai (struct aiRecord* pai);
static long read_ai (struct aiRecord* pai);
static long init_ao  (struct aoRecord* pao);
static long write_ao (struct aoRecord* pao);
static long init_airaw (struct aiRecord* pai);
static long read_airaw (struct aiRecord* pai);
static long init_aoraw  (struct aoRecord* pao);
static long write_aoraw (struct aoRecord* pao);
static long init_stringin (struct stringinRecord* pstringin);
static long read_stringin (struct stringinRecord* pstringin);
static long init_stringout  (struct stringoutRecord* pstringout);
static long write_stringout (struct stringoutRecord* pstringout);

typedef struct {
   long number;
   DEVSUPFUN report;
   DEVSUPFUN init;
   DEVSUPFUN init_record;
   DEVSUPFUN get_ioint_info;
   DEVSUPFUN write_record;
}OpcUaDSET;

OpcUaDSET devlongoutOpcUa =    {5, NULL, init, init_longout, get_ioint_info, write_longout  };
epicsExportAddress(dset,devlongoutOpcUa);

OpcUaDSET devlonginOpcUa =     {5, NULL, init, init_longin, get_ioint_info, read_longin	 };
epicsExportAddress(dset,devlonginOpcUa);

OpcUaDSET devmbbiDirectOpcUa = {5, NULL, init, init_mbbiDirect, get_ioint_info, read_mbbiDirect};
epicsExportAddress(dset,devmbbiDirectOpcUa);

OpcUaDSET devmbboDirectOpcUa = {5, NULL, init, init_mbboDirect, get_ioint_info, write_mbboDirect};
epicsExportAddress(dset,devmbboDirectOpcUa);

OpcUaDSET devmbbiOpcUa = {5, NULL, init, init_mbbi, get_ioint_info, read_mbbi};
epicsExportAddress(dset,devmbbiOpcUa);

OpcUaDSET devmbboOpcUa = {5, NULL, init, init_mbbo, get_ioint_info, write_mbbo};
epicsExportAddress(dset,devmbboOpcUa);

OpcUaDSET devbiOpcUa = {5, NULL, init, init_bi, get_ioint_info, read_bi};
epicsExportAddress(dset,devbiOpcUa);

OpcUaDSET devboOpcUa = {5, NULL, init, init_bo, get_ioint_info, write_bo};
epicsExportAddress(dset,devboOpcUa);

OpcUaDSET devstringinOpcUa = {5, NULL, init, init_stringin, get_ioint_info, read_stringin};
epicsExportAddress(dset,devstringinOpcUa);

OpcUaDSET devstringoutOpcUa = {5, NULL, init, init_stringout, get_ioint_info, write_stringout};
epicsExportAddress(dset,devstringoutOpcUa);

struct aidset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: (-1,0)=>(failure,success)
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	read_ai;    	    // 2 => success, don't convert)
                        // if convert then raw value stored in rval
	DEVSUPFUN	special_linconv;
} devaiOpcUa =         {6, NULL, init, init_ai, get_ioint_info, read_ai, NULL };
epicsExportAddress(dset,devaiOpcUa);

struct aodset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: 2=> success, no convert
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	write_ao;   	    //(0)=>(success )
	DEVSUPFUN	special_linconv;
} devaoOpcUa =         {6, NULL, init, init_ao, get_ioint_info, write_ao, NULL };
epicsExportAddress(dset,devaoOpcUa);

struct airdset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: (-1,0)=>(failure,success)
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	read_ai;    	    // 2 => success, don't convert)
                        // if convert then raw value stored in rval
	DEVSUPFUN	special_linconv;
} devairawOpcUa =         {6, NULL, init, init_airaw, get_ioint_info, read_airaw, NULL };
epicsExportAddress(dset,devairawOpcUa);

struct aordset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: 2=> success, no convert
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	write_ao;   	    //(0)=>(success )
	DEVSUPFUN	special_linconv;
} devaorawOpcUa =         {6, NULL, init, init_aoraw, get_ioint_info, write_aoraw, NULL };
epicsExportAddress(dset,devaorawOpcUa);

/*
static long init_waveformRecord();
static long read_sa();
struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_Record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} devwaveformOpcUa = {
    6,
    NULL,
    NULL,
    init_waveformRecord,
    get_ioint_info,
    read_sa,
    NULL
};
epicsExportAddress(dset,devwaveformOpcUa);
*/
OPCUA_ItemINFO *getHead() { return head;}
void setHead(OPCUA_ItemINFO *h) {head = h;}

/*+**************************************************************************
 *		Defines and Locals
 **************************************************************************-*/
long init (int after)
{

    if( after) {
            if( !onceFlag ) {
                onceFlag = 1;
//                OpcUaAddSubs(head);
            }
    }
    return 0;
}

long init_common (dbCommon *prec, int isInput, struct link* plnk, int recType, void *val)
{
    OPCUA_ItemINFO* pOPCUA_ItemINFO;
    int n=0;

    if(!prec) {                                                                                     
        recGblRecordError(S_db_notFound, prec,"Fatal error: init_record record has NULL-pointer");
        getchar();                                                                                  
        return S_db_notFound;
    }                                                                                               
    if(!plnk->type) {
        recGblRecordError(S_db_badField, prec,"Fatal error: init_record INP field not initialized (It has value 0!!!)");
        getchar();                                                                                  
        return S_db_badField;
    }                                                                                               
    if(plnk->type != INST_IO) {                                                                 
        recGblRecordError(S_db_badField, prec,"init_record Illegal INP field (INST_IO expected");
        return S_db_badField;                                                                       
    }                                                                                               

    pOPCUA_ItemINFO =  (OPCUA_ItemINFO *) calloc(1,sizeof(OPCUA_ItemINFO));
    if(strlen(plnk->value.instio.string) < ITEMPATHLEN)
        n = sscanf(plnk->value.instio.string,"%s",*(&(pOPCUA_ItemINFO->ItemPath)));

    if(n != 1)
    {
        recGblRecordError(S_db_badField, prec,"devOpcUa (init_record) INP field is not correct\n");
      return -1;
    }
//    printf("init_common %s debug=%i pOPCUA_ItemINFO=%p\n",prec->name,pOPCUA_ItemINFO->debug,pOPCUA_ItemINFO);
    prec->dpvt = (void *) pOPCUA_ItemINFO;
    pOPCUA_ItemINFO->recDataType=recType;
    pOPCUA_ItemINFO->pRecVal = val;
    pOPCUA_ItemINFO->prec = prec;
    pOPCUA_ItemINFO->debug = prec->tpro;
    if(pOPCUA_ItemINFO->debug >= 2) errlogPrintf("%s: init_common, PACT= %i\n",prec->name,prec->pact);
    OpcUaRECdatatype(pOPCUA_ItemINFO);

    if(isInput) {
        if(prec->scan <  SCAN_IO_EVENT) {
                recGblRecordError(S_db_badField, prec, "init_record Illegal SCAN field");
        	return S_db_badField;
        }
    	if(prec->scan == SCAN_IO_EVENT) {                                                               
                scanIoInit(&(pOPCUA_ItemINFO->ioscanpvt));
                if(pOPCUA_ItemINFO->debug >= 2)
                    errlogPrintf("%s initcommon: ioscanpvt=%p\n",prec->name,pOPCUA_ItemINFO->ioscanpvt);
                pOPCUA_ItemINFO->isCallback = 1;
                OpcUaAddSubs(pOPCUA_ItemINFO);
        } else {
                pOPCUA_ItemINFO->isCallback = 0;
                pOPCUA_ItemINFO->ioscanpvt  = 0;
        }
    }                                                                                               
    else {

        pOPCUA_ItemINFO->isCallback = 1;                                                                     \
        pOPCUA_ItemINFO->isOutRecord = 1;
        callbackSetCallback(outRecordCallback, &(pOPCUA_ItemINFO->callback));                                \
        callbackSetUser(prec, &(pOPCUA_ItemINFO->callback));                                                 \
        OpcUaAddSubs(pOPCUA_ItemINFO);
    }
    return 0;
}

/***************************************************************************
    	    	    	    	Longin Support
 **************************************************************************-*/
long init_longin (struct longinRecord* prec)
{
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsInt32T,(void*)&(prec->val));
}

long read_longin (struct longinRecord* prec)
{
    if(DEBUG_LEVEL >= 2) errlogPrintf("%s\tread_longin\n",prec->name);
    return read((dbCommon*)prec);
}

/***************************************************************************
    	    	    	    	Longout Support
 ***************************************************************************/
long init_longout( struct longoutRecord* prec)
{
    if(DEBUG_LEVEL >= 2) errlogPrintf("%s\tinit_longout\n",prec->name);
    return init_common((dbCommon*)prec,0,&(prec->out),epicsInt32T,(void*)&(prec->val));
}

long write_longout (struct longoutRecord* prec)
{
    if(DEBUG_LEVEL >= 2) errlogPrintf("%s\twrite_longout\n",prec->name);
    return write((dbCommon*)prec);
}

/*+**************************************************************************
    	    	    	    	MbbiDirect Support
 **************************************************************************-*/
long init_mbbiDirect (struct mbbiDirectRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsUInt32T,(void*)&(prec->rval));
}

long read_mbbiDirect (struct mbbiDirectRecord* prec)
{
    long status = read((dbCommon*)prec);
    if( !status)
    	prec->rval &= prec->mask;
    return status;
}

/***************************************************************************
    	    	    	    	mbboDirect Support
 ***************************************************************************/
long init_mbboDirect( struct mbboDirectRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,0,&(prec->out),epicsUInt32T,(void*)&(prec->rval));
}

long write_mbboDirect (struct mbboDirectRecord* prec)
{
    return write((dbCommon*)prec);
}
/*+**************************************************************************
    	    	    	    	Mbbi Support
 **************************************************************************-*/
long init_mbbi (struct mbbiRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsUInt32T,(void*)&(prec->rval));
}

long read_mbbi (struct mbbiRecord* prec)
{
    long status=0;
    
    status = read((dbCommon*)prec);
    if( !status)
    	prec->rval &= prec->mask;
    return status;
}

/***************************************************************************
    	    	    	    	mbbo Support
 ***************************************************************************/
long init_mbbo( struct mbboRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,0,&(prec->out),epicsUInt32T,(void*)&(prec->rval));
}

long write_mbbo (struct mbboRecord* prec)
{
    return write((dbCommon*)prec);
}

/*+**************************************************************************
    	    	    	    	Bi Support
 **************************************************************************-*/
long init_bi (struct biRecord* prec)
{
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsUInt32T,(void*)&(prec->rval));
}

long read_bi (struct biRecord* prec)
{
    if(DEBUG_LEVEL >= 2) errlogPrintf("%s\tread_bi\n",prec->name);
    return read((dbCommon*)prec);
}


/***************************************************************************
    	    	    	    	bo Support
 ***************************************************************************/
long init_bo( struct boRecord* prec)
{
    prec->mask=1;
    return init_common((dbCommon*)prec,0,&(prec->out),epicsUInt32T,(void*)&(prec->rval));
}

long write_bo (struct boRecord* prec)
{
    if(DEBUG_LEVEL >= 2) errlogPrintf("%s\twrite_bo\n",prec->name);
    return write((dbCommon*)prec);
}

/*+**************************************************************************
    	    	    	    	ao Support
 **************************************************************************-*/
long init_ao (struct aoRecord* prec)
{
    return init_common((dbCommon*)prec,0,&(prec->out),epicsFloat64T,(void*)&(prec->oval));
}

long write_ao (struct aoRecord* prec)
{
    return write((dbCommon*)prec);
}
/***************************************************************************
    	    	    	    	ai Support
 **************************************************************************-*/
long init_ai (struct aiRecord* prec)
{
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsFloat64T,(void*)&(prec->val));
}

long read_ai (struct aiRecord* prec)
{
    long status = 0;
    status = read((dbCommon*)prec);
    if (status)
        return status;
    prec->udf = FALSE;	// aiRecord process doesn't set udf field in case of no convert!
    return 2;
}

/*+**************************************************************************
    	    	    	    	ao raw Support
 **************************************************************************-*/
long init_aoraw (struct aoRecord* prec)
{
    return init_common((dbCommon*)prec,0,&(prec->out),epicsInt32T,(void*)&(prec->rval));
}

long write_aoraw (struct aoRecord* prec)
{
    return write((dbCommon*)prec);
}

/***************************************************************************
    	    	    	    	airaw Support
 **************************************************************************-*/
long init_airaw (struct aiRecord* prec)
{
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsInt32T,(void*)&(prec->rval));
}

long read_airaw (struct aiRecord* prec)
{
    return read((dbCommon*)prec);
}

/***************************************************************************
    	    	    	    	Stringin Support
 **************************************************************************-*/
long init_stringin (struct stringinRecord* prec)
{
    return init_common((dbCommon*)prec,1,&(prec->inp),epicsOldStringT,(void*)&(prec->val));
}

long read_stringin (struct stringinRecord* prec)
{
    long ret = read((dbCommon*)prec);
    if( !ret )
        prec->udf = FALSE;	// stringinRecord process doesn't set udf field in case of no convert!
    return ret;
}

/***************************************************************************
    	    	    	    	Stringout Support
 ***************************************************************************/
long init_stringout( struct stringoutRecord* prec)
{
    return init_common((dbCommon*)prec,0,&(prec->out),epicsStringT,(void*)&(prec->val));
}

long write_stringout (struct stringoutRecord* prec)
{
    return write((dbCommon*)prec);
}

/***************************************************************************
    	    	    	    	Waveform Support
 **************************************************************************-*/
/*long init_waveformRecord(struct waveformRecord* precord) {
    char* OpcUaItemName = 0;
    OPCUA_ItemINFO* pOpcUa2Epics = 0;
    int iInit = 0;
    if(!precord) {
        recGblRecordError(S_db_notFound, precord,
            "Fatal error: init_record record has NULL-pointer");
        return(S_db_notFound);
    }
    if( precord->inp.type == 0 ) {
        recGblRecordError(S_db_badField, (void*)precord,
            "Fatal error: init_record INP field not initialized (It has value 0!!!)");
        precord->pact = TRUE;     // disable this record
        getchar();
        return(S_db_badField);
    }
    if( precord->inp.type != INST_IO ) {
        recGblRecordError(S_db_badField, (void*)precord,
            "init_record Illegal INP field (INST_IO expected: "
            "check following in your db-file: field(DTYP,\"OpcUa\";field(IN,\"@OpcUaVar\")");
        precord->pact = TRUE;     // disable this record
        return S_db_badField;
    }
    if( precord->scan <  SCAN_IO_EVENT  ) {
        recGblRecordError(S_db_badField, (void*)precord, "init_record Illegal SCAN field");
        precord->pact = TRUE;     // disable this record
        return S_db_badField;
    }
    OpcUaItemName = precord->inp.value.instio.string;
    iInit = OpcUaInitItem(OpcUaItemName,(dbCommon*)precord,&pOpcUa2Epics);
    if(!pOpcUa2Epics) {
        recGblRecordError(S_db_noMemory,precord,"OpcUa-item interface has not been not initialized!");
        return S_db_noMemory;
    }
    pOpcUa2Epics->pRecVal=precord->bptr;
    pOpcUa2Epics->recType=waveformval;
    pOpcUa2Epics->nelm = precord->nelm;
    pOpcUa2Epics->nord = & precord->nord;
    precord->dpvt = (void*)pOpcUa2Epics;
    if(precord->scan == SCAN_IO_EVENT) {
        scanIoInit(&(pOpcUa2Epics->ioscanpvt));
        pOpcUa2Epics->isCallback = 1;
    } else {
        pOpcUa2Epics->isCallback = 0;
        pOpcUa2Epics->ioscanpvt  = 0;
    }
    if(!iInit) {
        precord->pact = TRUE;     // disable this record
        return S_dev_Conflict;
    }
    return 0;
}

long read_sa(struct waveformRecord *prec) {
    OPCUA_ItemINFO* pOpcUa2Epics = (OPCUA_ItemINFO*)prec->dpvt;
    long ret = recProp[pOpcUa2Epics->recType].ret;
    if(!pOpcUa2Epics) {
        if(DEBUG_LEVEL >= 2) errlogPrintf("%s: read error", (void *)prec);
        prec->pact = TRUE;     // disable this record
        return ret;
    }
    if(prec->pact)
        return ret;
    if(OpcUaGetScalar(pOpcUa2Epics))
        return ret;
    prec->pact = FALSE;
    prec->udf  = FALSE;
    return ret;
}
*/

/* callback service routine */
static void outRecordCallback(CALLBACK *pcallback) {
    dbCommon *prec;  
    callbackGetUser(prec, pcallback);
    if(prec) {
        if(DEBUG_LEVEL >= 2)
            errlogPrintf("%s\tcallb: dbProcess\n", prec->name);
        dbProcess(prec);
        if(DEBUG_LEVEL >= 2) errlogPrintf("%s\toRCb|UDF:%i\n",prec->name,prec->udf);
    }
}

static long get_ioint_info(int cmd, dbCommon *prec, IOSCANPVT * ppvt) {
    if(!prec || !prec->dpvt)
        return 1;
    if(!cmd) {
        *ppvt = ((OPCUA_ItemINFO*)(prec->dpvt))->ioscanpvt;
        if(DEBUG_LEVEL >= 2)
            errlogPrintf("%s\tget_ioint_info ioscanpvt=%p\n",prec->name,*ppvt);
    }
    else
        ((OPCUA_ItemINFO*)(prec->dpvt))->isCallback = 0;
    return 0;
}

static long read(dbCommon * prec) {
    OPCUA_ItemINFO* pOPCUA_ItemINFO = (OPCUA_ItemINFO*)prec->dpvt;
    pOPCUA_ItemINFO->debug = prec->tpro;
    long ret = 0;
    if(DEBUG_LEVEL >= 2)
        errlogPrintf("%s\tread, UDF=%i noOut:=%i\n",prec->name,prec->udf,pOPCUA_ItemINFO->noOut);
    if(!pOPCUA_ItemINFO) {
        errlogPrintf("%s\tread error\n", prec->name);
        ret = 1;
    }
    else if(pOPCUA_ItemINFO->noOut == 1) {
        pOPCUA_ItemINFO->noOut = 0;
        if(DEBUG_LEVEL >= 2)
            errlogPrintf("read release noOut=%i\n",pOPCUA_ItemINFO->noOut);
    }
    else if(OpcUaReadItems(pOPCUA_ItemINFO)){
        ret = 1;
    }
    if(DEBUG_LEVEL >= 2)
        errlogPrintf("%s\tread return: %d\n", prec->name,(int)ret);
    return ret;
}

static long write(dbCommon *prec) {
    long ret = 0;
    OPCUA_ItemINFO* pOPCUA_ItemINFO = (OPCUA_ItemINFO*)prec->dpvt;
    pOPCUA_ItemINFO->debug = prec->tpro;

    if(DEBUG_LEVEL >= 2)
        errlogPrintf("%s\twrite, UDF:%i, noOut=%i\n",prec->name,prec->udf,pOPCUA_ItemINFO->noOut);

    if(!pOPCUA_ItemINFO) {
        if(DEBUG_LEVEL > 0)
            errlogPrintf("%s\twrite error\n", prec->name);
        ret = -1;
    }
    else if(pOPCUA_ItemINFO->noOut == 1) {
        pOPCUA_ItemINFO->noOut = 0;
        if(DEBUG_LEVEL >= 2) errlogPrintf("%s\twrite set noOut=%i\n",prec->name,pOPCUA_ItemINFO->noOut);
    }
    else {
        pOPCUA_ItemINFO->noOut = 1;
        prec->stat = OpcUaWriteItems(pOPCUA_ItemINFO);
        if(prec->stat != menuAlarmStatNO_ALARM)
            ret = -1;
    }
    
    if(ret==0) {
        prec->stat = menuAlarmStatNO_ALARM;
        prec->sevr = menuAlarmSevrNO_ALARM;
        prec->udf=FALSE;
    }
    else {
        prec->stat = menuAlarmStatNO_ALARM;
        prec->sevr = menuAlarmSevrINVALID;
    }
    return ret;
}
