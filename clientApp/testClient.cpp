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

#include "opc/ua/client/client.h"
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <csignal>

// checkDataTypw
#include <epicsTypes.h>
#include "../opcUaIocApp/devOpcUa.h"

std::string optionUsage = "client [OPTIONS] -u URL ITEMPATH1 ITEMPATH2 ..."
"OPTIONS:\n\n"
"  ITEMPATH2:   Path to data item, begin after ROOT/OBJECT, mark namespace by n:\n"
"               eg: '4:device.myvar'' or '3:device.subdev.5:myvar'\n"
"  -h :         This help\n"
"  -t POLLTIME: in ms\n"
"  -u URL:      Server URL\n"
"  -v :         Verbose\n"
"  -d :         Debug\n"
"  -m :         Monitor value\n"
"  -c :         Get children of last object in the path\n"
"\n";

using namespace OpcUa;

//std::string hostname;
std::string serverUrl;
bool verbose = false;
int pollTime = 100;
bool debug = false;
bool monitor = false;
bool children = false;
OpcUa::UaClient client(debug);

// Time resolution of DateTime is 100ns
inline int64_t getMsec(DateTime dateTime){ return (dateTime.Value % 10000000LL)/10000; }
inline int64_t getUsec(DateTime dateTime){ return (dateTime.Value % 10000000LL)/10; }

void signalHandler( int signum )
{
    std::cout << "exit" << std::endl;
    monitor=false;
}
/*
class SubClient : public SubscriptionHandler
{
  void DataChange(uint32_t handle, const Node& node, const Variant& val, AttributeId attr) override
  {
    std::cout << "Received DataChange event, value of Node " << node << " is now: "  << val.ToString() << std::endl;
  }
};
*/
class SubClient : public SubscriptionHandler
{

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
      std::cout << node //<< "Type: '" << ((int) val.Value.Type())
                << "\t"<<tsSrc->tm_hour<< ":"<<tsSrc->tm_min<< ":"<<tsSrc->tm_sec<< "."<< getMsec(val.ServerTimestamp)
                << "\t" << val.Value.ToString()
                << std::endl ;
  }
};

int getOptions(int argc, char *argv[]) {
    char c;
    while((c =  getopt(argc, argv, "cdvmhu:t:")) != EOF)
    {
        switch (c)
        {
        case 'v':
            verbose = true;
            break;
        case 'd':
            debug = true;
            break;
        case 'm':
            monitor = true;
            break;
        case 'c':
            children = true;
            break;
        case 'h':
            printf("%s",optionUsage.c_str());
            exit(0);
        case 'u':
            serverUrl = std::string(optarg);
            break;
        case 't':
            pollTime = atoi(optarg);
            break;
        default:
            printf("Unknown option '%c'\n",c);
        }
    }
    return 0;
}

void printDevPath(std::vector<std::string> &devpath) 
{
    std::cout << "device path is: ";
    for(std::string item : devpath)
        std::cout <<"'"<<item<<"' ";
    std::cout <<std::endl;
}

void showChildren(Node &node) 
{
        if(verbose) std::cout << "Children of objects node are: " << std::endl;
		for (OpcUa::Node nd : node.GetChildren()) {
            std::cout << "\t" << nd<<" qname: ";
			QualifiedName qName;
            qName = nd.GetBrowseName();
            std::cout  << qName.NamespaceIndex<<":" <<qName.Name<<std::endl;
        }
}
int main(int argc, char** argv)
{
    signal(SIGINT, signalHandler);
/*    char *url  = getenv("OPCUA_URL");
    if( url ) serverUrl= std::string(url);
    char host[30];
    int ret = gethostname(host,30);
    if( !ret ) hostname = std::string(host);
*/
    getOptions(argc, argv);
    if(monitor && children) {
        printf("I'm confused with both options: 'c' and 'm'");
        exit(1);
    }
    if(verbose) {
//        printf( ("Host:\t'%s'\n"),hostname.c_str());
        printf( ("URL:\t'%s'\n"),serverUrl.c_str());
    }
    boost::replace_all(serverUrl," ","%20");

    try
	{
        if(verbose) std::cout << "Connecting to: " << serverUrl << std::endl;
        client.Connect(serverUrl);

		//get Root node on server
		OpcUa::Node root = client.GetRootNode();
        if(verbose) std::cout << "Root node is: " << root << std::endl;
        showChildren(root);

		//get and browse Objects node
        //get a node from standard namespace using objectId
		std::cout << "NamespaceArray is: " << std::endl;
        OpcUa::Node nsnode = client.GetNode(ObjectId::Server_NamespaceArray);
        OpcUa::Variant ns = nsnode.GetValue();

		for (std::string d : ns.As<std::vector<std::string>>())
			std::cout << "    " << d << std::endl;

        // supposed to have item arguments
        if (optind < argc) {
            std::vector<std::string> rootpath{ "Objects" };
            OpcUa::Node myRoot = root.GetChild(rootpath);
            if(verbose) std::cout << "got myRoot: " << myRoot << std::endl;
            if(children)showChildren(myRoot);
            SubClient sclt;
            std::unique_ptr<Subscription> sub = client.CreateSubscription(pollTime, sclt);
            std::vector<uint32_t> subscriptionHandles;
            for(int idx=optind;idx<argc;idx++) {

                std::string itemPath = std::string(argv[idx]);
                OpcUa::Node myvar;
                std::vector<std::string> devpath; // parsed item path
                boost::split(devpath,itemPath,boost::is_any_of("."));
                if(verbose) printDevPath(devpath);
                myvar =     myRoot.GetChild(devpath);
                if(verbose) std::cout << "got node: " << myvar << std::endl;

                if(monitor) {
                    uint32_t handle = sub->SubscribeDataChange(myvar);
                    subscriptionHandles.push_back(handle);
                    if(verbose) std::cout << "Got sub handle: " << handle << std::endl;
                }
                else if(children) {
                    if(verbose) std::cout << "Got children of " << myvar << std::endl;
                    showChildren(myvar);
                }
                else {
                    OpcUa::DataValue val = myvar.GetDataValue();
                    printf("DONE: GetDataValue\n");
                    time_t tsS = DateTime::ToTimeT(val.ServerTimestamp);
                    printf("Timestamp ts=%d\n",(int)tsS);
                    tm  *tsSrc = localtime(&tsS);
                    std::cout << "Item is:" << myvar << "\t" << "Type: '" << ((int) val.Value.Type())
                          << "\t"<<tsSrc->tm_hour<< ":"<<tsSrc->tm_min<< ":"<<tsSrc->tm_sec<< "."<< getMsec(val.ServerTimestamp)
                          << "\t" << val.Value.ToString()
                          << std::endl ;
                }
            }

            do {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            while(monitor);
        }

        if(verbose) std::cout << "Disconnecting" << std::endl;
		client.Disconnect();
		return 0;
	}
	catch (const std::exception& exc)
	{
        std::cout <<"EXCEPTION: " << exc.what() << std::endl;
	}
	catch (...)
	{
		std::cout << "Unknown error." << std::endl;
	}
	return -1;
}

