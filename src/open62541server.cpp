/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/
#include <open62541cpp/open62541server.h>
#include <open62541cpp/nodecontext.h>
#include <open62541cpp/serverbrowser.h>
#include <open62541cpp/open62541client.h>
#include <open62541cpp/historydatabase.h>
#include <open62541/plugin/accesscontrol.h>
#include <open62541cpp/condition.h>
#include <open62541cpp/servermethod.h>
#include <open62541cpp/open62541timer.h>
#include "../out/build/x86-Debug/src_generated/open62541/namespace_di_generated.h"
#include "../out/build/x86-Debug/src_generated/open62541/namespace_adi_generated.h"
#include "../out/build/x86-Debug/src_generated/open62541/namespace_bea_generated.h"

namespace Open62541 {
Server::ServerMap Server::s_serverMap;

void Server::timerCallback(UA_Server*, void* data)
{
    // timer callback
    if (data) {
        Timer* t = static_cast<Timer*>(data);
        if (t) {
            t->handle();
            if (t->oneShot()) {
                // Potential risk of the client disappearing
                t->server()->_timerMap.erase(t->id());
            }
        }
    }
}

/******************************************************************************
 * Call-back used to retrieve the list of children of a given node
 * @param childId
 * @param isInverse
 * @param referenceTypeId
 * @param handle
 * @return UA_STATUSCODE_GOOD
 */
UA_StatusCode browseTreeCallBack(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId /*referenceTypeId*/, void* outList)
{
    if (!isInverse) {  // not a parent node - only browse forward
        ((UANodeIdList*)outList)->put(childId);
    }
    return UA_STATUSCODE_GOOD;
}

/*!
 * \brief addTimedCallback
 * \param data
 * \param date
 * \param callbackId
 * \return
 */
bool Server::addTimedEvent(unsigned msDelay, UA_UInt64& callbackId, std::function<void(Timer&)> func)
{
    if (m_pServer) {
        UA_DateTime dt = UA_DateTime_nowMonotonic() + (UA_DATETIME_MSEC * msDelay);
        TimerPtr t(new Timer(this, 0, true, func));
        _lastError = UA_Server_addTimedCallback(m_pServer, Server::timerCallback, t.get(), dt, &callbackId);
        t->setId(callbackId);
        _timerMap[callbackId] = std::move(t);
        return lastOK();
    }
    callbackId = 0;
    return false;
}

/* Add a callback for cyclic repetition to the client.
 *
 * @param client The client object.
 * @param callback The callback that shall be added.
 * @param data Data that is forwarded to the callback.
 * @param interval_ms The callback shall be repeatedly executed with the given
 *        interval (in ms). The interval must be positive. The first execution
 *        occurs at now() + interval at the latest.
 * @param callbackId Set to the identifier of the repeated callback . This can
 *        be used to cancel the callback later on. If the pointer is null, the
 *        identifier is not set.
 * @return Upon success, UA_STATUSCODE_GOOD is returned. An error code
 *         otherwise. */

bool Server::addRepeatedTimerEvent(UA_Double interval_ms, UA_UInt64& callbackId, std::function<void(Timer&)> func)
{
    if (m_pServer) {
        TimerPtr t(new Timer(this, 0, false, func));
        _lastError = UA_Server_addRepeatedCallback(m_pServer, Server::timerCallback, t.get(), interval_ms, &callbackId);
        t->setId(callbackId);
        _timerMap[callbackId] = std::move(t);
        return lastOK();
    }
    callbackId = 0;
    return false;
}
/*!
 * \brief changeRepeatedCallbackInterval
 * \param callbackId
 * \param interval_ms
 * \return
 */
bool Server::changeRepeatedTimerInterval(UA_UInt64 callbackId, UA_Double interval_ms)
{
    if (m_pServer) {
        _lastError = UA_Server_changeRepeatedCallbackInterval(m_pServer, callbackId, interval_ms);
        return lastOK();
    }
    return false;
}
/*!
 * \brief UA_Client_removeCallback
 * \param client
 * \param callbackId
 */
void Server::removeTimerEvent(UA_UInt64 callbackId)
{
    _timerMap.erase(callbackId);
}
//***********************************************************************************
/*!
    \brief Server::findContext
    \param s
    \return
*/
NodeContext *Server::findContext(const std::string &s) {
    return RegisteredNodeContext::findRef(s); // not all node contexts are registered
}

// Life-cycle call backs
UA_StatusCode Server::constructor(
    UA_Server* server,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId, void** nodeContext) {
    if (!server || !nodeId || !nodeContext) return UA_STATUSCODE_GOOD;

    if (auto pContext = (NodeContext*)(*nodeContext)) {
        if (Server* pServer = findServer(server)) {
            NodeId node(*nodeId);
            return (pContext->construct(*pServer, node))
                ? UA_STATUSCODE_GOOD
                : UA_STATUSCODE_BADINTERNALERROR;
        }
    }
    return UA_STATUSCODE_GOOD;
}

//*****************************************************************************

void Server::destructor(
    UA_Server* server,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId, void* nodeContext) {
    if (!server || !nodeId || !nodeContext) return;

    if (Server* pServer = findServer(server)) {
        NodeId node(*nodeId);
        ((NodeContext*)nodeContext)->destruct(*pServer, node);
    }
}

//*****************************************************************************

/*!
 * \brief Server::asyncOperationNotifyCallback
 * \param server
 */
//void Open62541::Server::asyncOperationNotifyCallback(UA_Server* server)
//{
//    Server* p = Open62541::findServer(server);  // find the server
//    if (p) {
//        p->asyncOperationNotify();
//    }
//}

void Server::monitoredItemRegisterCallback(UA_Server* server,
                                                      const UA_NodeId* sessionId,
                                                      void* sessionContext,
                                                      const UA_NodeId* nodeId,
                                                      void* nodeContext,
                                                      UA_UInt32 attibuteId,
                                                      UA_Boolean removed)
{
    Server *p = findServer(server); // find the server
    if (p) {
        p->monitoredItemRegister(sessionId, sessionContext, nodeId, nodeContext, (uint32_t)attibuteId, (bool)removed);
    }
}

UA_Boolean Server::createOptionalChildCallback(UA_Server* server,
                                                          const UA_NodeId* sessionId,
                                                          void* sessionContext,
                                                          const UA_NodeId* sourceNodeId,
                                                          const UA_NodeId* targetParentNodeId,
                                                          const UA_NodeId* referenceTypeId)
{
    Server *p = findServer(server); // find the server
    if (p) {
        return p->createOptionalChild(sessionId, sessionContext, sourceNodeId, targetParentNodeId, referenceTypeId);
    }
    return UA_FALSE;
}

UA_StatusCode Server::generateChildNodeIdCallback(UA_Server* server,
                                                             const UA_NodeId* sessionId,
                                                             void* sessionContext,
                                                             const UA_NodeId* sourceNodeId,
                                                             const UA_NodeId* targetParentNodeId,
                                                             const UA_NodeId* referenceTypeId,
                                                             UA_NodeId* targetNodeId)
{
    Server *p = findServer(server); // find the server
    if (p) {
        p->generateChildNodeId(sessionId,
                               sessionContext,
                               sourceNodeId,
                               targetParentNodeId,
                               referenceTypeId,
                               targetNodeId);
    }
    return UA_STATUSCODE_BADSERVERNOTCONNECTED;
}

// Access Control Callbacks
UA_Boolean Server::allowAddNodeHandler(UA_Server* server,
                                       UA_AccessControl* accessControl,
                                       const UA_NodeId* sessionId,
                                       void* sessionContext,
                                       const UA_AddNodesItem* item)
{
    if (Server* pServer = findServer(server)) {
        return pServer->allowAddNode(accessControl, sessionId, sessionContext, item);
    }
    return UA_FALSE;
}

UA_Boolean Server::allowAddReferenceHandler(UA_Server* server,
                                                       UA_AccessControl* ac,
                                                       const UA_NodeId* sessionId,
                                                       void* sessionContext,
                                                       const UA_AddReferencesItem* item)
{
    Server* p = findServer(server);
    if (p) {
        return p->allowAddReference(ac, sessionId, sessionContext, item);
    }
    return UA_FALSE;
}

//*****************************************************************************

UA_Boolean Server::allowDeleteNodeHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_DeleteNodesItem* item) {
    if (Server* pServer = findServer(server)) {
        return pServer->allowDeleteNode(
            accessControl,
            sessionId,
            sessionContext,
            item);
    }
    return UA_FALSE; // Do not allow deletion from client
}

