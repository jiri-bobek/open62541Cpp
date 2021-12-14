/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/

#include <open62541cpp/discoveryserver.h>

namespace Open62541 {

DiscoveryServer::DiscoveryServer(int port, const std::string& url) {
    m_pServer = UA_Server_new();
    if (m_pServer) {
        m_pConfig = UA_Server_getConfig(m_pServer);
        if (m_pConfig) {
            configure(port, url);
        }
    }
}

//*****************************************************************************

void DiscoveryServer::configure(int port, const std::string& url) {
    UA_ServerConfig_setMinimal(m_pConfig, port, nullptr);

    m_pConfig->applicationDescription.applicationType = UA_APPLICATIONTYPE_DISCOVERYSERVER;
    UA_String_clear(&m_pConfig->applicationDescription.applicationUri);
    m_pConfig->applicationDescription.applicationUri = UA_String_fromChars(url.c_str());
#ifdef UA_ENABLE_DISCOVERY_MULTICAST
    m_pConfig->mdnsEnabled                = true;
#endif
    // See http://www.opcfoundation.org/UA/schemas/1.03/ServerCapabilities.csv
    // timeout in seconds when to automatically remove a registered server from the list,
    // if it doesn't re-register within the given time frame.
    // A value of 0 disables automatic removal. Default is 60 Minutes (60*60).
    // It must be bigger than 10 seconds, because cleanup is only triggered
    // approximately every 10 seconds. 
    // The server will still be removed depending on the state of the semaphore file.

    // config.discoveryCleanupTimeout = 60*60;
}

//*****************************************************************************

DiscoveryServer::~DiscoveryServer() {
    if (m_pServer)
        UA_Server_delete(m_pServer);

    if (m_pConfig)
        delete m_pConfig;
}

//*****************************************************************************

bool DiscoveryServer::run() {
    return UA_Server_run(m_pServer, &m_running) == UA_STATUSCODE_GOOD;
}

} // namespace Open62541
