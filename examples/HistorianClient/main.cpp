#include <iostream>
#include <open62541cpp/open62541client.h>
using namespace std;

#define DISCOVERY_SERVER_ENDPOINT "opc.tcp://localhost:4850"

/*
 * This demonstrates how to access historical values. The node must have been configured as a historizing node on the
 * server Clients cannot create historizing nodes directly
 */


/******************************************************************************
 * Iterate over all values
 * @param data
 * @return 
 */
/* static UA_Boolean readRaw(const UA_HistoryData *data) {
    cout << "readRaw Value count:" <<  ((long unsigned)data->dataValuesSize) << endl;

    // Iterate over all values
    for (UA_UInt32 i = 0; i < data->dataValuesSize; ++i)
    {
        cout << Open62541::dataValueToString(&data->dataValues[i]) << endl;
    }

    // We want more data!
    return true;
}
*/

/******************************************************************************
 * The HistoricalClient read a historical node
 */
class HistoricalClient : public opc::Client
{
public:
    HistoricalClient() {}

    /**
     * This is the readRaw callback. It is a virtual function in the Client class
     * @param moreDataAvailable
     * @param data
     * @return true for more data
     */
    bool historicalIterator(
        const opc::NodeId &node,
        UA_Boolean moreDataAvailable,
        const UA_ExtensionObject &data) {

        printf("\nRead historical callback:\n");
        printf("\tHas more data:\t%d\n\n", moreDataAvailable);

        if (data.content.decoded.type == &UA_TYPES[UA_TYPES_HISTORYDATA]) {
            // now decode the data
            UA_HistoryData* p = (UA_HistoryData*)data.content.decoded.data;
            cout << "Node Id " << opc::toString(node) << " ";
            cout << "readRaw Value count:" <<  p->dataValuesSize << endl;
            // Iterate over all values
            for (UA_UInt32 i = 0; i < p->dataValuesSize; ++i)
            {
                cout << opc::dataValueToString(&p->dataValues[i]) << endl;
            }
            return true;
        }
        return false;
    }
};

//*****************************************************************************

int main(int /*argc*/, char** /*argv*/) {
    cout << "Test Historical Client - requires the TestHistoricalServer running" << endl;

    HistoricalClient client; // Construct client

    // Connect
    if (client.connect("opc.tcp://localhost:4840")) {
        cout << "Connected" << endl;
        opc::NodeId nodeNumber(2, "Number_Value"); // this is the node we want to monitor

        // The server updates the Number_Value node every 2 seconds
        // so if we wait 10 seconds between calls we should get 5 values
        // when we query the history
        for(;;) {
            cout << "Reading node history" << endl;

            if(client.historyReadRaw(nodeNumber, UA_DateTime_fromUnixTime(0),  UA_DateTime_now(), 10)) {
                cout << "Done history read" << endl;
            }
            else {
                cout << "History read fails " << client.lastError() << " " << UA_StatusCode_name(client.lastError()) << endl;
            }
            cout << "Sleeping for 10 s" << endl;
            sleep(10);
        }
    }
    else {
        cout << "Failed to connect" << endl;
    }
    return 0;
}