//*****************************************************************************

UA_Boolean
Server::allowDeleteReferenceHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_DeleteReferencesItem* item) {
    if (Server* pServer = findServer(server)) {
        return pServer->allowDeleteReference(
            accessControl,
            sessionId,
            sessionContext,
            item);
    }
    return UA_FALSE;
}

//*****************************************************************************

UA_StatusCode Server::activateSessionHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_EndpointDescription* endpointDescription,
    const UA_ByteString* secureChannelRemoteCertificate,
    const UA_NodeId* sessionId,
    const UA_ExtensionObject* userIdentityToken,
    void** sessionContext) {
    if (Server* pServer = findServer(server)) {
        return pServer->activateSession(
            accessControl,
            endpointDescription,
            secureChannelRemoteCertificate,
            sessionId,
            userIdentityToken,
            sessionContext);
    }
    return -1;
}

//*****************************************************************************

void Server::closeSessionHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext) {
    if (Server* pServer = findServer(server)) {
        pServer->closeSession(accessControl, sessionId, sessionContext);
    }
}

//*****************************************************************************

UA_UInt32 Server::getUserRightsMaskHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId, void* nodeContext) {
    if (Server* pServer = findServer(server)) {
        return pServer->getUserRightsMask(
            accessControl,
            sessionId,
            sessionContext,
            nodeId,
            nodeContext);
    }
    return 0;
}

//*****************************************************************************

UA_Byte Server::getUserAccessLevelHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId, void* nodeContext) {
    if (Server* pServer = findServer(server)) {
        return pServer->getUserAccessLevel(
            accessControl,
            sessionId,
            sessionContext,
            nodeId,
            nodeContext);
    }
    return 0;
}

