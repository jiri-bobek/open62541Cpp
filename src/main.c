#include <open62541/plugin/log_stdout.h>
#include "open62541/server.h"
#include <open62541/server_config_default.h>

#include "namespace_adi_generated.h"
#include "namespace_di_generated.h"
#include "namespace_bea_generated.h"

#include <signal.h>
#include <stdlib.h>

UA_Boolean running = true;

static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

int main(int argc, char** argv) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server* server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    UA_StatusCode retval;
    /* create nodes from nodeset */
    if (namespace_di_generated(server) != UA_STATUSCODE_GOOD
        || namespace_adi_generated(server) != UA_STATUSCODE_GOOD
        || namespace_bea_generated(server) != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Could not add the bea nodeset. "
            "Check previous output for any error.");
        retval = UA_STATUSCODE_BADUNEXPECTEDERROR;
    }
    else {

        retval = UA_Server_run(server, &running);
    }
    //bea(server);
    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
