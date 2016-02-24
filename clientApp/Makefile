TOP=..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CXXFLAGS += -pthread -fomit-frame-pointer

PROD = client
SRCS += testClient.cpp # client.cpp# 
FREEOPCUA_PATH =/opt/freeopcua

USR_CPPFLAGS += -std=c++11 -ggdb
USR_SYS_LIBS += dl pthread xml2 boost_system 
USR_SYS_LIBS += opcuaclient opcuaprotocol opcuacore
USR_LDFLAGS  += -L$(FREEOPCUA_PATH)/lib

USR_INCLUDES += -I$(FREEOPCUA_PATH)/include
USR_INCLUDES += -I/usr/include/libxml2

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
