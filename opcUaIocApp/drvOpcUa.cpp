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
#include <stdlib.h>
#include <limits.h> // ?
#include <set>      // ?
#include <string>   // ?
#include <map>


// #EPICS LIBS
#define epicsTypesGLOBAL
#include <epicsTypes.h>
#include <epicsPrint.h>
#include <epicsExport.h>
#include <registryFunction.h>
#include <dbCommon.h>
#include <devSup.h>
#include <drvSup.h>
#include <devLib.h>
#include <iocsh.h>

// #Softing LIBS
/// http://www.gnu.org/licenses/lgpl.html)
///

#include "opc/ua/client/client.h"
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>

#include <time.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <csignal>

#include <drvOpcUa.h>
#include <devOpcUa.h>

using namespace OpcUa;

static const char * const variantTypeStrings[] = {
"NUL",
"BOOLEAN",
"SBYTE",
"BYTE",
"INT16",
"UINT16",
"INT32",
"UINT32",
"INT64",
"UINT64",
"FLOAT",
"DOUBLE",
"STRING",
"DATE_TIME",
"GUId",
"BYTE_STRING",
"XML_ELEMENT",
"NODE_Id",
"EXPANDED_NODE_Id",
"STATUS_CODE",
"QUALIFIED_NAME",
"LOCALIZED_TEXT",
"EXTENSION_OBJECT",
"DATA_VALUE",
"VARIANT",
"DIAGNOSTIC_INFO"};

long setRecVal(const DataValue &readRes,OPCUA_ItemINFO* pOPCUA_ItemINFO);
void setTimestamp(dbCommon *prec, const DateTime &dt);

static std::unique_ptr<Subscription> sub;

inline int64_t getMsec(DateTime dateTime){ return (dateTime.Value % 10000000LL)/10000; }
class SubClient : public SubscriptionHandler
{
    // just to avoid the default message
    void DataChange(uint32_t handle, const Node& node, const Variant& val, AttributeId attribute) override
    {
        OPCUA_UNUSED(handle);
        OPCUA_UNUSED(node);
        OPCUA_UNUSED(val);
        OPCUA_UNUSED(attribute);
    }
    void DataValueChange(uint32_t handle, const Node& node, const DataValue& val, AttributeId attribute) override
    {
      time_t tsS = DateTime::ToTimeT(val.ServerTimestamp);
      tm  *tsSrc = localtime(&tsS);
      OPCUA_ItemINFO * pOPCUA_ItemINFO = (OPCUA_ItemINFO*) sub->getUsrPtr(handle);  // the unique and global subscription pointer
      if(pOPCUA_ItemINFO->debug >= 2)
        std::cout  << pOPCUA_ItemINFO->prec->name<<"\tDataValueChange: "<< node << variantTypeStrings[((int) val.Value.Type())]
                << "\t"<<tsSrc->tm_hour<< ":"<<tsSrc->tm_min<< ":"<<tsSrc->tm_sec<< "."<< getMsec(val.ServerTimestamp)
                << "\t" << val.Value.ToString()
                << std::endl ;
          if(pOPCUA_ItemINFO->debug >= 2)
              printf("\tisCallback=%i\n",pOPCUA_ItemINFO->isCallback);

          if(pOPCUA_ItemINFO->isCallback) {
              if( pOPCUA_ItemINFO->isOutRecord ) { // out-records are SCAN="passive" so scanIoRequest doesn't work
                  if(pOPCUA_ItemINFO->noOut == 0) {
                        pOPCUA_ItemINFO->noOut = 1;
                      if(pOPCUA_ItemINFO->debug >= 2)
                            printf("\tisOutREC=%i set cbRequest\n",pOPCUA_ItemINFO->isOutRecord);
                      setRecVal(val,pOPCUA_ItemINFO);
                      callbackRequest(&(pOPCUA_ItemINFO->callback));
                  }
                  else {
                      pOPCUA_ItemINFO->noOut = 0;
                      if(pOPCUA_ItemINFO->debug >= 2)
                        printf("\tnoOut is set -> reset noOut=0\n");
                  }
              }
              else {
                  pOPCUA_ItemINFO->noOut = 1; //noOut bei 1 bedeutet das Request angelegt wurde aber noch nicht prozessiert wurde
                  setTimestamp(pOPCUA_ItemINFO->prec,val.ServerTimestamp); // it's OPC-server time
                  setRecVal(val,pOPCUA_ItemINFO);
                  scanIoRequest( pOPCUA_ItemINFO->ioscanpvt );
                  if(pOPCUA_ItemINFO->debug >= 2)
                    printf("\tscanIoRequest: ioscanpvt=%p\n",pOPCUA_ItemINFO->ioscanpvt);
              }
          }

          else {
            if(pOPCUA_ItemINFO->debug >= 2)
                printf("\tisNOTCallback\n");
          }
    }
};

