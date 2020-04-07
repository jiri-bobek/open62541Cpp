#include <iostream>
#include <open62541server.h>
#include "testmethod.h"
#include <serverrepeatedcallback.h>

namespace opc = Open62541;
using namespace std;

// example server with memory based historian

/**
 * The TestServer class
 */
class TestServer : public opc::Server {
    int _idx = 2; // namespace index
    TestMethod _method;
    opc::NodeId eventType;

public:
    TestServer() {
    }

    void initialise(); // initialise the server before it runs but after it has been configured
};

/**
 * TestServer::initialise
 */
void TestServer::initialise() {
    cout << "initialise()" << endl;
    _idx = addNamespace("urn:test:test"); // create a name space

    cout << "Namespace " << _idx << endl;
    _method.initialise(*this);

    // Add a node and set its context to test context
    opc::NodeId newFolder(_idx, "ServerMethodItem");

    if (addFolder(opc::NodeId::Objects, "ServerMethodItem", newFolder, opc::NodeId::Null)) {
        opc::NodeId nodeNumber(_idx, "Number_Value");
        opc::Variant numberValue(1);

        if (!addVariable(opc::NodeId::Objects, "Number_Value", numberValue, nodeNumber, opc::NodeId::Null)) {
            cout << "Failed to create Number Value Node " << endl;
        }

        opc::NodeId methodId(_idx, "EventTrigger");
        if (_method.addServerMethod(*this, "TestEventTriggerMethod", newFolder, methodId, opc::NodeId::Null, _idx)) {
            cout << "Added TestMethod - Event Trigger Method - call from client (e.g. UAExpert)" << endl;
        }
        else {
            cout << "Failed to add method " << " " <<  UA_StatusCode_name(lastError()) << endl;
        }
    }
}

/**
 * main
 * @return 
 */
int main(int/* argc*/, char **/*argv[]*/) {
    TestServer server;
    cerr << "Starting server" << endl;
    server.start();
    cerr << "Server Finished" << endl;
    return 0;
}
