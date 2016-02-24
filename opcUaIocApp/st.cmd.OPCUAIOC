#!../../bin/linux-x86/OPCUAIOC

# Change to top directory
cd ..

################## Set up Environment ######################################### 

epicsEnvSet IOC OPCUAIOC
################## Register all support components ############################  

dbLoadDatabase "dbd/OPCUAIOC.dbd",0,0
${IOC}_registerRecordDeviceDriver pdbbase

################### Configure drivers ######################################### 

drvOpcUaSetup("opc.tcp://193.149.12.196:48010")
#drvOpcUaSetup("opc.tcp://hazel.acc.bessy.de:4880/Softing")
#drvOpcUaSetup("opc.tcp://cray.acc.bessy.de:4980/Softing dataFEED OPC Suite Configuration 1")

################### Load Databases ############################################ 

# Load IOC System Stats database (flat)
dbLoadRecords "db/ibh_ua.db"
#dbLoadRecords "db/Softing.db"
#dbLoadRecords "db/OPCUA_RECORD.db"


################### Configure IOC Core ######################################## 

# IOC Log Server Connection  0=enabled 1=disabled
setIocLogDisable 1

# Configure Access Security
#asSetFilename db/no_security.acf

################### Ignition... ############################################### 

iocInit