//*****************************************************************************

UA_Boolean Server::getUserExecutableHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* methodId, void* methodContext) {
    if (Server* pServer = findServer(server)) {
        return pServer->getUserExecutable(
            accessControl,
            sessionId,
            sessionContext,
            methodId,
            methodContext);
    }
    return UA_FALSE;
}

//*****************************************************************************

UA_Boolean Server::getUserExecutableOnObjectHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* methodId, void* methodContext,
    const UA_NodeId* objectId, void* objectContext) {
    if (Server* pServer = findServer(server)) {
        return pServer->getUserExecutableOnObject(
            accessControl,
            sessionId,
            sessionContext,
            methodId,
            methodContext,
            objectId,
            objectContext);
    }
    return UA_FALSE;
}

//*****************************************************************************

UA_Boolean Server::allowHistoryUpdateUpdateDataHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId,
    UA_PerformUpdateType performInsertReplace,
    const UA_DataValue* value) {
    if (Server* pServer = findServer(server)) {
        return pServer->allowHistoryUpdateUpdateData(
            accessControl,
            sessionId,
            sessionContext,
            nodeId,
            performInsertReplace,
            value);
    }
    return UA_FALSE;
}

//*****************************************************************************

UA_Boolean Server::allowHistoryUpdateDeleteRawModifiedHandler(
    UA_Server* server, UA_AccessControl* accessControl,
    const UA_NodeId* sessionId, void* sessionContext,
    const UA_NodeId* nodeId,
    UA_DateTime startTimestamp,
    UA_DateTime endTimestamp,
    bool isDeleteModified) {
    if (Server* pServer = findServer(server)) {
        return pServer->allowHistoryUpdateDeleteRawModified(
            accessControl,
            sessionId,
            sessionContext,
            nodeId,
            startTimestamp,
            endTimestamp,
            isDeleteModified);
    }
    return UA_FALSE;
}

//*****************************************************************************

Server::Server() {
    m_pServer = UA_Server_new();
    if (m_pServer) {
        m_pConfig = UA_Server_getConfig(m_pServer);
        if (m_pConfig) {
            UA_ServerConfig_setDefault(m_pConfig);
            m_pConfig->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
            m_pConfig->nodeLifecycle.destructor = destructor;
        }
    }
}

Server::Server(
    int port,
    const UA_ByteString& certificate /*= UA_BYTESTRING_NULL*/) {
    m_pServer = UA_Server_new();
    if (m_pServer) {

        if (namespace_di_generated(m_pServer) != UA_STATUSCODE_GOOD ||
            namespace_adi_generated(m_pServer) != UA_STATUSCODE_GOOD ||
            namespace_bea_generated(m_pServer) != UA_STATUSCODE_GOOD) {
            UA_LOG_ERROR(UA_Log_Stdout,
                         UA_LOGCATEGORY_SERVER,
                         "Could not add the bea nodeset. "
                         "Check previous output for any error.");
        }

        m_pConfig = UA_Server_getConfig(m_pServer);
        if (m_pConfig) {
            UA_ServerConfig_setMinimal(m_pConfig, port, &certificate);
            m_pConfig->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
            m_pConfig->nodeLifecycle.destructor = destructor;
        }
    }
}

/* Allow browsing a node */
UA_Boolean Server::allowBrowseNodeHandler(UA_Server* server,
                                                     UA_AccessControl* ac,
                                                     const UA_NodeId* sessionId,
                                                     void* sessionContext,
                                                     const UA_NodeId* nodeId,
                                                     void* nodeContext)
{
    Server* p = findServer(server);
    if (p) {
        return (p->allowBrowseNode(ac, sessionId, sessionContext, nodeId, nodeContext)) ? UA_TRUE : UA_FALSE;
    }
    return UA_FALSE;
}

#ifdef UA_ENABLE_SUBSCRIPTIONS
/* Allow transfer of a subscription to another session. The Server shall
 * validate that the Client of that Session is operating on behalf of the
 * same user */
UA_Boolean Server::allowTransferSubscriptionHandler(UA_Server* server,
                                                    UA_AccessControl* ac,
                                                    const UA_NodeId* oldSessionId,
                                                    void* oldSessionContext,
                                                    const UA_NodeId* newSessionId,
                                                    void* newSessionContext)
{
    Server* p = findServer(server);
    if (p) {
        return (p->allowTransferSubscription(ac, oldSessionId, oldSessionContext, newSessionId, newSessionContext))
                   ? UA_TRUE
                   : UA_FALSE;
    }
    return UA_FALSE;
}

#endif

//*****************************************************************************

    
/*!
    \brief Server::initialise
*/
void Server::initialise()
{
    // called after the server object has been created but before run has been called
    // load configuration files and set up the address space
    // create namespaces and endpoints
    // set up methods and stuff
}

void Server::shutdown()
{
    if (!m_pServer) return;

    UA_Server_run_shutdown(m_pServer);
    s_serverMap.erase(m_pServer); // unreachable by call-backs
}


/*!
    \brief Server::stop
*/
void Server::stop()
{  // stop the server
    m_running = false;
}