struct OpcItem {
    std::vector<std::string> devpath;

    OpcItem(char *path){
        std::string itemPath = std::string(path);
        boost::split(devpath,itemPath,boost::is_any_of("."));
        std::string object = std::string("Objects");
        std::vector<std::string>::iterator it;
        it = devpath.begin();
        devpath.insert(it,object);
   }
};
OpcUa::Node getNode(OPCUA_ItemINFO *pOPCUA_ItemINFO);
bool debug = false;
static OpcUa::UaClient client(debug);
int pollTime = 100;
static SubClient *sclt;
static std::vector<uint32_t> subscriptionHandles;
static std::vector<OpcItem *> opcItems;

extern "C" {
                                    /* DRVSET */
    long opcUa_io_report (void);
    long opcUa_init (void);

    struct {
        long      number;
        DRVSUPFUN report;
        DRVSUPFUN init;
    }  drvOpcUa = {
        2,
        opcUa_io_report,
        opcUa_init
    };
    epicsExportAddress(drvet,drvOpcUa);


    epicsRegisterFunction(opcUa_init);
    long opcUa_init(void)
    {

        return 0;
    }

    epicsRegisterFunction(opcUa_io_report);
    long opcUa_io_report (void) /* Write IO report output to stdout. */
    {
        return 0;
    }
}

long setRecVal(const DataValue &readRes,OPCUA_ItemINFO* pOPCUA_ItemINFO)
{
    bool            eBool;
    epicsInt8       I8val;  //0
    epicsUInt8      UI8val; //1
    epicsInt16      I16val; //2
    epicsUInt16     UI16val;//3
    epicsInt32      I32val; //5
    epicsUInt32     UI32val;//6
//    epicsInt64      I64val; // see eptcsTypes.h only if __STDC_VERSION__ >= 199901L
//    epicsUInt64     UI64val;//6
    epicsFloat32    F32val; //7
    epicsFloat64    F64val; //8
    std::string     STDString;
    const OpcUa::Variant &value = readRes.Value;
    if(pOPCUA_ItemINFO->debug >= 2)
        errlogPrintf( "%s: setRecVal %s to %s\n",pOPCUA_ItemINFO->prec->name,variantTypeStrings[(int)value.Type()],epicsTypeNames[pOPCUA_ItemINFO->recDataType]);
    switch (value.Type())
    {
    case VariantType::BOOLEAN:
        eBool = value.As<bool>();
        printf("BOOL: %d\n",eBool);
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)eBool;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)eBool;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)eBool;break;
        default: return 1;
        }
        break;
    case VariantType::BYTE:
        UI8val = value.As<unsigned char>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)UI8val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)UI8val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)UI8val;break;
        default: return 1;
        }
        break;
    case VariantType::SBYTE:
        I8val = value.As<char>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)I8val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)I8val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)I8val;break;
        default: return 1;
        }
        break;
    case VariantType::DOUBLE:
        F64val = value.As<double>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)F64val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)F64val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)F64val;break;
        default: return 1;
        }
        break;
    case VariantType::FLOAT:
        F32val= value.As<float>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)F32val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)F32val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)F32val;break;
        default: return 1;
        }
        break;
    case VariantType::INT16:
        I16val = value.As<int16_t>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)I16val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)I16val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)I16val;break;
        default: return 1;
        }
        break;
    case VariantType::INT32:
        I32val = value.As<int32_t>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)I32val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)I32val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)I32val;break;
        default: return 1;
        }
        break;
    case VariantType::UINT16:
        UI16val = value.As<uint16_t>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)UI16val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)UI16val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)UI16val;break;
        default: return 1;
        }
        break;
    case VariantType::UINT32:
        UI32val = value.As<uint32_t>();
        switch(pOPCUA_ItemINFO->recDataType){
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)UI32val;break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)UI32val;break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)UI32val;break;
        default: return 1;
        }
        break;
    case VariantType::BYTE_STRING:
        STDString = value.As<std::string>();
        switch(pOPCUA_ItemINFO->recDataType){ /* stringin/outRecord definition of 'char val[40]' */
        case epicsOldStringT: strncpy( (char *)(pOPCUA_ItemINFO->pRecVal),STDString.c_str(),39);break;
        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=atol(STDString.c_str());break;
        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=atol(STDString.c_str());break;
        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=atof(STDString.c_str());break;
        default: return 1;
        }
        break;
        /*        case VariantType::INT64:
                    I64val = value.As<int64_t>();
                    switch(pOPCUA_ItemINFO->recDataType){
                        case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)I64val;break;
                        case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)I64val;break;
                        case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)I64val;break;
                        default: return 1;
                    }
                    break;
    case VariantType::UINT64:
        UI64val = value.As<uint64_t>();
        switch(pOPCUA_ItemINFO->recDataType){
            case epicsInt32T:   *((epicsInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsInt32)UI64val;break;
            case epicsUInt32T:  *((epicsUInt32*)pOPCUA_ItemINFO->pRecVal)=(epicsUInt32)UI64val;break;
            case epicsFloat64T: *((epicsFloat64*)pOPCUA_ItemINFO->pRecVal)=(epicsFloat64)UI64val;break;
            default: return 1;
        }
        break;
*/        default:
        if(pOPCUA_ItemINFO->debug >= 2)
            errlogPrintf("Variant type %d (%s) not supported\n",value.Type(),variantTypeStrings[(int)value.Type()]);
        return -1;
        break;
    }
    return 0;
}

