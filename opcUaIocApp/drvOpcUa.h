/*+**************************************************************************
* Author:	Bernhard Kuner
 **************************************************************************-*/


#ifndef __DRVOPCUA_H
#define __DRVOPCUA_H


#include <devOpcUa.h>

#ifdef __cplusplus
extern "C" {
#endif

extern long OpcUaRECdatatype(OPCUA_ItemINFO *pOPCUA_ItemINFO);
extern long OpcUaAddSubs(OPCUA_ItemINFO *pOU_ItemINFO);
extern long opcUa_init(void);
extern long opcUa_io_report (void); /* Write IO report output to stdout. */
extern long OpcUaReadItems(OPCUA_ItemINFO* pOU_ItemINFO);
extern long OpcUaWriteItems(OPCUA_ItemINFO* pOU_ItemINFO);
#ifdef __cplusplus
//
}
#endif
#endif /* ifndef __DRVOPCUA_H */