//*****************************************************************************

void Server::terminate()
{
    if (!m_pServer) return;
    _timerMap.clear();
    _conditionMap.clear();
    UA_Server_run_shutdown(m_pServer);
    UA_Server_delete(m_pServer);
    s_serverMap.erase(m_pServer); // unreachable by call-backs
    m_pServer = nullptr;
}

//*****************************************************************************

void Server::start() {
    if (m_running || !m_pServer)
      return;

    m_running = true;

    create();
    initialise();
    while (m_running)
      iterate();

    terminate();

    m_running = false;
}

//*****************************************************************************

void Server::create()
{
  s_serverMap[m_pServer] = this; // map for call-backs
  UA_Server_run_startup(m_pServer);
}

//*****************************************************************************

void Server::iterate()
{
  UA_Server_run_iterate(m_pServer, true);
  // called from time to time.
  // Only safe places to access server are in process() and callbacks
  process();
}

//*****************************************************************************

void Server::applyEndpoints(EndpointDescriptionArray& endpoints) {
    m_pConfig->endpoints     = endpoints.data();
    m_pConfig->endpointsSize = endpoints.length();

    endpoints.release(); // Transfer ownership
}

//*****************************************************************************

/*!
    \brief Client::browseTree
    \param nodeId
    \param tree
    \return
*/
bool Server::browseTree(const NodeId& nodeId, UANodeTree& tree)
{
    // form a heirachical tree of nodes given node is not added to tree
    return browseTree(nodeId.get(), tree.rootNode());
}

//*****************************************************************************

/*!
    \brief Client::browseTree
    \param nodeId
    \param node
    \return
*/
bool Server::browseTree(const UA_NodeId& nodeId, UANode* node)
{
    if (!m_pServer)
        return UA_FALSE;
    // form a heirachical tree of nodes
    UANodeIdList l;  // shallow copy node IDs and take ownership
    {
        WriteLock ll(m_mutex);
        UA_Server_forEachChildNodeCall(m_pServer, nodeId, browseTreeCallBack, &l);  // get the childlist
    }
    for (int i = 0; i < int(l.size()); i++) {
        if (l[i].namespaceIndex > 0) {
            QualifiedName outBrowseName;
            {
                WriteLock ll(m_mutex);
                _lastError = __UA_Server_read(m_pServer, &l[i], UA_ATTRIBUTEID_BROWSENAME, outBrowseName);
            }
            if (_lastError == UA_STATUSCODE_GOOD) {
                std::string s = toString(outBrowseName.get().name);  // get the browse name and leak key
                auto nId = l[i];                                // deep copy
                UANode* n     = node->createChild(s);                // create the node
                n->setData(nId);
                browseTree(l[i], n);
            }
        }
    }
    return UA_TRUE;
}

//*****************************************************************************

bool Server::deleteTree(const NodeId& nodeId) {
    if (!m_pServer) return false;

    NodeIdMap nodeMap; // set of nodes to delete
    browseTree(nodeId, nodeMap);
    for (auto& node : nodeMap) {
        if (node.second.namespaceIndex > 0) { // namespace 0 appears to be reserved
            WriteLock l(m_mutex);
            UA_Server_deleteNode(m_pServer, node.second, true);
        }
    }
    return lastOK();
}

void Server::setServerUri(const std::string& uri) {
    UA_String_clear(&m_pConfig->applicationDescription.applicationUri);
    m_pConfig->applicationDescription.applicationUri = UA_String_fromChars(uri.c_str());
}

void Server::setMdnsServerName(const std::string& name)
{
    if (m_pConfig) {
        // m_pConfig-> = true;
#ifdef UA_ENABLE_DISCOVERY_MULTICAST
        m_pConfig->mdnsConfig.mdnsServerName = UA_String_fromChars(name.c_str());
#else
        (void)name;
#endif
    }
}

    

//*****************************************************************************

UANodeIdList Server::getChildrenList(const UA_NodeId& node) {
    UANodeIdList children;
    WriteLock ll(m_mutex);

    UA_Server_forEachChildNodeCall(
        m_pServer, node,
        browseTreeCallBack, // browse the tree
        &children);         // output arg of the call-back. hold the list

    return children; // NRVO
}

//*****************************************************************************

bool Server::browseChildren(const UA_NodeId& nodeId, NodeIdMap& nodeMap) {
    if (!m_pServer) return false;

    UANodeIdList children;
    {
        WriteLock ll(m_mutex);
        UA_Server_forEachChildNodeCall( // get the child list
            m_pServer, nodeId,
            browseTreeCallBack,
            &children);
    }
    for (auto& child : getChildrenList(nodeId)) {
        if (child.namespaceIndex != nodeId.namespaceIndex)
            continue; // only in same namespace

        if (nodeMap.find(toString(child)) == nodeMap.end()) {
            nodeMap.put(child);
            browseChildren(child, nodeMap); // recurse no duplicates
        }
    }
    return lastOK();
}

//*****************************************************************************