OpcUa::Node getNode(OPCUA_ItemINFO *pOPCUA_ItemINFO)
{
    std::vector<std::string> &devpath = opcItems[pOPCUA_ItemINFO->NdIdx]->devpath;
    std::string joined = boost::algorithm::join(devpath, ".");
    OpcUa::Node root = client.GetRootNode();
    OpcUa::Node node = root.GetChild(devpath);
    return node;
}

void setTimestamp(dbCommon *prec, const DateTime &dt)
{
    if(prec->tse == epicsTimeEventDeviceTime ) {
        prec->time.secPastEpoch = DateTime::ToTimeT(dt);;
        prec->time.nsec         = (dt.Value % 10000000LL)*100;
    }
//    else
//        prec->time = epicsTime::getCurrent();   // it's epics IOC time
}


epicsRegisterFunction(OpcUaWriteItems);
long OpcUaWriteItems(OPCUA_ItemINFO* pOPCUA_ItemINFO)
{
    bool            epicsBool;
    epicsInt8       I8val;  //0
    epicsUInt8      UI8val; //1
    epicsInt16      I16val; //2
    epicsUInt16     UI16val;//3
    epicsInt32      I32val; //5
    epicsUInt32     UI32val;//6
//    epicsInt64      I64val; // see eptcsTypes.h only if __STDC_VERSION__ >= 199901L
//    epicsUInt64     UI64val;//6
    epicsFloat32    F32val; //7
    epicsFloat64    F64val; //8
    std::string     STDString;
    std::string     recName;
    OpcUa::Variant varVal;

    recName = std::string(pOPCUA_ItemINFO->prec->name);
    std::cout<<pOPCUA_ItemINFO->prec->name<< ": OpcUaWriteItems"<< pOPCUA_ItemINFO->prec->tpro<<std::endl;
// Convert from EPICS type to datatyp of OPCUA Object,
    try {
        if(pOPCUA_ItemINFO->isBool==epicsTrue)
        {
            switch(pOPCUA_ItemINFO->recDataType){
            case epicsInt32T:   epicsBool = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
            case epicsUInt32T:  epicsBool = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
            case epicsFloat64T: epicsBool = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
            default: return 1;
            }
            varVal = OpcUa::Variant(epicsBool);

        }
        else{

            switch((int)pOPCUA_ItemINFO->itemDataType){
            case epicsInt8T:

                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   I8val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  I8val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: I8val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(I8val);
                break;
            case epicsUInt8T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   UI8val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  UI8val =  (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: UI8val =  (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                break;
                varVal = OpcUa::Variant(UI8val);
            case epicsInt16T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   I16val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  I16val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: I16val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(I16val);
                break;
            case epicsUInt16T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   UI16val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  UI16val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: UI16val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(UI16val);
                break;
            case epicsInt32T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   I32val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  I32val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: I32val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(I32val);
                break;
            case epicsUInt32T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   UI32val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  UI32val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: UI32val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(UI32val);
                break;
            case epicsFloat32T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   F32val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  F32val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: F32val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(F32val);
                break;
            case epicsFloat64T:
                switch(pOPCUA_ItemINFO->recDataType){//REC_Datatype(EPICS_Datatype)
                case epicsInt32T:   F64val = (*((epicsInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsUInt32T:  F64val = (*((epicsUInt32*)(pOPCUA_ItemINFO)->pRecVal));break;
                case epicsFloat64T: F64val = (*((epicsFloat64*)(pOPCUA_ItemINFO)->pRecVal));break;
                default: return 1;
                }
                varVal = OpcUa::Variant(F64val);
                break;
            case epicsOldStringT:
                if(pOPCUA_ItemINFO->recDataType == epicsOldStringT) { /* stringin/outRecord definition of 'char val[40]' */
                    strncpy( (char *)(pOPCUA_ItemINFO->pRecVal),STDString.c_str(),39);break;
                    varVal = OpcUa::Variant(STDString);
                }
                break;
            default:
                std::cout <<pOPCUA_ItemINFO->prec->name<< "\tOpcUaWriteItems ERROR: unsupported epics data type: "<< epicsTypeNames[pOPCUA_ItemINFO->recDataType]<< std::endl;
            }

        }

        OpcUa::DataValue dataVal = OpcUa::DataValue(varVal);
        std::cout <<"\tset Value: " << dataVal.Value.ToString() << std::endl;
        OpcUa::Node node = getNode(pOPCUA_ItemINFO);
        node.SetAttribute(AttributeId::Value,dataVal);
        pOPCUA_ItemINFO->prec->time = epicsTime::getCurrent();
    }
    catch (const std::exception& exc)
    {
        std::cout <<recName<< ": OpcUaWriteItems EXCEPTION: " << exc.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout <<recName<< ": OpcUaWriteItemsUnknown error." << std::endl;
    }
    return 0;
}

static const iocshArg drvOpcUaSetupArg0 = {"[URL]", iocshArgString};
static const iocshArg *const drvOpcUaSetupArg[1] = {&drvOpcUaSetupArg0};
iocshFuncDef drvOpcUaSetupFuncDef = {"drvOpcUaSetup", 1, drvOpcUaSetupArg};
void drvOpcUaSetup (const iocshArgBuf *args )
{
    char *Surl = args[0].sval;
    if(Surl == NULL)
    {
      errlogPrintf("drvOpcUaSetup: ABORT Missing Argument \"url\".\n");
    }
    std::string serverUrl = std::string(Surl);
    boost::replace_all(serverUrl," ","%20");
    try {
        client.Connect(serverUrl);
        std::cout <<"Conected with: "<<serverUrl<<std::endl;
        OpcUa::Node root = client.GetRootNode();
        sclt = new SubClient();
        std::vector<std::string> rootpath{ "Objects" };
        sub = client.CreateSubscription(pollTime, (*sclt));
    }
    catch (const std::exception& exc)
    {
        std::cout <<"drvOpcUaSetup EXCEPTION: " << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "drvOpcUaSetup Unknown error." << std::endl;
    }
    return;
}
epicsRegisterFunction(drvOpcUaSetup);

/*Get Opc-UA Datatype and set it as epicsType into the struct*/
epicsRegisterFunction(OpcUaRECdatatype);
long OpcUaRECdatatype(OPCUA_ItemINFO *pOPCUA_ItemINFO)
{
    if(pOPCUA_ItemINFO->debug >= 2)
        errlogPrintf("\n%s: OpcUaRECdatatyp\n\t%s\n",pOPCUA_ItemINFO->prec->name,pOPCUA_ItemINFO->ItemPath);

    try {
        OpcItem *oi = new OpcItem(pOPCUA_ItemINFO->ItemPath);
        opcItems.push_back(oi);
        pOPCUA_ItemINFO->NdIdx = opcItems.size()-1;
        OpcUa::Node node = getNode(pOPCUA_ItemINFO);
        if(pOPCUA_ItemINFO->debug >= 2)
            std::cout <<"\tNode: "<<node<<" Idx: "<<(pOPCUA_ItemINFO->NdIdx)<<std::endl;
        Variant value =  node.GetAttribute(AttributeId::Value);
        if(pOPCUA_ItemINFO->debug >= 2)
            std::cout <<"\tattr type "<<(int)value.Type()<<std::endl;
        pOPCUA_ItemINFO->isBool=epicsFalse;
        switch(value.Type())//try OPCUA internal Datatyp
        {
        case VariantType::BOOLEAN:
            pOPCUA_ItemINFO->isBool=epicsTrue;
            break;
        case VariantType::SBYTE:
            pOPCUA_ItemINFO->itemDataType=epicsInt8T;
            break;
        case VariantType::BYTE:
            pOPCUA_ItemINFO->itemDataType=epicsUInt8T;
            break;
        case VariantType::INT16:
            pOPCUA_ItemINFO->itemDataType=epicsInt16T;
            break;
        case VariantType::UINT16:
            pOPCUA_ItemINFO->itemDataType=epicsUInt16T;
            break;
        case VariantType::INT32:
            pOPCUA_ItemINFO->itemDataType=epicsInt32T;
            break;
        case VariantType::UINT32:
            pOPCUA_ItemINFO->itemDataType=epicsUInt32T;
            break;
        case VariantType::FLOAT:
            pOPCUA_ItemINFO->itemDataType=epicsFloat32T;
            break;
        case VariantType::DOUBLE:
            pOPCUA_ItemINFO->itemDataType=epicsFloat64T;
            break;
        case VariantType::BYTE_STRING:
            pOPCUA_ItemINFO->itemDataType=epicsStringT;
            break;
        default:
            errlogPrintf("%s\tUnsupported Type\n",pOPCUA_ItemINFO->prec->name);
            return -1;
        }
        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cout <<pOPCUA_ItemINFO->prec->name<<": OpcUaRECdatatyp EXCEPTION: " << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cout <<pOPCUA_ItemINFO->prec->name<< ": OpcUaRECdatatyp Unknown error." << std::endl;
    }
    return -1;

}

/*Add a subscription to the item and monitore it*/
epicsRegisterFunction(OpcUaAddSubs);
long OpcUaAddSubs(OPCUA_ItemINFO *pOPCUA_ItemINFO)
{

    try
    {
        if(pOPCUA_ItemINFO->debug >= 2)
            errlogPrintf( "%s: OpcUaAddSubs\n",pOPCUA_ItemINFO->prec->name);
        debug = true;
        OpcUa::Node node = getNode(pOPCUA_ItemINFO);
        if(pOPCUA_ItemINFO->debug >= 2)
            std::cout <<"\tNode: "<<node << std::endl;
        Variant value =  node.GetAttribute(AttributeId::Value);
        if(pOPCUA_ItemINFO->debug >= 2)
            std::cout <<"\tVALUE: "<<value.ToString() << std::endl;

        uint32_t handle = sub->SubscribeDataChange(node);
        sub->setUsrPtr(handle,(OpcUa::UserData*)pOPCUA_ItemINFO);
        subscriptionHandles.push_back(handle);
        if(pOPCUA_ItemINFO->debug >= 2)
            std::cout << "\tHandle is: " << handle <<"pOPCUA_ItemINFO * = "<< pOPCUA_ItemINFO<< std::endl;
        /*LIst*/
        pOPCUA_ItemINFO->next = getHead();
        setHead(pOPCUA_ItemINFO);
        debug = false;
        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cout  <<pOPCUA_ItemINFO->prec->name<< ": OpcUaAddSubs EXCEPTION: " << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cout  <<pOPCUA_ItemINFO->prec->name<< ": OpcUaAddSubs Unknown error." << std::endl;
    }
    return -1;
}

epicsRegisterFunction(OpcUaReadItems);
long OpcUaReadItems(OPCUA_ItemINFO* pOPCUA_ItemINFO)
{
    dbCommon *prec = pOPCUA_ItemINFO->prec;

    if(pOPCUA_ItemINFO->debug >= 2)
        errlogPrintf( "%s:\tOpcUaReadItems\n",pOPCUA_ItemINFO->prec->name);
    try {
        OpcUa::Node node = getNode(pOPCUA_ItemINFO);
        OpcUa::DataValue val = node.GetAttributeAsDataValue(AttributeId::Value);
        setTimestamp(prec,val.ServerTimestamp); // it's OPC-server time
        setRecVal(val,pOPCUA_ItemINFO);
    }
    catch (const std::exception& exc)
    {
        std::cout <<"OpcUaReadItems EXCEPTION: " << exc.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "OpcUaReadItems Unknown error." << std::endl;
    }

    return 0;
}

//create a static object to make shure that opcRegisterToIocShell is called on beginning of
class OpcRegisterToIocShell
{
public :
        OpcRegisterToIocShell(void);
        ~OpcRegisterToIocShell()
        {
            for(uint32_t handle: subscriptionHandles)
                sub->UnSubscribe(handle);
            client.Disconnect();
        };
};

OpcRegisterToIocShell::OpcRegisterToIocShell(void)
{
      iocshRegister(&drvOpcUaSetupFuncDef, drvOpcUaSetup);
      //
}
static OpcRegisterToIocShell opcRegisterToIocShell;