bool Server::browseSimplifiedBrowsePath(
    const NodeId&        origin,
    size_t               browsePathSize,
    const QualifiedName& browsePath,
    BrowsePathResult&    result) {
    result = UA_Server_browseSimplifiedBrowsePath(
        m_pServer,
        origin,
        browsePathSize,
        browsePath.constRef());
    _lastError = result.statusCode();
    return lastOK();
}



//*****************************************************************************

bool Server::browseTree(const NodeId& nodeId, NodeIdMap& nodeMap) {
    nodeMap.put(nodeId);
    return browseChildren(nodeId, nodeMap);
}

//*****************************************************************************

bool Server::getNodeContext(const NodeId& node, NodeContext*& pContext) {
    if (!server()) return false;

    void* p = (void*)pContext;
    _lastError = UA_Server_getNodeContext(m_pServer, node.get(), &p);
    return lastOK();
}

//*****************************************************************************

bool Server::setNodeContext(const NodeId& node, const NodeContext* context) {
    if (!server()) return false;

    _lastError = UA_Server_setNodeContext(m_pServer, node.get(), (void*)context);
    return lastOK();
}

//*****************************************************************************

bool Server::nodeIdFromPath(
    const NodeId& start,
    const Path&   path,
    NodeId&       nodeId) {
    nodeId = start;
    int level = 0;

    if (path.size() > 0) {
        ServerBrowser browser(*this);
        while (level < int(path.size())) {
            browser.browse(nodeId);
            auto pNode = browser.find(path[level]);
            if (!pNode) return false;
            level++;
            nodeId = pNode->nodeId;
        }
    }
    return (level == int(path.size()));
}

//*****************************************************************************

bool Server::createFolderPath(
    const NodeId& start,
    const Path&   path,
    int           nameSpaceIndex,
    NodeId&       nodeId) {
    UA_NodeId node = start.get(); // use node ids to browse with
    int level = 0;

    if (path.size() > 0) {
        ServerBrowser browser(*this);
        while (level < int(path.size())) {
            browser.browse(node);
            auto pNode = browser.find(path[level]);
            if (!pNode) break;
            level++;
            node = pNode->nodeId; // shallow copy
        }
        nodeId = node;
        NodeId newNode;
        while (level < int(path.size())) {
            if (!addFolder(nodeId,
                            path[level],
                            NodeId::Null,
                            newNode.notNull(),
                            nameSpaceIndex)) {
                break; // stop on failure
            }
            nodeId = newNode; // assign
            level++;
        }
    }
    return (level == int(path.size()));
}

//*****************************************************************************

bool Server::getChild(
    const NodeId&       start,
    const std::string&  childName,
    NodeId&             ret) {
    Path path;
    path.push_back(childName);
    return nodeIdFromPath(start, path, ret);
}

//*****************************************************************************

UA_UInt16 Server::addNamespace(const std::string& name) {
    if (!server()) return 0;

    WriteLock l(m_mutex);
    return UA_Server_addNamespace(m_pServer, name.c_str());
}

//*****************************************************************************

bool Server::addFolder(
    const NodeId&       parent,
    const std::string&  browseName,
    const NodeId&       nodeId          /*= NodeId::Null*/,
    NodeId&             outNewNode      /*= NodeId::Null*/,
    int                 nameSpaceIndex  /*= 0*/) {

    if (nameSpaceIndex == 0) // inherit parent by default
        nameSpaceIndex = parent.nameSpaceIndex();
    return addObjectNode(
        nodeId,
        parent,
        NodeId::Organizes,
        QualifiedName(nameSpaceIndex, browseName),
        NodeId::FolderType,
        ObjectAttributes(browseName),
        outNewNode);
}

//*****************************************************************************

bool Server::addVariable(
    const NodeId&       parent,
    const std::string&  browseName,
    const Variant&      value,
    const NodeId&       nodeId          /*= NodeId::Null*/,
    NodeId&             outNewNode      /*= NodeId::Null*/,
    NodeContext*        context         /*= nullptr*/,
    int                 nameSpaceIndex  /*= 0*/) {

    if (nameSpaceIndex == 0) // inherit parent by default
        nameSpaceIndex = parent.nameSpaceIndex();

    return addVariableNode(
        nodeId,
        parent,
        NodeId::Organizes,
        QualifiedName(nameSpaceIndex, browseName),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), // no variable type
        VariableAttributes(browseName, value)
            .setDataType(value.get().type->typeId)
            .setArray(value)
            .setAccessLevelMask(UA_ACCESSLEVELMASK_READ
                                | UA_ACCESSLEVELMASK_WRITE),
        outNewNode,
        context);
}

//*****************************************************************************

bool Server::addHistoricalVariable(
    const NodeId&       parent,
    const std::string&  browseName,
    const Variant&      value,
    const NodeId&       nodeId          /*= NodeId::Null*/,
    NodeId&             outNewNode      /*= NodeId::Null*/,
    NodeContext*        context         /*= nullptr*/,
    int                 nameSpaceIndex  /*= 0*/) {

    if (nameSpaceIndex == 0) // inherit parent by default
        nameSpaceIndex = parent.nameSpaceIndex();

    return addVariableNode(
        nodeId,
        parent,
        NodeId::Organizes,
        QualifiedName(nameSpaceIndex, browseName),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), // no variable type
        VariableAttributes(browseName, value)
                               .setDataType(value.get().type->typeId)
            .setHistorizing()
            .setAccessLevelMask(UA_ACCESSLEVELMASK_READ
                              | UA_ACCESSLEVELMASK_WRITE),
        outNewNode,
        context);
}

//*****************************************************************************

bool Server::addProperty(
    const NodeId&   parent,
    const std::string& browseName,
    const Variant&  value,
    const NodeId&   nodeId          /*= NodeId::Null*/,
    NodeId&         outNewNode      /*= NodeId::Null*/,
    NodeContext*    context         /*= nullptr*/,
    int             nameSpaceIndex  /*= 0*/) {

    return addVariableNode(
        nodeId,
        parent,
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
        QualifiedName(nameSpaceIndex, browseName),
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
        VariableAttributes(browseName, value)
            .setAccessLevelMask(UA_ACCESSLEVELMASK_READ
                              | UA_ACCESSLEVELMASK_WRITE),
        outNewNode,
        context);
}

//*****************************************************************************

bool Server::addMethod(
    ServerMethod*       method,
    const std::string&  browseName,
    const NodeId&       parent,
    const  NodeId&      nodeId,
    NodeId&             outNewNode      /*= NodeId::Null*/,
    int                 nameSpaceIndex  /*= 0*/) {

    if (!server()) return false;
    if (nameSpaceIndex == 0) // inherit parent by default
        nameSpaceIndex = parent.nameSpaceIndex();

    WriteLock l(mutex());
    _lastError = UA_Server_addMethodNode(
        m_pServer,
        nodeId,
        parent,
        NodeId::HasOrderedComponent,
        QualifiedName(nameSpaceIndex, browseName),
        MethodAttributes(browseName)
            .setExecutable(),
        ServerMethod::methodCallback,
        method->in().size() - 1,
        method->in().data(),
        method->out().size() - 1,
        method->out().data(),
        (void*)(method), // method context is reference to the call handler
        outNewNode.isNull() ? nullptr : outNewNode.ref());

    return lastOK();
}

//*!
//     * \brief addServerMethod
//     * \param method - this must persist for the life time of the node !!!!!!
//     * \param browseName
//     * \param parent
//     * \param nodeId
//     * \param newNode
//     * \param nameSpaceIndex
//     * \return
//     */
    bool Server::addServerMethod(ServerMethod* method,
                         const std::string& browseName,
                         const NodeId& parent,
                         const NodeId& nodeId,
                         NodeId& newNode,
                         int nameSpaceIndex)
{
    //
    if (!server())
        return false;
    //
    if (nameSpaceIndex == 0)
        nameSpaceIndex = parent.nameSpaceIndex();  // inherit parent by default
    //
    MethodAttributes attr;
    attr.setDefault();
    attr.setDisplayName(browseName);
    attr.setDescription(browseName);
    attr.setExecutable();
    //
    QualifiedName qn(nameSpaceIndex, browseName);
    {
        WriteLock l(mutex());
        _lastError = UA_Server_addMethodNode(m_pServer,
                                             nodeId,
                                             parent,
                                             NodeId::HasOrderedComponent,
                                             qn,
                                             attr,
                                             ServerMethod::methodCallback,
                                             method->in().size() - 1,
                                             method->in().data(),
                                             method->out().size() - 1,
                                             method->out().data(),
                                             (void*)(method),  // method context is reference to the call handler
                                             newNode.isNull() ? nullptr : newNode.ref());
    }
    return lastOK();
}

//*****************************************************************************

bool Server::deleteNode(const NodeId& nodeId, bool deleteReferences) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_deleteNode(m_pServer, nodeId, UA_Boolean(deleteReferences));
    return lastOK();
}

//*****************************************************************************

bool Server::addVariableNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const NodeId&           typeDefinition,
    const VariableAttributes& attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addVariableNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        typeDefinition,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());

    return lastOK();
}

//*****************************************************************************

bool Server::addVariableTypeNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const NodeId&           typeDefinition,
    const VariableTypeAttributes& attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addVariableTypeNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        typeDefinition,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addObjectNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const NodeId&           typeDefinition,
    const ObjectAttributes& attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addObjectNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        typeDefinition,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());

    return lastOK();
}

//*****************************************************************************

bool Server::addObjectTypeNode(
    const NodeId&               nodeId,
    const NodeId&               parent,
    const NodeId&               referenceTypeId,
    const QualifiedName&        browseName,
    const ObjectTypeAttributes& attr,
    NodeId&                     outNewNode  /*= NodeId::Null*/,
    NodeContext*                context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addObjectTypeNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addViewNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const ViewAttributes&   attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addViewNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addReferenceTypeNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const ReferenceTypeAttributes& attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addReferenceTypeNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addDataTypeNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const DataTypeAttributes& attr,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addDataTypeNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        attr,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addDataSourceVariableNode(
    const NodeId&           nodeId,
    const NodeId&           parent,
    const NodeId&           referenceTypeId,
    const QualifiedName&    browseName,
    const NodeId&           typeDefinition,
    const VariableAttributes& attr,
    const DataSource&       dataSource,
    NodeId&                 outNewNode  /*= NodeId::Null*/,
    NodeContext*            context     /*= nullptr*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addDataSourceVariableNode(
        m_pServer,
        nodeId,
        parent,
        referenceTypeId,
        browseName,
        typeDefinition,
        attr,
        dataSource,
        context,
        outNewNode.isNull() ? nullptr : outNewNode.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::addReference(
    const NodeId&           sourceId,
    const NodeId&           referenceTypeId,
    const ExpandedNodeId&   targetId,
    bool                    isForward) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_addReference(
        server(),
        sourceId,
        referenceTypeId,
        targetId,
        isForward);
    return lastOK();
}

//*****************************************************************************

bool Server::markMandatory(const NodeId& nodeId) {
    return addReference(
        nodeId,
        NodeId::HasModellingRule,
        ExpandedNodeId::ModellingRuleMandatory,
        true);
}

//*****************************************************************************

bool Server::deleteReference(
    const NodeId&   sourceNodeId,
    const NodeId&   referenceTypeId,
    bool            isForward,
    const ExpandedNodeId& targetNodeId,
    bool            deleteBidirectional) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_deleteReference(
        server(),
        sourceNodeId,
        referenceTypeId,
        isForward,
        targetNodeId,
        deleteBidirectional);
    return lastOK();
}

//*****************************************************************************

bool Server::addInstance(
    const std::string&  name,
    const NodeId&       nodeId,
    const NodeId&       parent,
    const NodeId&       typeId,
    NodeId&             outNewNode  /*= NodeId::Null*/,
    NodeContext*        context     /*= nullptr*/) {

    return addObjectNode(
        nodeId,
        parent,
        NodeId::Organizes,
        QualifiedName(parent.nameSpaceIndex(), name),
        typeId,
        ObjectAttributes(name),
        outNewNode,
        context);
}

//*****************************************************************************

bool Server::createEvent(const NodeId& eventType, NodeId& outNodeId) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_createEvent(m_pServer, eventType, outNodeId.ref());
    return lastOK();
}

//*****************************************************************************

bool Server::triggerEvent(
    const NodeId&   eventNodeId,
    UA_ByteString*  outEventId      /*= nullptr*/,
    bool            deleteEventNode /*= true*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_triggerEvent(
        m_pServer,
        eventNodeId,
        UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER),
        outEventId,
        deleteEventNode);
    return lastOK();
}

//*****************************************************************************

bool Server::addNewEventType(
    const std::string&  name,
    NodeId&             outEventType,
    const std::string&  description /*= std::string()*/) {

    return addObjectTypeNode(
        UA_NODEID_NULL,
        UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE),
        UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
        QualifiedName(0, name),
        ObjectTypeAttributes()
            .setDefault()
            .setDisplayName(name)
            .setDescription((description.empty() ? name : description)),
        outEventType);
}

//*****************************************************************************

bool Server::setUpEvent(
    NodeId&             outId,
    NodeId&             eventType,
    const std::string&  eventMessage,
    const std::string&  eventSourceName,
    int                 eventSeverity /*= 100*/,
    UA_DateTime         eventTime     /*= UA_DateTime_now()*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_createEvent(server(), eventType, outId);
    if (_lastError != UA_STATUSCODE_GOOD)
        return lastOK();

    /* Set the Event Attributes */
    /* Setting the Time is required or else the event will not show up in UAExpert! */
    UA_Server_writeObjectProperty_scalar(server(), outId,
        UA_QUALIFIEDNAME(0, const_cast<char*>("Time")),
        &eventTime, &UA_TYPES[UA_TYPES_DATETIME]);

    UA_Server_writeObjectProperty_scalar(server(), outId,
        UA_QUALIFIEDNAME(0, const_cast<char*>("Severity")),
        &eventSeverity, &UA_TYPES[UA_TYPES_UINT16]);

    LocalizedText eM(const_cast<char*>("en-US"), eventMessage);
    UA_Server_writeObjectProperty_scalar(server(), outId,
        UA_QUALIFIEDNAME(0, const_cast<char*>("Message")),
        eM.ref(), &UA_TYPES[UA_TYPES_LOCALIZEDTEXT]);

    UA_String eSN = UA_STRING(const_cast<char*>(eventSourceName.c_str()));
    UA_Server_writeObjectProperty_scalar(server(), outId,
        UA_QUALIFIEDNAME(0, const_cast<char*>("SourceName")),
        &eSN, &UA_TYPES[UA_TYPES_STRING]);

    return lastOK();
}

//*****************************************************************************

bool Server::call(const CallMethodRequest& request, CallMethodResult& ret) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    ret = UA_Server_call(m_pServer, request);
    return ret.get().statusCode == UA_STATUSCODE_GOOD;
}

//*****************************************************************************

bool Server::translateBrowsePathToNodeIds(
    const BrowsePath&   path,
    BrowsePathResult&   result) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    result = UA_Server_translateBrowsePathToNodeIds(m_pServer, path);
    return result.statusCode() == UA_STATUSCODE_GOOD;
}

//*****************************************************************************

bool Server::readAttribute(
    const UA_NodeId* nodeId,
    UA_AttributeId   attributeId,
    void*            value) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = __UA_Server_read(m_pServer, nodeId, attributeId, value);
    return lastOK();
}

//*****************************************************************************

bool Server::writeAttribute(
    const UA_NodeId*     nodeId,
    const UA_AttributeId attributeId,
    const UA_DataType*   attr_type,
    const void*          attr) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = __UA_Server_write(m_pServer, nodeId, attributeId, attr_type, attr);
    return lastOK();
}

//*****************************************************************************

bool Server::readBrowseName(const NodeId& nodeId, std::string& name, int& idxNameSpace) {
    if (!m_pServer) throw std::runtime_error("Null server"); // why not return false?

    QualifiedName browseName;
    if (readBrowseName(nodeId, browseName)) {
        name = toString(browseName.name());
        idxNameSpace  = browseName.namespaceIndex();
    }
    return lastOK();
}

//*****************************************************************************

bool Server::setEnable(const NodeId& nodeId) {
    UA_Byte accessLevel;
    if (readAccessLevel(nodeId, accessLevel)) {
        accessLevel |= UA_ACCESSLEVELMASK_WRITE;
        return setAccessLevel(nodeId, accessLevel);
    }
    return false;
}

//*****************************************************************************

bool Server::setReadOnly(const NodeId& nodeId, bool historyEnable /*= false*/) {
    UA_Byte accessLevel;
    if (!readAccessLevel(nodeId, accessLevel))
        return false;

    // remove the write bits
    accessLevel &= ~(UA_ACCESSLEVELMASK_WRITE | UA_ACCESSLEVELMASK_HISTORYWRITE);
    // add the read bits
    accessLevel |= UA_ACCESSLEVELMASK_READ;
    if (historyEnable) accessLevel |= UA_ACCESSLEVELMASK_HISTORYREAD;
    return setAccessLevel(nodeId, accessLevel);
}

//*****************************************************************************

bool Server::updateCertificate(
    const UA_ByteString* oldCertificate,
    const UA_ByteString* newCertificate,
    const UA_ByteString* newPrivateKey,
    bool                 closeSessions       /*= true*/,
    bool                 closeSecureChannels /*= true*/) {
    if (!server()) return false;

    WriteLock l(m_mutex);
    _lastError = UA_Server_updateCertificate(
        m_pServer,
        oldCertificate,
        newCertificate,
        newPrivateKey,
        closeSessions,
        closeSecureChannels);
    return lastOK();
}

//*****************************************************************************

void Server::serverOnNetworkCallback(
    const UA_ServerOnNetwork*   serverNetwork,
    UA_Boolean                  isServerAnnounce,
    UA_Boolean                  isTxtReceived,
    void*                       data) {
    if (auto pServer = (Server*)data)
        pServer->serverOnNetwork(serverNetwork, isServerAnnounce, isTxtReceived);
}

//*****************************************************************************

void Server::registerServerCallback(
    const UA_RegisteredServer*  registeredServer,
    void*                       data) {
    if (auto pServer = (Server*)data)
        pServer->registerServer(registeredServer);
}

//*****************************************************************************

bool Server::registerDiscovery(
    Client&             client,
    const std::string&  semaphoreFilePath) {
    _lastError = UA_Server_register_discovery(
        m_pServer,
        client.client(),
        semaphoreFilePath.empty() ? nullptr : semaphoreFilePath.c_str());

    return lastOK();
}

//*****************************************************************************

bool Server::unregisterDiscovery(Client& client) {
    if (!server()) return false;

    _lastError = UA_Server_unregister_discovery(server(), client.client());
    return lastOK();
}

//*****************************************************************************
/*!
    \brief addPeriodicServerRegister
    \param discoveryServerUrl
    \param intervalMs
    \param delayFirstRegisterMs
    \param periodicCallbackId
    \return true on success
*/
bool Server::addPeriodicServerRegister(
    const std::string&  discoveryServerUrl,
    Client&             client,
    UA_UInt64&          periodicCallbackId,
    UA_UInt32           intervalMs              /*= 600*1000*/,
    UA_UInt32           delayFirstRegisterMs    /*= 1000*/) {
    if (!server()) return false;

    _lastError = UA_Server_addPeriodicServerRegisterCallback(
        server(),
        client.client(),
        discoveryServerUrl.c_str(),
        intervalMs,
        delayFirstRegisterMs,
        &periodicCallbackId);

    if (lastOK()) {
        m_discoveryList[periodicCallbackId] = discoveryServerUrl;
    }

    return lastOK();
}

#ifdef UA_ENABLE_SUBSCRIPTIONS_ALARMS_CONDITIONS
bool Server::setConditionTwoStateVariableCallback(const NodeId& condition,
                                          UA_TwoStateVariableCallbackType callbackType,
                                          bool removeBranch)
{
    ConditionPtr& c = findCondition(condition);  // conditions are bound to servers - possible for the same node id
                                                 // to be used in different servers
    if (c) {
        return c->setCallback(callbackType, removeBranch);
    }
    return false;
}
#endif

} // namespace Open62541
