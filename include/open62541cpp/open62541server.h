/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/
#ifndef OPEN62541SERVER_H
#define OPEN62541SERVER_H
#ifndef OPEN62541OBJECTS_H
#include <open62541cpp/open62541objects.h>
#endif
#ifndef NODECONTEXT_H
#include <open62541cpp/nodecontext.h>
#endif
#ifndef SERVERMETHOD_H
#include <open62541cpp/servermethod.h>
#endif
#ifndef SERVERREPEATEDCALLBACK_H
#include <open62541cpp/serverrepeatedcallback.h>
#endif
#ifndef CONDITION_H
#include <open62541cpp/condition.h>
#endif

namespace Open62541 {

class HistoryDataGathering;
class HistoryDataBackend;

/**
 * The Server class abstracts the server side.
 * This class wraps the corresponding C functions. Refer to the C documentation for a full explanation.
 * The main thing to watch for is Node ID objects are passed by reference. There are stock Node Id objects including NodeId::Null
 * Pass NodeId::Null where a NULL UA_NodeId pointer is expected.
 * If a NodeId is being passed to receive a value use the notNull() method to mark it as a receiver of a new node id.
 * Most functions return true if the lastError is UA_STATUSCODE_GOOD.
 */
class UA_EXPORT Server {
    using CallBackList = std::map<std::string, ServerRepeatedCallbackRef>;  /**< Map call-backs names to a repeated call-back shared pointers. */
    using ServerMap    = std::map<UA_Server*, Server*>;                     /**< Map UA_Server pointer key to servers pointer value */
    using DiscoveryMap = std::map<UA_UInt64, std::string>;                  /**< Map the repeated registering call-back id with the discovery server URL */
    using LoginList    = std::vector<UA_UsernamePasswordLogin>;

    UA_Server*          m_pServer   = nullptr;  /**< assume one server per application */
    UA_ServerConfig*    m_pConfig   = nullptr;  /**< The server configuration */
    UA_Boolean          m_running   = false;    /**< Flag both used to keep the server running and storing the server status. Set it to false to stop the server. @see stop(). */
    CallBackList        m_callbacks;            /**< Map call-backs names to a repeated call-back shared pointers. */
    ReadWriteMutex      m_mutex;                /**< mutex for thread-safe read-write of the server nodes. Should probably mutable */

    static ServerMap    s_serverMap;            /**< map UA_SERVERs to Server objects. Enables a server to find another one. */
    DiscoveryMap        m_discoveryList;        /**< set of discovery servers this server has registered with.
                                                     Map the repeated registering call-back id with the discovery server URL. */
    LoginList           m_logins;               /**< set of permitted logins (user, password pairs)*/


    // Life cycle call-backs

    /**
     * Call-back used to construct a new node in a given server.
     * Can be NULL. May replace the nodeContext.
     * nodeContext.construct() is used to to build the new node, taking nodeId as input.
     * @param server specify the server of the new node.
     * @param sessionId specify the session id (currently unused)
     * @param sessionContextspecify the session context (currently unused)
     * @param nodeId the node copied by the constructed node
     * @param nodeContext specify how to construct the node via its construct() method.
     * @return UA_STATUSCODE_GOOD on success or nothing done, UA_STATUSCODE_BADINTERNALERROR construction failure.
     * @see NodeContext
     */
    static UA_StatusCode constructor(
        UA_Server* server,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,    void** nodeContext);
public:
    /*!
     * \brief The Timer class - used for timed events
     */
    class  Timer
    {
        Server * _server = nullptr;
        UA_UInt64 _id = 0;
        bool _oneShot = false;
        std::function<void (Timer &)> _handler;

    public:
        Timer() {}
        Timer(Server *c, UA_UInt64 i, bool os, std::function<void (Timer &)> func ) : _server(c),_id(i), _oneShot(os),_handler(func) {}
        virtual ~Timer() {
            UA_Server_removeCallback(_server->server(), _id);
        }
        virtual void handle()
        {
            if(_handler) _handler(*this);
        }
        Server * server() const {
            return  _server;
        }
        UA_UInt64 id() const {
            return  _id;
        }
        void setId(UA_UInt64 i)
        {
            _id = i;
        }
        bool oneShot() const {
            return _oneShot;
        }
    };
protected:
    UA_StatusCode _lastError = 0;
private:
    //
    typedef std::unique_ptr<Timer> TimerPtr;
    std::map<UA_UInt64, TimerPtr> _timerMap; // one map per client
    
    ReadWriteMutex _mutex;
    std::string _customHostName;
    std::map<unsigned, ConditionPtr> _conditionMap; // Conditions - SCADA Alarm state handling by any other name
   
    std::map<UA_UInt64, std::string> _discoveryList; // set of discovery servers this server has registered with

    //
    static void timerCallback(UA_Server *, void *data)
    {
        // timer callback
        if(data)
        {
            Timer *t = static_cast<Timer *>(data);
            if(t)
            {
                t->handle();
                if(t->oneShot())
                {
                    // Potential risk of the client disappearing
                    t->server()->_timerMap.erase(t->id());
                }
            }
        }
    }

    /**
     * Call-back used to destroy a node in a given server.
     * Can be NULL. The context cannot be replaced since
     * the node is destroyed immediately afterwards anyway.
     * nodeContext.destruct() is used to to destroy the node, taking nodeId as input.
     * @param server specify the server of the node to destroy
     * @param sessionId specify the session id (currently unused)
     * @param sessionContextspecify the session context (currently unused)
     * @param nodeId used to identify the node to destroy
     * @param nodeContext specify how to destroy the node via its destruct() method.
     */
    static void destructor(
        UA_Server* server,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,    void* nodeContext);

    // Access Control call-backs - these invoke virtual functions to control access
    /**
     * Call-back called before giving access to the Add Node function on a given server.
     * Behavior can be customized by overriding the allowAddNode() hook.
     * Allowed by default if not specialized.
     * @return true if access is granted.
     */
    static UA_Boolean allowAddNodeHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_AddNodesItem* item);
    /*  Can be NULL. The context cannot be replaced since the node is destroyed
        immediately afterwards anyway. */
    static void destructor(UA_Server *server,
                           const UA_NodeId *sessionId, void *sessionContext,
                           const UA_NodeId *nodeId, void *nodeContext);
    //

    /* Can be NULL. Called during recursive node instantiation. While mandatory
        * child nodes are automatically created if not already present, optional child
        * nodes are not. This callback can be used to define whether an optional child
        * node should be created.
        *
        * @param server The server executing the callback
        * @param sessionId The identifier of the session
        * @param sessionContext Additional data attached to the session in the
        *        access control layer
        * @param sourceNodeId Source node from the type definition. If the new node
        *        shall be created, it will be a copy of this node.
        * @param targetParentNodeId Parent of the potential new child node
        * @param referenceTypeId Identifies the reference type which that the parent
        *        node has to the new node.
        * @return Return UA_TRUE if the child node shall be instantiatet,
        *         UA_FALSE otherwise. */
    static  UA_Boolean createOptionalChildCallback(UA_Server *server,
            const UA_NodeId *sessionId,
            void *sessionContext,
            const UA_NodeId *sourceNodeId,
            const UA_NodeId *targetParentNodeId,
            const UA_NodeId *referenceTypeId);


    /* Can be NULL. Called when a node is to be copied during recursive
         * node instantiation. Allows definition of the NodeId for the new node.
         * If the callback is set to NULL or the resulting NodeId is UA_NODEID_NULL,
         * then a random NodeId will be generated.
         *
         * @param server The server executing the callback
         * @param sessionId The identifier of the session
         * @param sessionContext Additional data attached to the session in the
         *        access control layer
         * @param sourceNodeId Source node of the copy operation
         * @param targetParentNodeId Parent node of the new node
         * @param referenceTypeId Identifies the reference type which that the parent
         *        node has to the new node. */
    static UA_StatusCode generateChildNodeIdCallback(UA_Server *server,
            const UA_NodeId *sessionId, void *sessionContext,
            const UA_NodeId *sourceNodeId,
            const UA_NodeId *targetParentNodeId,
            const UA_NodeId *referenceTypeId,
            UA_NodeId *targetNodeId);


    //
    //
    // Access Control Callbacks - these invoke virtual functions to control access

    static void clearAccesControlHandler(UA_AccessControl *ac)
    {
        Server *s = static_cast<Server *>(ac->context);
        if(s)
        {
            s->clearAccessControl(ac);
        }
    }

    
    /**
     * Control access to the Add Node Reference function on a given server.
     * Behavior can be customized by overriding the allowAddReference() hook.
     * Allowed by default if not specialized.
     * @return true if access is granted.
     */
    static UA_Boolean allowAddReferenceHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_AddReferencesItem* item);

    /**
     * Control access to the Delete Node function on a given server.
     * Behavior can be customized by overriding the allowDeleteNode() hook.
     * Not allowed by default if not specialized.
     * @return true if access is granted.
     */
    static UA_Boolean allowDeleteNodeHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_DeleteNodesItem* item);

    /**
     * Control access to the Delete Node Reference function on a given server.
     * Behavior can be customized by overriding the allowDeleteReference() hook.
     * Allowed by default if not specialized.
     * @return true if access is granted.
     */
    static UA_Boolean allowDeleteReferenceHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_DeleteReferencesItem* item);

    /**
     * Control access to the Activate Session function on a given server.
     * Behavior can be customized by overriding the activateSession() hook.
     * Not allowed by default if not specialized.
     * @return  UA_STATUSCODE_GOOD on success.
                UA_STATUSCODE_BADSESSIONIDINVALID or any relevant error on refusal.
                -1 if the server wasn't found.
     */
    static UA_StatusCode activateSessionHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_EndpointDescription* endpointDescription,
        const UA_ByteString* secureChannelRemoteCertificate,
        const UA_NodeId* sessionId,
        const UA_ExtensionObject* userIdentityToken,
        void** sessionContext);

    /**
     * De-authenticate a session and cleanup.
     * Behavior can be customized by overriding the closeSession() hook.
     */
    static void closeSessionHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext);

    /**
     * Access control for all nodes.
     * Behavior can be customized by overriding the getUserRightsMask() hook.
     * Allowed by default.
     * @return UA_STATUSCODE_GOOD on success.
     */
    static UA_UInt32 getUserRightsMaskHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,    void* nodeContext);

    /**
     * Additional access control for variable nodes.
     * Behavior can be customized by overriding the getUserAccessLevel() hook.
     * Allowed by default.
     * @return UA_STATUSCODE_GOOD on success.
     */
    static UA_Byte getUserAccessLevelHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,    void* nodeContext);

    /**
     * Additional access control for method nodes.
     * Behavior can be customized by overriding the getUserExecutable() hook.
     * Not allowed by default.
     * @return UA_TRUE on success.
     */
    static UA_Boolean getUserExecutableHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* methodId,  void* methodContext);

    /**
     * Additional access control for calling a method node
     * in the context of a specific object.
     * Behavior can be customized by overriding the getUserExecutableOnObject() hook.
     * Not allowed by default.
     * @return UA_TRUE on success.
     */
    static UA_Boolean getUserExecutableOnObjectHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* methodId,  void* methodContext,
        const UA_NodeId* objectId,  void* objectContext);

    /**
     * Allow insert,replace,update of historical data.
     * Behavior can be customized by overriding the allowHistoryUpdateUpdateData() hook.
     * Not allowed by default.
     * @return UA_TRUE on success.
     */
    static UA_Boolean allowHistoryUpdateUpdateDataHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,
        UA_PerformUpdateType performInsertReplace,
        const UA_DataValue* value);

    /**
     * Allow delete of historical data.
     * Behavior can be customized by overriding the allowHistoryUpdateDeleteRawModified() hook.
     * Not allowed by default.
     * @return UA_TRUE on success.
     */
    static UA_Boolean allowHistoryUpdateDeleteRawModifiedHandler(
        UA_Server* server, UA_AccessControl* ac,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,
        UA_DateTime startTimestamp,
        UA_DateTime endTimestamp,
        bool isDeleteModified);


    /* Allow browsing a node */
    static UA_Boolean allowBrowseNodeHandler (UA_Server *server, UA_AccessControl *ac,
            const UA_NodeId *sessionId, void *sessionContext,
            const UA_NodeId *nodeId, void *nodeContext);

#ifdef UA_ENABLE_SUBSCRIPTIONS
    /* Allow transfer of a subscription to another session. The Server shall
     * validate that the Client of that Session is operating on behalf of the
     * same user */
    static UA_Boolean allowTransferSubscriptionHandler(UA_Server *server, UA_AccessControl *ac,
            const UA_NodeId *oldSessionId, void *oldSessionContext,
            const UA_NodeId *newSessionId, void *newSessionContext);
#endif



    // Async handler
    static void asyncOperationNotifyCallback(UA_Server *server);


    /*!
     * \brief monitoredItemRegisterCallback
     * \param server
     * \param sessionId
     * \param sessionContext
     * \param nodeId
     * \param nodeContext
     * \param attibuteId
     * \param removed
     */
    static void monitoredItemRegisterCallback(UA_Server *server,
            const UA_NodeId *sessionId, void *sessionContext,
            const UA_NodeId *nodeId, void *nodeContext,
            UA_UInt32 attibuteId, UA_Boolean removed);


public:
    ConditionPtr & findCondition(const UA_NodeId *condition) {
        return _conditionMap[UA_NodeId_hash(condition)];
    }

    ConditionPtr & findCondition(UA_UInt32 n) {
        return _conditionMap[n];
    }

    /*!
        \brief Server
    */
    Server() {
        _server = UA_Server_new();
        if (_server) {
            _config = UA_Server_getConfig(_server);
            if (_config) {
                UA_ServerConfig_setDefault(_config);
                _config->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
                _config->nodeLifecycle.destructor = destructor;
            }
        }
    }

    /*!
        \brief Server
        \param port
        \param certificate
    */
    Server(int port, const UA_ByteString &certificate = UA_BYTESTRING_NULL) {
        _server = UA_Server_new();
        if (_server) {
            _config = UA_Server_getConfig(_server);
            if (_config) {
                UA_ServerConfig_setMinimal(_config, port, &certificate);
                _config->nodeLifecycle.constructor = constructor; // set up the node global lifecycle
                _config->nodeLifecycle.destructor = destructor;
            }
        }
    }
    /**
     * Return a reference to the login vector member.
     * @todo creat clear(), add(), delete() update
     * @return a std::vector of user name / passwords by reference
     * @see LoginList
     */
    LoginList& logins() { return m_logins; }

    /*!
        \brief ~Server
    */
    virtual ~Server() {
        // possible abnormal exit
        if (_server) {
            WriteLock l(_mutex);
            terminate();
        }
    }
    /**
     * Get the last execution error code.
     * @return the error code of the last executed function.
     *         UA_STATUSCODE_GOOD (0) if no error.
     */
    UA_StatusCode lastError() const { return _lastError; }

    /*!
     * \brief asyncOperationNotify
     * Callback handler
     */
    virtual void asyncOperationNotify() {}

    /*!
     * \brief enableasyncOperationNotify
     */
    void setAsyncOperationNotify()
    {
        if(_config) _config->asyncOperationNotifyCallback = Server::asyncOperationNotifyCallback;
    }

    /*!
     * \brief monitoredItemRegister
     * \param sessionId
     * \param sessionContext
     * \param nodeId
     * \param nodeContext
     * \param attibuteId
     * \param removed
     */
    virtual void monitoredItemRegister(const UA_NodeId */*sessionId*/, void */*sessionContext*/,
                                       const UA_NodeId */*nodeId*/, void */*nodeContext*/,
                                       uint32_t /*attibuteId*/, bool /*removed*/)
    {

    }

    /*!
     * \brief setMonitoredItemRegister
     */
    void setMonitoredItemRegister()
    {
        if(_config) _config->monitoredItemRegisterCallback = Server::monitoredItemRegisterCallback;
    }


    /*!
     * \brief createOptionalChild
     * \return true if child is to be created
     */
    UA_Boolean createOptionalChild(     const UA_NodeId */*sessionId*/,
                                        void */*sessionContext*/,
                                        const UA_NodeId */*sourceNodeId*/,
                                        const UA_NodeId */*targetParentNodeId*/,
                                        const UA_NodeId */*referenceTypeId*/)
    {
        return UA_FALSE;
    }

    /*!
     * \brief setcreateOptionalChild
     */
    void setcreateOptionalChild()
    {
        if(_config) _config->nodeLifecycle.createOptionalChild = Server::createOptionalChildCallback;
    }

    /*!
     * \brief generateChildNodeId
     * \param targetNodeId
     * \return UA_STATUSCODE_GOOD on success
     */
    UA_StatusCode generateChildNodeId(  const UA_NodeId */*sessionId*/,
                                        void */*sessionContext*/,
                                        const UA_NodeId */*sourceNodeId*/,
                                        const UA_NodeId */*targetParentNodeId*/,
                                        const UA_NodeId */*referenceTypeId*/,
                                        UA_NodeId *targetNodeId)
    {
        *targetNodeId =  UA_NODEID_NULL;
        return UA_STATUSCODE_GOOD;
    }

    /*!
     * \brief setGenerateChildNodeId
     */
    void setGenerateChildNodeId()
    {
        if(_config)  _config->nodeLifecycle.generateChildNodeId = Server::generateChildNodeIdCallback;
    }

    /*!
     * \brief setMdnsServerName
     * \param name
     */

    void setMdnsServerName(const std::string &name) {
        if (_config) {
            _config->mdnsEnabled = true;
#ifdef UA_ENABLE_DISCOVERY_MULTICAST
            _config->mdnsConfig.mdnsServerName = UA_String_fromChars(name.c_str());
#else
            (void)name;
#endif
        }
    }
    /**
     * Get the underlying server pointer.
     * @return pointer to underlying server structure
     */
    UA_Server* server() { return m_pServer; }

    /**
     * Get the running state of the server
     * @return UA_TRUE if the server is running,
     *         UA_FALSE if not yet started or stopping.
     */
    UA_Boolean running() const { return m_running; }

    /**
     * access mutex - most accesses need a write lock
     * @return a reference to the server mutex
     */
    ReadWriteMutex& mutex() { return m_mutex; }

    /**
     * Get the server configuration.
     * @return a reference to the server configuration as a UA_ServerConfig
     * @warning assumes the configuration is present, undefined behavior otherwise.
     */
    UA_ServerConfig& serverConfig() { return *UA_Server_getConfig(m_pServer); }
    
    /**
    * Set the list of endpoints for the server.
    * @param endpoints the new list of endpoints for the server, stored in its config.
    * @warning endpoints ownership is transfered, don't use it after this call.
    */
    void applyEndpoints(EndpointDescriptionArray &endpoints) {
        _config->endpoints = endpoints.data();
        _config->endpointsSize = endpoints.length();
        // Transfer ownership
        endpoints.release();
    }

    /**
     * Reset the server configuration.
     */
    void configClean() { if (m_pConfig) UA_ServerConfig_clean(m_pConfig); }


    /**
     * Set up for simple login
     * assumes the permitted logins have been set up beforehand.
     * This gives username / password access and disables anonymous access
     * @return true on success 
     */
    bool enableSimpleLogin();

    /**
     * Set a custom hostname in server configuration
     * @param name the new custom hostname of the server.
     */
    void setCustomHostname(const std::string& name) {
        UA_ServerConfig_setCustomHostname(m_pConfig, toUA_String(name)); // shallow copy
    }

    /**
     * Set the server URI (uniform resource identifier).
     * @param uri the new URI of the server.
     * @see UA_ApplicationDescription.
     */
    void setServerUri(const std::string& uri);

    /**
     * Enable Multi-cast DNS with a given hostname
     * Available only if UA_ENABLE_DISCOVERY_MULTICAST is defined.
     */
    void setMdnsServerName(const std::string& name);

    /**
     * Find an existing Server by its UA_Server pointer.
     * Used by call-backs to verify the server exists and is still running.
     * @param pUAServer a pointer on the Server underlying UA_Server.
     * @return a pointer on the matching Server
     */
    static Server* findServer(UA_Server* const pUAServer) { return s_serverMap[pUAServer]; }

    // Discovery

    /**
     * Register this server at the Discovery Server.
     * This should be called periodically to keep the server registered.
     * @param client used to call the RegisterServer.
     *        It must already be connected to the correct endpoint.
     * @param semaphoreFilePath optional parameter pointing to semaphore file.
     *        If the given file is deleted, the server will automatically be unregistered.
     *        This could be for example a pid file which is deleted if the server crashes.
     * @return true on success.
     * @see UA_Server_register_discovery
     */
    bool registerDiscovery(Client& client, const std::string& semaphoreFilePath = "");

    /**
     * Unregister this server from the Discovery Server
     * This should only be called when the server is shutting down.
     * @param client used to call the RegisterServer.
     *        It must already be connected to the correct endpoint.
     * @return true on success.
     * @see UA_Server_unregister_discovery
     */
    bool unregisterDiscovery(Client& client);

    /**
     * Adds a callback to periodically register the server with the LDS (local discovery server)
     *
     * When you manually unregister the server, you also need to cancel the
     * periodic callback, otherwise it will be automatically registered again.
     *
     * If you call this method multiple times for the same discoveryServerUrl, the older
     * periodic call-back will be removed.
     *
     * @param discoveryServerUrl where this server should register itself.
     * @param client used to call the RegisterServer. It must not yet be connected
     *        and will be connected for every register call to the given discoveryServerUrl.
     * @param periodicCallbackId return the created call-back.
     *        Map it to the discovery server URL in _discoveryList.
     * @param intervalMs specify the duration in ms between each register call.
     *        Set to 10 minutes by default (10*60*1000).
     * @param delayFirstRegisterMs indicates the delay for the first register call in ms.
     *        Set to 1s by default (1000ms).
     *        If it is 0, the first register call will be after intervalMs milliseconds,
     *        otherwise the server's first register will be after delayFirstRegisterMs.
     * @return true on success.
     * @see UA_Server_addPeriodicServerRegisterCallback
     */
    bool addPeriodicServerRegister(
        const std::string&  discoveryServerUrl,
        Client&             client,
        UA_UInt64&          periodicCallbackId,
        UA_UInt32           intervalMs           = 600 * 1000, // default to 10 minutes
        UA_UInt32           delayFirstRegisterMs = 1000);
    
    /**
     * Hook used to customize registerServerCallback() by derived classes.
     * By default do nothing.
     * @see registerServerCallback
     */
    virtual void registerServer(const UA_RegisteredServer* registeredServer) {
        OPEN62541_TRC
    }

    /**
     * Call-back called every time the server gets a register call.
     * This means that for every periodic server register the callback will be called.
     * Behavior can be customized by overriding the registerServer() hook.
     * @param registeredServer the server that (un)register itself to this one.
     * @param data
     * @see registerServer
     */
    static void registerServerCallback(const UA_RegisteredServer* registeredServer, void* data);

    /**
     * Activate the Server Registered trigger.
     * Called every time another server register or unregister with this one.
     * @see registerServer for behavior customization for this event.
     */
    void setRegisterServerCallback() {
        UA_Server_setRegisterServerCallback(server(), registerServerCallback, (void*)(this));
    }

    /**
     * Hook used to customize serverOnNetworkCallback() by derived classes.
     * By default do nothing.
     * @param serverOnNetwork
     * @param isServerAnnounce
     * @param isTxtReceived
     * @see registerServerCallback
     */
    virtual void serverOnNetwork(
        const UA_ServerOnNetwork* serverOnNetwork,
        UA_Boolean isServerAnnounce,
        UA_Boolean isTxtReceived) {
        OPEN62541_TRC
    }

    /**
     * Call-back called if another server is found through mDNS or deleted.
     * It will be called for any mDNS message from the remote server,
     * thus it may be called multiple times for the same instance.
     * Also the SRV and TXT records may arrive later,
     * therefore for the first call the server capabilities may not be set yet.
     * If called multiple times, previous data will be overwritten.
     * Behavior can be customized by overriding the serverOnNetwork() hook.
     * @param serverNetwork
     * @param isServerAnnounce
     * @param isTxtReceived
     * @param data
     */
    static void serverOnNetworkCallback(
        const UA_ServerOnNetwork* serverNetwork,
        UA_Boolean isServerAnnounce,
        UA_Boolean isTxtReceived,
        void* data);

    #ifdef UA_ENABLE_DISCOVERY_MULTICAST

    /**
     * Activate the mDNS Found Server on Network trigger.
     * Called every time another server is found through mDNS or deleted.
     * @see serverOnNetwork for behavior customization for this event.
     */
    void setServerOnNetworkCallback() {
        UA_Server_setServerOnNetworkCallback(server(), serverOnNetworkCallback, (void*)(this));
    }
    #endif

    /**
     * Start the server life-cycle.
     * Initialize it, iterate until stop() is called then terminate.
     */
    virtual void start();

    /**
     * stop the server (prior to delete) - do not try start-stop-start
     * If you need to be able to stop->start, you will need to write your own 
     * life-cycle.
     */
    virtual void stop() { m_running = false; }

    /**
     * Hook called after the server object has been created but before it runs.
     * load configuration files and set up the address space
     * create namespaces and endpoints
     * set up methods and stuff
     */
    virtual void initialise() {}

    /**
     * Hook called between server loop iterations
     * Use it to process thread event
     */
    virtual void process() {}

    /**
     * stop the server (prior to delete) - do not try start-stop-start
     * If you need to be able to stop->start, you will need to write your own 
     * life-cycle.
     */
    void shutdown();

    /**
     * Hook called before the server is closed
     */
    virtual void terminate();

    /**
     * Create and register the server in the call-back map.
     * Called by start() just before initialise().
     */
    virtual void create();

    /**
     * Core loop of the server process when running.
     * Only safe places to access server are in process() and callbacks
     */
    virtual void iterate();

    /**
     * Retrieve the context of a given node.
     * @param node
     * @param[out] pContext a pointer to found context of the given node.
     * @return true on success.
     */
    bool getNodeContext(const NodeId& node, NodeContext*& pContext);

    /**
     * Find a registered node context by its name.
     * @param name of the context to find.
     * @return a pointer on the found context, nullptr if the context wasn't registered/found.
     */
    static NodeContext* findContext(const std::string& name) {
        return RegisteredNodeContext::findRef(name); // not all node contexts are registered
    }
    /*!
        \brief getNodeContext
        \param n node if
        \param c pointer to context
        \return true on success
    */
    bool getNodeContext(NodeId &n, NodeContext *&c) {
        if (!server()) return false;
        void *p = (void *)(c);
        _lastError = UA_Server_getNodeContext(_server, n.get(), &p);
        return lastOK();
    }

    /**
     * Assign a given context to a node.
     * @warning The user has to ensure that the destructor call-backs still work.
     * @param node id of the node to modify
     * @param pContext the new context for the modified node.
     *        A context defines the Ctor, Dtor of the node and optionally:
     *        - the type Ctor and Dtor for object node
     *        - the value read/write methods for variable node
     *        - the data read/write methods for data source node
     * @return true on success.
     */
    bool setNodeContext(const NodeId& node, const NodeContext* pContext);

    /**
     * Delete a node and all its descendants
     * @param nodeId node to be deleted with its children
     * @return true on success.
     */
    bool deleteTree(const NodeId& nodeId);

    /**
     * Copy the descendants tree of a given UA_NodeId into a given PropertyTree.
     * Browse the tree from a given UA_NodeId (excluded from copying)
     * and add all its children as children of the given UANode.
     * @param nodeId parent of the nodes to copy.
     * @param node destination point in tree to which children nodes are added.
     * @return true on success.
     */
    bool browseTree(const UA_NodeId& nodeId, UANode* const node);

    /*!
        \brief readAttribute
        \param nodeId
        \param attributeId
        \param v data pointer
        \return true on success
    */
    bool readAttribute(const UA_NodeId *nodeId, UA_AttributeId attributeId, void *v) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  __UA_Server_read(_server, nodeId, attributeId, v);
        return lastOK();
    }
    /**
     * Copy the descendants tree of a NodeId into a UANodeTree.
     * Browse the tree from the given NodeId (excluded from copying)
     * and add all its children as children of the given UANodeTree's root.
     * Produces an addressable tree using dot separated browse path as key.
     * UANodeTree is a specialized PropertyTree using node name as key and NodeId as value.
     * @param nodeId source from which browsing starts in the source tree. It isn't copied, only its children.
     * @param tree the destination UANodeTree. Its root isn't modified.
     * @return true on success.
     */
    bool browseTree(const NodeId& nodeId, UANodeTree& tree) {
        return browseTree(nodeId, tree.rootNode());
    }

    /*!
        \brief writeAttribute
        \param nodeId
        \param attributeId
        \param attr_type
        \param attr data pointer
        \return true on success
    */
    bool writeAttribute(const UA_NodeId *nodeId, const UA_AttributeId attributeId, const UA_DataType *attr_type, const void *attr) {
        if (!server()) return false;
        WriteLock l(_mutex);
        _lastError =  __UA_Server_write(_server, nodeId, attributeId, attr_type, attr) == UA_STATUSCODE_GOOD;
        return lastOK();
    }
    /*!
        \brief mutex
        \return server mutex
    */
    ReadWriteMutex &mutex() {
        return _mutex; // access mutex - most accesses need a write lock
    }
    /**
     * Copy a NodeId and its descendants tree into a NodeIdMap.
     * NodeIdMap maps a serialized UA_NodeId as key with the UA_NodeId itself as value.
     * @param nodeId the starting point added to the map with its children.
     * @param map the destination NodeIdMap.
     * @return true on success.
     */
    bool browseTree(const NodeId& nodeId, NodeIdMap& map);

    /**
     * Copy only the non-duplicate children of a UA_NodeId into a NodeIdMap.
     * NodeIdMap maps a serialized UA_NodeId as key with the UA_NodeId itself as value.
     * @param nodeId parent of children to copy
     * @param map to fill
     * @return true on success.
     */
    bool browseChildren(const UA_NodeId& nodeId, NodeIdMap& map);

    /**
     * A simplified TranslateBrowsePathsToNodeIds based on the
     * SimpleAttributeOperand type (Part 4, 7.4.4.5).
     * This specifies a relative path using a list of BrowseNames instead of the
     * RelativePath structure. The list of BrowseNames is equivalent to a
     * RelativePath that specifies forward references which are subtypes of the
     * HierarchicalReferences ReferenceType. All Nodes followed by the browsePath
     * shall be of the NodeClass Object or Variable.
     * @param origin the node starting point
     * @param browsePathSize the number of level in the given browse path. Can be less than the actual number of elements.
     * @param browsePath the relative path to browse. Relative to origin.
     * @param[out] result the generated node_id stored in a BrowsePathResult
     * @see BrowsePathResult.
     * @return true on success.
     */
    bool browseSimplifiedBrowsePath(
        const NodeId&        origin,
        size_t               browsePathSize,
        const QualifiedName& browsePath,
        BrowsePathResult&    result);

    /**
     * create a browse path and add it to the tree
     * @warning not implemented.
     * @todo implement this knockoff of the above browseSimplifiedBrowsePath()
     * @param parent node to start with
     * @param path path to create
     * @param tree
     * @return true on success.
     */
    bool createBrowsePath(const NodeId& parent, const UAPath& path, UANodeTree& tree);

    /**
     * Add a new namespace to the server, thread-safely.
     * @param name of the new namespace.
     * @return the index of the new namespace.
     */
    UA_UInt16 addNamespace(const std::string& name);

    /**
     * Add a new method node to the server, thread-safely.
     * @param method point to the method to add.
     * @param browseName method name and description.
     * @param parent parent of the method node
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param nameSpaceIndex of new node, if non-zero otherwise namespace of parent
     * @return true on success.
     */
    bool addMethod(
        ServerMethod*   method,
        const std::string& browseName,
        const NodeId&   parent,
        const NodeId&   nodeId,
        NodeId&         newNode         = NodeId::Null,
        int             nameSpaceIndex  = 0);

    /**
     * Add a new Repeated call-back to the server.
     * @param id name of the call-back used to find it in the call-back map
     * @param pCallback function pointer on the call-back to add.
     */
    void addRepeatedCallback(const std::string& id, ServerRepeatedCallback* pCallback) {
        m_callbacks[id] = ServerRepeatedCallbackRef(pCallback);
    }

    /**
     * Create a new Repeated call-back in the server.
     * @param id name of the call-back used to find it in the call-back map
     * @param interval for the call-back periodic call repetition.
     * @param pCallback the call-back
     */
    void addRepeatedCallback(const std::string& id, int interval, ServerRepeatedCallbackFunc pCallback) {
        auto p = new ServerRepeatedCallback(*this, interval, pCallback);
        m_callbacks[id] = ServerRepeatedCallbackRef(p);
    }

    /**
     * Remove a repeated call-back from the server.
     * @param id name of the call-back used to find it in the call-back map.
     */
    void removeRepeatedCallback(const std::string& id) {
        m_callbacks.erase(id);
    }

    /**
     * Retrieve a repeated call-back from the server.
     * @param name of the call-back used to find it in the call-back map.
     * @return a reference to the found call-back
     */
    ServerRepeatedCallbackRef& repeatedCallback(const std::string& name) {
        return m_callbacks[name];
    }

    /*!
     * \brief addServerMethod
     * \param method - this must persist for the life time of the node !!!!!!
     * \param browseName
     * \param parent
     * \param nodeId
     * \param newNode
     * \param nameSpaceIndex
     * \return
     */
    bool addServerMethod(ServerMethod *method, const std::string &browseName,
                         NodeId &parent,  NodeId &nodeId,
                         NodeId &newNode,  int nameSpaceIndex = 0) {
        //
        if (!server()) return false;
        //
        if (nameSpaceIndex == 0) nameSpaceIndex = parent.nameSpaceIndex(); // inherit parent by default
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
            _lastError = UA_Server_addMethodNode(_server,
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
                                                 (void *)(method), // method context is reference to the call handler
                                                 newNode.isNull() ? nullptr : newNode.ref());


        }
        return lastOK();
    }
    /**
     * Create folder path first then add variable node to path's end leaf
     * @param start the reference node for the path
     * @param path relative to start
     * @param nameSpaceIndex
     * @param nodeId is a shallow copy - do not delete and is volatile
     * @return true on success.
     */
    bool createFolderPath(const NodeId& start, const Path& path, int nameSpaceIndex, NodeId& nodeId);

    /**
     * Get the node id from the path of browse names in the given namespace. Tests for node existence
     * @param start the reference node for the path
     * @param path relative to start
     * @param nodeId the found node
     * @return true on success, otherwise nodeId refer to the last node matching the path.
     */
    bool nodeIdFromPath(const NodeId& start, const Path& path, NodeId& nodeId);

    /**
     * Get the child with a specific name of a given node.
     * @param start the parent node
     * @param childName the name of the child node to find.
     * @param[out] the found node.
     * @return true on success.
     */
    bool getChild(const NodeId& start, const std::string& childName, NodeId& ret);

    /**
     * Get the list of children of a node, thread-safely.
     * @param node the id of the node.
     * @return a vector of UA_NodeId containing the list of all the node's children.
     */
    UANodeIdList getChildrenList(const UA_NodeId& node);

    /**
     * Add a children Folder node in the server, thread-safely.
     * @param parent parent node
     * @param childName browse name of the folder node
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param nameSpaceIndex of the new node, if non-zero otherwise namespace of parent
     * @return true on success.
     */
    bool addFolder(
        const NodeId&       parent,
        const std::string&  childName,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        int                 nameSpaceIndex  = 0);

    /**
     * Add a new variable node in the server, thread-safely.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param value variant with the value for the new node. Also specifies its type.
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param context customize how the node will be created if not null.
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    bool addVariable(
        const NodeId&       parent,
        const std::string&  childName,
        const Variant&      value,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        NodeContext*        context         = nullptr,
        int                 nameSpaceIndex  = 0);

    /**
     * Add a variable of the given type, thread-safely.
     * @param T specify the UA_ built-in type.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param context customize how the node will be created if not null.
     * @param newNode receives new node if not null
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    template<typename T>
    bool addVariable(
        const NodeId&       parent,
        const std::string&  childName,
        const std::string&  context,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        int                 nameSpaceIndex  = 0) {
        if (auto cp = findContext(context)) {
            Variant val(T());
            return addVariable(parent, childName, val, nodeId,  newNode, cp, nameSpaceIndex);
        }
        return false;
    }

    /**
     * Add a new historical variable node in the server, thread-safely.
     * Same as addVariable but with the historizing attribute set to true
     * and the read history access.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param value variant with the value for the new node. Also specifies its type.
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param context customize how the node will be created if not null.
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    bool addHistoricalVariable(
        const NodeId&       parent,
        const std::string&  childName,
        const Variant&      value,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        NodeContext*        context         = nullptr,
        int                 nameSpaceIndex  = 0);

    /**
     * Add a new historical variable node of the given type in the server, thread-safely.
     * Same as addVariable but with the historizing attribute set to true
     * and the read history access.
     * @param T specify the UA_ built-in type.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param context customize how the node will be created if not null.
     * @param newNode receives new node if not null
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    template<typename T>
    bool addHistoricalVariable(
        const NodeId&       parent,
        const std::string&  childName,
        const std::string&  contextName,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        int                 nameSpaceIndex  = 0) {
        if (NodeContext* context = findContext(contextName)) {
            Variant val(T());
            return addHistoricalVariable(
                parent, childName, val, nodeId, newNode, context, nameSpaceIndex);
        }
        return false;
    }
    //
    //
    /*!
        \brief browseName
        \param nodeId
        \return
    */
    bool  browseName(NodeId &nodeId, std::string &s, int &ns) {
        if (!_server) throw std::runtime_error("Null server");
        QualifiedName outBrowseName;
        if (UA_Server_readBrowseName(_server, nodeId, outBrowseName) == UA_STATUSCODE_GOOD) {
            s =   toString(outBrowseName.get().name);
            ns = outBrowseName.get().namespaceIndex;
        }
        return lastOK();
    }

    /**
     * Add a new property node in the server, thread-safely.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param value variant with the value for the new node. Also specifies its type.
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param context customize how the node will be created if not null.
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    bool addProperty(
        const NodeId&       parent,
        const std::string&  childName,
        const Variant&      value,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        NodeContext*        context         = nullptr,
        int                 nameSpaceIndex  = 0);

    /*!
        \brief setBrowseName
        \param nodeId
        \param nameSpaceIndex
        \param name
    */
    void setBrowseName(NodeId &nodeId, int nameSpaceIndex, const std::string &name) {
        if (!server()) return;
        QualifiedName newBrowseName(nameSpaceIndex, name);
        WriteLock l(_mutex);
        UA_Server_writeBrowseName(_server, nodeId, newBrowseName);
    }
    /**
     * Add a new property node of the given type in the server, thread-safely.
     * @param T specify the UA_ built-in type.
     * @param parent specify the parent node containing the added node
     * @param childName browse name of the new node
     * @param value variant with the value for the new node. Also specifies its type.
     * @param nodeId assigned node id or NodeId::Null for auto assign
     * @param newNode receives new node if not null
     * @param context customize how the node will be created if not null.
     * @param nameSpaceIndex of the new node if non-zero, otherwise namespace of parent
     * @return true on success.
     */
    template <typename T>
    bool addProperty(
        const NodeId&       parent,
        const std::string&  childName,
        const T&            value,
        const NodeId&       nodeId          = NodeId::Null,
        NodeId&             newNode         = NodeId::Null,
        NodeContext*        context         = nullptr,
        int                 nameSpaceIndex  = 0) {
        Variant val(value);
        return addProperty(
            parent, key, val, nodeId, newNode, context, nameSpaceIndex);
    }

    /**
     * Deletes a node and optionally all references leading to the node, thread-safely.
     * @param nodeId to delete
     * @param deleteReferences specify if the references to this node must also be deleted.
     * @return true on success
     */
    bool deleteNode(const NodeId& nodeId, bool deleteReferences);

    // Add Nodes thread-safe thin wrapper around UA functions

    /**
     * Add a new variable type node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param typeDefinition nodeId containing the definition of the type of the added node.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addVariableNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const NodeId&           typeDefinition,
        const VariableAttributes& attr,
        NodeId&                 outNewNodeId            = NodeId::Null,
        NodeContext*            instantiationCallback   = nullptr);

    /**
     * Add a new variable type node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param typeDefinition nodeId containing the definition of the type of the added node.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addVariableTypeNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const NodeId&           typeDefinition,
        const VariableTypeAttributes& attr,
        NodeId&                 outNewNodeId = NodeId::Null,
        NodeContext*            instantiationCallback = nullptr);

    /**
     * Add a new object node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param typeDefinition nodeId containing the definition of the type of the added node.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addObjectNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const NodeId&           typeDefinition,
        const ObjectAttributes& attr,
        NodeId&                 outNewNodeId          = NodeId::Null,
        NodeContext*            instantiationCallback = nullptr);

    /**
     * Add a new object type node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addObjectTypeNode(
        const NodeId&               requestedNewNodeId,
        const NodeId&               parentNodeId,
        const NodeId&               referenceTypeId,
        const QualifiedName&        browseName,
        const ObjectTypeAttributes& attr,
        NodeId&                     outNewNodeId            = NodeId::Null,
        NodeContext*                instantiationCallback   = nullptr);

    /**
     * Add a new view node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addViewNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const ViewAttributes&   attr,
        NodeId&                 outNewNodeId = NodeId::Null,
        NodeContext*            instantiationCallback = nullptr);

    /**
     * Add a new reference type node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addReferenceTypeNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const ReferenceTypeAttributes& attr,
        NodeId&                 outNewNodeId            = NodeId::Null,
        NodeContext*            instantiationCallback   = nullptr);

    /**
     * Add a new data type node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addDataTypeNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const DataTypeAttributes& attr,
        NodeId&                 outNewNodeId = NodeId::Null,
        NodeContext*            instantiationCallback = nullptr);

    /**
     * Add a new data source variable node in the server, thread-safely.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parentNodeId parent node id of the added node.
     * @param referenceTypeId specify the relation between the added node and its children.
     *        ie: NodeId::HasSubType or NodeId::HasComponent.
     * @param browseName browse name of the added node, includes the namespace.
     * @param typeDefinition nodeId containing the definition of the type of the added node.
     * @param attr the attributes of the added node.
     * @param outNewNodeId receives new node if not null
     * @param instantiationCallback customize how the node will be created if not null.
     * @return true on success.
     */
    bool addDataSourceVariableNode(
        const NodeId&           requestedNewNodeId,
        const NodeId&           parentNodeId,
        const NodeId&           referenceTypeId,
        const QualifiedName&    browseName,
        const NodeId&           typeDefinition,
        const VariableAttributes& attr,
        const DataSource&       dataSource,
        NodeId&                 outNewNodeId            = NodeId::Null,
        NodeContext*            instantiationCallback   = nullptr);

    /**
     * Add in a given node a reference to another node, thread-safely.
     * Reference can be bidirectional or hierarchical (unidirectional).
     * Bidirectional typically target a standard node specifying additional parameter.
     * Hierarchical target a node like a child node. The meaning of the reference is its type itself.
     * @param sourceId id of the source node that will have the new reference.
     * @param referenceTypeId specify the type of reference.
     *        ie: NodeId::HasModellingRule (bi).
     * @param targetId URI of the node specifying additional parameter.
     *        ie: ExpandedNodeId::ModellingRuleMandatory
     * @param isForward if true ????? magic ?????
     * @return true on success.
     * @see https://open62541.org/doc/current/nodestore.html?highlight=reference#referencetypenode
     */
    bool addReference(
        const NodeId&         sourceId,
        const NodeId&         referenceTypeId,
        const ExpandedNodeId& targetId,
        bool                  isForward);

    /**
     * Add a reference making a given node a mandatory member
     * in all instance of an object type, thread-safely.
     * If the node isn't explicitly created, it will be created with default value.
     * @param nodeId id of the mandatory node.
     * @return true on success.
     */
    bool markMandatory(const NodeId& nodeId);

    /**
     * Remove a reference from a given node.
     * @param sourceNodeId the node with the reference to delete
     * @param referenceTypeId specify the type of reference.
     *        ie: NodeId::HasModellingRule.
     * @param isForward
     * @param targetNodeId
     * @param deleteBidirectional if true delete the reference to the source in the target.
     * @return true on success.
     */
    bool deleteReference(
        const NodeId&   sourceNodeId,
        const NodeId&   referenceTypeId,
        bool            isForward,
        const ExpandedNodeId& targetNodeId,
        bool            deleteBidirectional);


    /**
     * Add an object node with default values of a given type, thread-safely.
     * @param name specify the display name of the new node.
     * @param requestedNewNodeId assigned node id or NodeId::Null for auto assign
     * @param parent id of the new node parent.
     * @param typeId id of the object Type node specify the type of the instance, its structure and default values.
     * @param outNewNodeId receives new node if not null
     * @param context customize how the node will be created if not null.
     * @return true on success.
     */
    bool addInstance(
        const std::string&  name,
        const NodeId&       requestedNewNodeId,
        const NodeId&       parent,
        const NodeId&       typeId,
        NodeId&             outNewNodeId    = NodeId::Null,
        NodeContext*        context         = nullptr);

    /**
     * Creates a node representation of an event, thread-safely.
     * @param eventType The type of the event for which a node should be created
     * @param outNodeId The NodeId of the newly created node for the event
     * @return true on success.
     */
    bool createEvent(const NodeId& eventType, NodeId& outNodeId);

    /**
     * Triggers an event by applying EventFilters represented by a given node
     * and adding the event to the appropriate queues, thread-safely.
     * @param eventNodeId The id of the node representation of the event to trigger.
     * @param[out] outEvent the Id of the triggered event.
     * @param deleteEventNode Specifies whether the node representation of the event should be deleted
     * @return true on success.
     * @see example\TestEventServer\testmethod.cpp for usage example.
     */
    bool triggerEvent(
        const NodeId&   eventNodeId,
        UA_ByteString*  outEventId      = nullptr,
        bool            deleteEventNode = true);

    /**
     * Add a new Event type, thread-safely.
     * Create a new object type node with the event sub-type.
     * @param name the display name of the new event type
     * @param[out] eventType a ref to the created node, if not null.
     * @param description description of the event. If empty, name is used.
     * @return true on success.
     * @see example\TestEventServer\testmethod.cpp for usage example.
     */
    bool addNewEventType(
        const std::string&  name,
        NodeId&             outEventType,
        const std::string&  description = std::string());

    /**
     * Create an event of a given type, thread-safely.
     * @param[out] outId node of the created event.
     * @param[out] eventType type of the event. Held in an object type node.
     * @param eventMessage message of the event
     * @param eventSourceName, name of the event emitter.
     * @param eventSeverity severity level of the event, 100 by default.
     * @param eventTime time stamp of the event, now by default.
     * @return true on success.
     * @see example\TestEventServer\testmethod.cpp for usage example.
     */
    bool setUpEvent(
        NodeId&             outId,
        NodeId&             outEventType,
        const std::string&  eventMessage,
        const std::string&  eventSourceName,
        int                 eventSeverity = 100,
        UA_DateTime         eventTime     = UA_DateTime_now());

    /**
     * Call a given server method, thread-safely.
     * @param request the method to call
     * @param[out] ret the result of the request
     * @return true on success.
     */
    bool call(const CallMethodRequest& request, CallMethodResult& ret);

    /**
     * Translate a given BrowsePath to an array of NodeIds, thread-safely.
     * @param path specify the target via a starting node and a path relative to it.
     * @param[out] result a BrowsePathResult containing an error code and an array of nodes.
     *        The array contains the nods matching the path, starting node excluded.
     * @return true on success.
     * @see UA_Server_translateBrowsePathToNodeIds
     */
    bool translateBrowsePathToNodeIds(const BrowsePath& path, BrowsePathResult& result);
    /*!
        \brief variable
        \param nodeId
        \param value
        \return true on success
    */
    bool  variable(NodeId &nodeId,  Variant &value) {
        if (!server()) return false;

        // outValue is managed by caller - transfer to output value
        value.null();
        WriteLock l(_mutex);
        UA_Server_readValue(_server, nodeId, value.ref());
        return lastOK();
    }
    /**
     * Test if last operation executed successfully.
     * @return true if last error code is UA_STATUSCODE_GOOD
     */
    bool lastOK() const { return _lastError == UA_STATUSCODE_GOOD; }

    ///////////////////////////////////////////////////////////////////////////
    // Attributes accessors
    ///////////////////////////////////////////////////////////////////////////

    /**
     * Primitive used to retrieve one attribute of a given node, thread-safely.
     * @warning Don't use it directly. Use one of the 19 typed version instead, like readNodeId().
     * There are up to 22 possible node attributes.
     * @param nodeId to read.
     * @param attributeId identify the attribute to retrieve.
     * @param[out] value of the attribute, must be casted to attribute type.
     *             Some are UA_Boolean, U_NodeId, etc...
     * @see UA_AttributeId for the list of possible attribute id.
     * @return true on success.
     */
    bool readAttribute(
        const UA_NodeId* nodeId,
        UA_AttributeId attributeId,
        void* value);
    /**
     * Read the Id attribute of a given node, thread-safely.
     * Permit to test if the node exists, since its id is already known
     * @param nodeId of the node to read.
     * @param[out] outNodeId the id of the node.
     * @return true on success.
     */
    bool readNodeId(const UA_NodeId& nodeId, NodeId& outNodeId) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_NODEID, outNodeId);
    }

    /**
     * Read the Node Class attribute of a given node, thread-safely.
     * @param nodeId of the node to read.
     * @param[out] outNodeClass the node type (object, variable, method, object type, variable type, reference, data, view)
     * @see UA_NodeClass the enum mask encoding the node type
     * @return true on success.
     */
    bool readNodeClass(const UA_NodeId& nodeId, UA_NodeClass& outNodeClass) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_NODECLASS, &outNodeClass);
    }

    /**
     * Read the Browse Name attribute of a given node, thread-safely.
     * @param nodeId of the node to read.
     * @param[out] outBrowseName the browse name of the node.
     * @return true on success.
     */
    bool readBrowseName(const UA_NodeId& nodeId, QualifiedName& outBrowseName) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_BROWSENAME, outBrowseName);
    }

    /**
     * Get the name and namespace index of a given node
     * @param[in] nodeId of the node to read.
     * @param[out] name the qualified name of the node.
     * @param[out] ns the namespace index of the node.
     * @return true on success. On failure the output params are unchanged.
     */
    bool readBrowseName(const NodeId& nodeId, std::string& name, int& ns);

    /**
     * Read the Display Name attribute of a given node, thread-safely.
     * @param nodeId of the node to read.
     * @param[out] outDisplayName the display name of the node, translated in the local language.
     * @return true on success.
     */
    bool readDisplayName(const UA_NodeId& nodeId, LocalizedText& outDisplayName) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_DISPLAYNAME, outDisplayName);
    }

    /**
     * Read the Description attribute of a given node, thread-safely.
     * @param nodeId of the node to read.
     * @param[out] outDescription the node description, translated in the local language.
     * @return true on success.
     */
    bool readDescription(const UA_NodeId& nodeId, LocalizedText& outDescription) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_DESCRIPTION, outDescription);
    }

    /**
     * Read the Write Mask attribute of a given node, thread-safely.
     * @param nodeId of the node to read.
     * @param[out] outWriteMask specify which attribute of the node can be modified
     * @return true on success.
     */
    bool readWriteMask(const UA_NodeId& nodeId, UA_UInt32& outWriteMask) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_WRITEMASK, &outWriteMask);
    }

    /**
     * Read the Is Abstract attribute of a given node, thread-safely.
     * Only for Data, Object Type, Variable Type or Reference nodes.
     * @param nodeId of the node to read.
     * @param[out] outIsAbstract true if the node is abstract.
     *             ie: UA_Numeric is abstract, UA_Int32 is not and is a concrete implementation of UA_Numeric.
     * @return true on success.
     */
    bool readIsAbstract(const UA_NodeId& nodeId, UA_Boolean& outIsAbstract) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_ISABSTRACT, &outIsAbstract);
    }

    /**
     * Read the Symmetric attribute of a given node, thread-safely.
     * Only for Reference nodes.
     * @param nodeId of the node to read.
     * @param[out] outSymmetric true if the reference applies both to the child and parent.
     * @return true on success.
     */
    bool readSymmetric(const UA_NodeId& nodeId, UA_Boolean& outSymmetric) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_SYMMETRIC, &outSymmetric);
    }

    /**
     * Read the Inverse Name attribute of a given node, thread-safely.
     * Only for Reference nodes.
     * @param nodeId of the node to read.
     * @param[out] outInverseName
     * @return true on success.
     */
    bool readInverseName(const UA_NodeId& nodeId, LocalizedText& outInverseName) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_INVERSENAME, outInverseName);
    }

    /**
     * Read the Contains No Loop attribute of a given node, thread-safely.
     * Only for View nodes.
     * @param nodeId of the node to read.
     * @param[out] outContainsNoLoops
     * @return true on success.
     */
    bool readContainsNoLoop(const UA_NodeId& nodeId, UA_Boolean& outContainsNoLoops) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_CONTAINSNOLOOPS, &outContainsNoLoops);
    }

    /**
     * Read the Event Notifier attribute of a given node, thread-safely.
     * Only for Object and View nodes.
     * @param nodeId of the node to read.
     * @param[out] outEventNotifier
     * @return true on success.
     */
    bool readEventNotifier(const UA_NodeId& nodeId, UA_Byte& outEventNotifier) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_EVENTNOTIFIER, &outEventNotifier);
    }

    /**
     * Read the Value attribute of a given node, thread-safely.
     * Only for Variable and Variable Type nodes.
     * @param nodeId of the node to read.
     * @param[out] outValue the value of the variable node.
     * @return true on success.
     */
    bool readValue(const UA_NodeId& nodeId, Variant& outValue) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_VALUE, outValue);
    }

    /**
     * Read the Data Type attribute of a given node, thread-safely.
     * Only for Variable and Variable Type nodes.
     * @param nodeId of the node to read.
     * @param[out] outDataType the type of the data of the variable node.
     * @return true on success.
     */
    bool readDataType(const UA_NodeId& nodeId, NodeId& outDataType) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_DATATYPE, outDataType);
    }

    /**
     * Read the Value Rank attribute of a given node, thread-safely.
     * Only for Variable and Variable Type nodes.
     * @param nodeId of the node to read.
     * @param[out] outValueRank indicates whether the variable is an array
     *             and how many dimensions the array has.
     * @return true on success.
     * @see https://open62541.org/doc/current/nodestore.html?highlight=writemask#value-rank
     */
    bool readValueRank(const UA_NodeId& nodeId, UA_Int32& outValueRank) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_VALUERANK, &outValueRank);
    }

    /**
     * Read the Array Dimensions attribute of a given node, thread-safely.
     * Only for Variable and Variable Type nodes.
     * @param nodeId of the node to read.
     * @param[out] outArrayDimensions a variant with an int32 array containing the size of each dimension
     *             ie. if ValueRank is 3, ArrayDimensions can be something link {2, 2, 3}
     * @return true on success.
     * @see https://open62541.org/doc/current/nodestore.html?highlight=writemask#array-dimensions
     */
    bool readArrayDimensions(const UA_NodeId& nodeId, Variant& outArrayDimensions) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_ARRAYDIMENSIONS, outArrayDimensions);
    }

    /**
     * Read the Access Level attribute of a given node, thread-safely.
     * Only for Variable nodes.
     * @param nodeId of the node to read.
     * @param[out] outAccessLevel a mask specifying if the value can be read/written, its history read/written, etc...
     * @return true on success.
     * @see UA_ACCESSLEVELMASK_READ, UA_ACCESSLEVELMASK_WRITE, UA_ACCESSLEVELMASK_HISTORYREAD, UA_ACCESSLEVELMASK_HISTORYWRITE
     *      UA_ACCESSLEVELMASK_SEMANTICCHANGE, UA_ACCESSLEVELMASK_STATUSWRITE, UA_ACCESSLEVELMASK_TIMESTAMPWRITE
     */
    bool readAccessLevel(const UA_NodeId& nodeId, UA_Byte& outAccessLevel) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_ACCESSLEVEL, &outAccessLevel);
    }

    /**
     * Read the Minimum Sampling Interval attribute of a given node, thread-safely.
     * Only for Variable nodes.
     * @param nodeId of the node to read.
     * @param[out] outMinInterval the value Minimum Sampling Interval.
     * @return true on success.
     */
    bool readMinimumSamplingInterval(const UA_NodeId& nodeId, UA_Double& outMinInterval) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL, &outMinInterval);
    }

    /**
     * Read the Historizing attribute of a given node, thread-safely.
     * Only for Variable nodes.
     * @param nodeId of the node to read.
     * @param[out] outHistorizing true if the variable node is keeping its value history.
     * @return true on success.
     */
    bool readHistorizing(const UA_NodeId& nodeId, UA_Boolean& outHistorizing) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_HISTORIZING, &outHistorizing);
    }

    /**
     * Read the Executable attribute of a given node, thread-safely.
     * Only for method nodes.
     * @param nodeId of the node to read.
     * @param[out] outExecutable true if the method is active and can be executed.
     * @return true on success.
     */
    bool readExecutable(const UA_NodeId& nodeId, UA_Boolean& outExecutable) {
        return readAttribute(&nodeId, UA_ATTRIBUTEID_EXECUTABLE, &outExecutable);
    }

    ///////////////////////////////////////////////////////////////////////////

    /**
     * Primitive used to write one attribute of a given node, thread-safely.
     * @warning Don't use it directly. Use one of the 13 typed version instead, like writeValue().
     * There are up to 22 possible node attributes.
     * @param nodeId to write
     * @param attributeId identify the attribute to write. 
     * @see UA_AttributeId for the list of possible attribute id.
     * @param attr_type pointer to the attribute built-in type. Normally stored in the UA_TYPES array.
     * @see UA_TYPES for the list of possible type.
     * @param attr void pointer to the data to write.
     * @return true on success.
     */
    bool writeAttribute(
        const UA_NodeId*     nodeId,
        const UA_AttributeId attributeId,
        const UA_DataType*   attr_type,
        const void*          attr);

    /**
     * Set the BrowseName attribute of the given node, thread-safely.
     * @param nodeId
     * @param browseName
     * @return true on success.
     */
    bool setBrowseName(const NodeId& nodeId, const QualifiedName& browseName) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_BROWSENAME,
                                &UA_TYPES[UA_TYPES_QUALIFIEDNAME], browseName);
    }

    /**
     * Set the BrowseName of a node with the given namespace and name, thread-safely.
     * @param nodeId to modify
     * @param nameSpaceIndex part of the new browse name
     * @param name
     * @return true on success.
     */
    bool setBrowseName(const NodeId& nodeId, int nameSpaceIndex, const std::string& name) {
        return setBrowseName(nodeId, {nameSpaceIndex, name});
    }

    /**
     * Set the DisplayName attribute of the given node, thread-safely.
     * @param nodeId
     * @param displayName
     * @return true on success.
     */
    bool setDisplayName(const NodeId& nodeId, const LocalizedText& displayName) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_DISPLAYNAME,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], displayName);
    }

    /**
     * Set the Description attribute of the given node, thread-safely.
     * @param nodeId
     * @param description
     * @return true on success.
     */
    bool setDescription(const NodeId& nodeId, const LocalizedText& description) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_DESCRIPTION,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], description);
    }

    /**
     * Set the WriteMask attribute of the given node, thread-safely.
     * @param nodeId
     * @param writeMask
     * @return true on success.
     */
    bool setWriteMask(const NodeId& nodeId, const UA_UInt32 writeMask) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_WRITEMASK,
                                &UA_TYPES[UA_TYPES_UINT32], &writeMask);
    }

    /**
     * Set the IsAbstract attribute of the given node, thread-safely.
     * @param nodeId
     * @param isAbstract
     * @return true on success.
     */
    bool setIsAbstract(const NodeId& nodeId, const UA_Boolean isAbstract) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_ISABSTRACT,
                                &UA_TYPES[UA_TYPES_BOOLEAN], &isAbstract);
    }

    /**
     * Set the InverseName attribute of the given node, thread-safely.
     * @param nodeId
     * @param inverseName
     * @return true on success.
     */
    bool setInverseName(const NodeId& nodeId, const UA_LocalizedText inverseName) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_INVERSENAME,
                                &UA_TYPES[UA_TYPES_LOCALIZEDTEXT], &inverseName);
    }

    /**
     * Set the EventNotifier attribute of the given node, thread-safely.
     * @param nodeId
     * @param eventNotifier
     * @return true on success.
     */
    bool setEventNotifier(const NodeId& nodeId, const UA_Byte eventNotifier) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_EVENTNOTIFIER,
                                &UA_TYPES[UA_TYPES_BYTE], &eventNotifier);
    }
                \param value
    /**
     * Set the Value attribute of the given node, thread-safely.
     * @param nodeId
     * @param value
     * @return true on success.
     */
    bool setValue(const NodeId& nodeId, const Variant& value) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_VALUE,
                                &UA_TYPES[UA_TYPES_VARIANT], value);
    }

    /**
     * Set the DataType attribute of the given node, thread-safely.
     * @param nodeId
     * @param dataType
     * @return true on success.
     */
    bool setDataType(const NodeId& nodeId, const NodeId& dataType) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_DATATYPE,
                                &UA_TYPES[UA_TYPES_NODEID], dataType);
    }

    /**
     * Set the ValueRank attribute of the given node, thread-safely.
     * @param nodeId
     * @param valueRank
     * @return true on success.
     */
    bool setValueRank(const NodeId& nodeId, const UA_Int32 valueRank) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_VALUERANK,
                                &UA_TYPES[UA_TYPES_INT32], &valueRank);
    }

    /**
     * Set the ArrayDimensions attribute of the given node, thread-safely.
     * @param nodeId
     * @param arrayDimensions
     * @return true on success.
     */
    bool setArrayDimensions(const NodeId& nodeId, const Variant& arrayDimensions) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_VALUE,
                                &UA_TYPES[UA_TYPES_VARIANT], &arrayDimensions);
    }

    /**
     * Set the AccessLevel attribute of the given node, thread-safely.
     * @param nodeId
     * @param accessLevel
     * @return true on success.
     */
    bool setAccessLevel(const NodeId& nodeId, const UA_Byte accessLevel) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_ACCESSLEVEL,
                                &UA_TYPES[UA_TYPES_BYTE], &accessLevel);
    }

    /**
     * Set the MinimumSamplingInterval attribute of the given node, thread-safely.
     * @param nodeId
     * @param miniumSamplingInterval
     * @return true on success.
     */
    bool setMinimumSamplingInterval(const NodeId& nodeId, const UA_Double miniumSamplingInterval) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_MINIMUMSAMPLINGINTERVAL,
                                &UA_TYPES[UA_TYPES_DOUBLE], &miniumSamplingInterval);
    }
    /**
     * Set the Executable attribute of the given node, thread-safely.
     * @param nodeId
     * @param executable
     * @return true on success.
     */
    bool setExecutable(const NodeId& nodeId, const UA_Boolean executable) {
        return writeAttribute(nodeId, UA_ATTRIBUTEID_EXECUTABLE,
                                &UA_TYPES[UA_TYPES_BOOLEAN], &executable);
    }

    /*!
 * \brief writeObjectProperty
 * \param objectId
 * \param propertyName
 * \param value
 * \return true on success
 */
    bool writeObjectProperty(
        const NodeId& objectId, 
        const QualifiedName& propertyName, 
        const Variant& value)
    {
        return UA_Server_writeObjectProperty(server(), objectId, propertyName, value) 
            == UA_STATUSCODE_GOOD;
    }


    template <typename T>
    /*!
     * \brief writeObjectProperty
     * \param objectId
     * \param propertyName
     * \param value
     * \return
     */
    bool writeObjectProperty(
        const NodeId& objectId, 
        const std::string& propertyName, 
        const T& value)
    {
        Variant v(value);
        QualifiedName qn(0, propertyName);
        return  writeObjectProperty(objectId, qn, v);
    }
    /*!
     * \brief writeObjectProperty_scalar
     * \param objectId
     * \param propertyName
     * \param value
     * \param type
     * \return true on success
     */
    bool writeObjectProperty_scalar(
        const NodeId& objectId, 
        const std::string& propertyName, 
        const void* value, 
        const UA_DataType* type)
    {
        QualifiedName qn(0, propertyName);
        return UA_Server_writeObjectProperty_scalar(server(), objectId, qn, value, type) 
            == UA_STATUSCODE_GOOD;
    }
    // Some short cuts
    /**
     * Set the Enable attribute of the given node, thread-safely.
     * @param nodeId
     * @return true on success.
     */
    bool setEnable(const NodeId& nodeId);

    /**
     * setReadOnly
     * @param nodeId
     * @param historyEnable
     * @return true on success.
     */
    bool setReadOnly(const NodeId& nodeId, bool historyEnable = false);

    
    /**
     * Update the server Certificate
     * @param oldCertificate
     * @param newCertificate
     * @param newPrivateKey
     * @param closeSessions
     * @param closeSecureChannels
     * @return true on success.
     */
    bool updateCertificate(
        const UA_ByteString* oldCertificate,
        const UA_ByteString* newCertificate,
        const UA_ByteString* newPrivateKey,
        bool                 closeSessions       = true,
        bool                 closeSecureChannels = true);
                                                         referenceTypeId,
    /**
     * Allow insertion, replacement, update of historical data
     * @param sessionId
     * @param sessionContext
     * @param nodeId
     * @param performInsertReplace
     * @param value
     * @return true if allowed.
     */
    bool accessControlAllowHistoryUpdateUpdateData(
        const NodeId&   sessionId,
        void*           sessionContext,
        const NodeId&   nodeId,
        UA_PerformUpdateType performInsertReplace,
        UA_DataValue&   value);
                                                   referenceTypeId,
    /**
     * Allow suppression of historical data
     * @param sessionId
     * @param sessionContext
     * @param nodeId
     * @param performInsertReplace
     * @param value
     * @return true if allowed.
     */
    bool accessControlAllowHistoryUpdateDeleteRawModified(
        const NodeId&   sessionId,
        void*           sessionContext,
        const NodeId&   nodeId,
        UA_DateTime     startTimestamp,
        UA_DateTime     endTimestamp,
        bool            isDeleteModified = true);

    // Access control hook
    /**
     * Hook used to customize allowAddNodeHandler() call-back by derived classes.
     * Allow a client to add nodes.
     * By default allowed.
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return true if allowed.
     */
    virtual bool allowAddNode(
        UA_AccessControl* ac,
        const UA_NodeId* sessionId,
        void* sessionContext,
        const UA_AddNodesItem* item) {
        return true;
    }

    /**
     * Hook used to customize allowAddReferenceHandler() call-back by derived classes.
     * Allow a client to add Reference nodes.
     * By default allowed.
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return true if allowed.
     */
    virtual bool allowAddReference(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_AddReferencesItem* item) { return true; }

    /**
     * Hook used to customize allowDeleteNodeHandler() call-back by derived classes.
     * Allow a client to delete nodes.
     * By default not allowed.
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return true if allowed.
     */
    virtual bool allowDeleteNode(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_DeleteNodesItem* item) { return false; }
    /**
     * Hook used to customize allowDeleteReferenceHandler() call-back by derived classes.
     * Allow a client to delete Reference nodes.
     * By default allowed.
     * @param ac
     * @param sessionId
     * @param sessionContext
     * @param item
     * @return true if allowed.
     */
    virtual bool allowDeleteReference(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_DeleteReferencesItem* item) { return true; }
    /**
     * Hook used to customize activateSessionHandler() call-back by derived classes.
     * Called before activating a Session.
     * By default do nothing.
     * @return -1 if the server doesn't exist, UA_STATUSCODE_GOOD on success, UA_STATUSCODE_<ErrorName> otherwise.
     */
    virtual UA_StatusCode activateSession(
        UA_AccessControl*           ac,
        const UA_EndpointDescription* endpointDescription,
        const UA_ByteString*        secureChannelRemoteCertificate,
        const UA_NodeId*            sessionId,
        const UA_ExtensionObject*   userIdentityToken,
        void**                      sessionContext)  { return UA_STATUSCODE_BADSESSIONIDINVALID; }
    /**
     * Hook used to customize closeSessionHandler() call-back by derived classes.
     * De-authenticate a session and cleanup.
     * By default do nothing.
     */
    virtual void closeSession(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext)   {}

    /**
     * Hook used to customize getUserRightsMaskHandler() call-back by derived classes.
     * Access control for all nodes
     * By default nothing is allowed.
     * @return a mask of UA_ACCESSLEVELMASK_...
     */
    virtual uint32_t getUserRightsMask(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_NodeId*    nodeId,
        void*               nodeContext) { return 0; }

    /**
     * Hook used to customize getUserAccessLevelHandler() call-back by derived classes.
     * Additional access control for variable nodes
     * By default nothing is allowed.
     * @return a mask of UA_ACCESSLEVELMASK_...
     */
    virtual uint8_t getUserAccessLevel(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_NodeId*    nodeId,
        void*               nodeContext) { return 0; }

    /**
     * Hook used to customize getUserExecutableHandler() call-back by derived classes.
     * Additional access control for method nodes.
     * By default not allowed.
     * @return true if allowed.
     */
    virtual bool getUserExecutable(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_NodeId*    methodId,
        void*               methodContext) { return false; }

    /**
     * Hook used to customize getUserExecutableOnObjectHandler() call-back by derived classes.
     * Additional access control for calling a method node
     * in the context of a specific object.
     * By default not allowed.
     * @return true if allowed.
     */
    virtual bool getUserExecutableOnObject(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_NodeId*    methodId,
        void*               methodContext,
        const UA_NodeId*    objectId,
        void*               objectContext) { return false; }
    
    /**
     * Hook used to customize allowHistoryUpdateUpdateDataHandler() call-back by derived classes.
     * Allow insert, replace, update of historical data.
     * By default not allowed.
     * @return true if allowed.
     */
    virtual bool allowHistoryUpdateUpdateData(
        UA_AccessControl*       ac,
        const UA_NodeId*        sessionId,
        void*                   sessionContext,
        const UA_NodeId*        nodeId,
        UA_PerformUpdateType    performInsertReplace,
        const UA_DataValue*     value) { return false; }

    /**
     * Hook used to customize allowHistoryUpdateDeleteRawModifiedHandler() call-back by derived classes.
     *Allow delete of historical data
     * By default not allowed.
     * @return true if allowed.
     */
    virtual bool allowHistoryUpdateDeleteRawModified(
        UA_AccessControl*   ac,
        const UA_NodeId*    sessionId,
        void*               sessionContext,
        const UA_NodeId*    nodeId,
        UA_DateTime         startTimestamp,
        UA_DateTime         endTimestamp,
        bool                isDeleteModified) { return false; }

    /*!
        \brief activateSession
        \return
    */
    virtual UA_StatusCode activateSession(UA_AccessControl */*ac*/,
                                          const UA_EndpointDescription */*endpointDescription*/,
                                          const UA_ByteString */*secureChannelRemoteCertificate*/,
                                          const UA_NodeId */*sessionId*/,
                                          const UA_ExtensionObject */*userIdentityToken*/,
                                          void **/*sessionContext*/) {
        return UA_STATUSCODE_BADSESSIONIDINVALID;
    }

    /* Deauthenticate a session and cleanup */
    virtual void closeSession(UA_AccessControl */*ac*/,
                              const UA_NodeId */*sessionId*/, void */*sessionContext*/) {

    }

    /* Access control for all nodes*/
    virtual uint32_t getUserRightsMask(UA_AccessControl */*ac*/,
                                       const UA_NodeId */*sessionId*/, void */*sessionContext*/,
                                       const UA_NodeId */*nodeId*/, void */*nodeContext*/) {
        return 0;
    }

    /* Additional access control for variable nodes */
    virtual uint8_t getUserAccessLevel(UA_AccessControl */*ac*/,
                                       const UA_NodeId */*sessionId*/, void */*sessionContext*/,
                                       const UA_NodeId */*nodeId*/, void */*nodeContext*/) {
        return 0;
    }

    /* Additional access control for method nodes */
    virtual bool getUserExecutable(UA_AccessControl */*ac*/,
                                   const UA_NodeId */*sessionId*/, void */*sessionContext*/,
                                   const UA_NodeId */*methodId*/, void */*methodContext*/) {
        return false;
    }

    /*  Additional access control for calling a method node in the context of a
        specific object */
    virtual bool getUserExecutableOnObject(UA_AccessControl *ac,
                                           const UA_NodeId *sessionId, void *sessionContext,
                                           const UA_NodeId *methodId, void *methodContext,
                                           const UA_NodeId *objectId, void *objectContext) {
        return false;
    }
    /* Allow insert,replace,update of historical data */
    virtual bool allowHistoryUpdateUpdateData(UA_AccessControl */*ac*/,
            const UA_NodeId */*sessionId*/, void */*sessionContext*/,
            const UA_NodeId */*nodeId*/,
            UA_PerformUpdateType /*performInsertReplace*/,
            const UA_DataValue */*value*/) {
        return false;
    }

    /* Allow delete of historical data */
    virtual bool allowHistoryUpdateDeleteRawModified(UA_AccessControl */*ac*/,
            const UA_NodeId */*sessionId*/,
            void */*sessionContext*/,
            const UA_NodeId */*nodeId*/,
            UA_DateTime /*startTimestamp*/,
            UA_DateTime/* endTimestamp*/,
            bool /*isDeleteModified*/) {
        return false;
    }

    /*!
     * \brief clearAccessControl
     */
    virtual void clearAccessControl(UA_AccessControl */*ac*/)
    {
        // reset to defaults
    }
    /*!
     * \brief allowBrowseNodeHandler
     * \return
     */
    virtual bool allowBrowseNode ( UA_AccessControl */*ac*/, const UA_NodeId */*sessionId*/, void */*sessionContext*/,  const UA_NodeId */*nodeId*/, void */*nodeContext*/)
    {
        return true;
    }

    /*!
     * \brief allowTransferSubscriptionHandler
     * \param ac
     * \return
     */
    virtual bool allowTransferSubscription(UA_AccessControl *ac, const UA_NodeId */*oldSessionId*/, void */*oldSessionContext*/,
                                           const UA_NodeId */*newSessionId*/, void */*newSessionContext*/)
    {
        return false;
    }



    /*!
        \brief setHistoryDatabase
    */
    void setHistoryDatabase(UA_HistoryDatabase &);
    // Publish - Subscribe interface



    // Creates an instance of a condition handler - must be derived from Condition

    template <typename T>
    /*!
     * \brief createCondition
     * \param conditionType
     * \param conditionName
     * \param conditionSource
     * \param outConditionId
     * \param hierarchialReferenceType
     * \return true on success
     */
    bool createCondition(
        const NodeId &conditionType,
        const std::string &conditionName,
        const NodeId & conditionSource, // parent
        Condition_p & outCondition, // newly created condition object has the condition node
        const NodeId &hierarchialReferenceType = NodeId::Null)
    {
        NodeId outConditionId;
        QualifiedName qn(conditionSource.nameSpaceIndex(),conditionName);
        outConditionId.notNull(); // this is the key to the condition dictionary
        outCondition = nullptr;
        _lastError =  UA_Server_createCondition(server(),
                                                NodeId::Null,
                                                conditionType,
                                                qn, conditionSource,
                                                hierarchialReferenceType,
                                                outConditionId.isNull()?nullptr:outConditionId.ref());
        if(lastOK())
        {
            // create the condition object
            ConditionPtr c(new T(*this,outConditionId,conditionSource));
            outCondition = c.get();
            unsigned key = UA_NodeId_hash(outConditionId.ref());
            _conditionMap[key] = std::move(c); // servers own the condition objects
            return true;
        }
        return false;
    }

    /*!
     * \brief deleteCondition
     * \param c
     */
    void deleteCondition(const NodeId &c)
    {
        _conditionMap.erase(UA_NodeId_hash(c.ref()));
    }

    /*!
     * \brief setConditionTwoStateVariableCallback
     * \param condition
     * \param removeBranch
     * \param callbackType
     * \return
     */
    bool setConditionTwoStateVariableCallback( const NodeId  &condition, UA_TwoStateVariableCallbackType callbackType, bool removeBranch = false)
    {
        ConditionPtr &c = findCondition(condition); // conditions are bound to servers - possible for the same node id to be used in different servers
        if(c)
        {
            return c->setCallback(callbackType,removeBranch);
        }
        return false;
    }

    /*!
     * \brief getNamespaceByName
     * \param namespaceUri
     * \param foundIndex
     * \return
     */
    bool getNamespaceByName(const std::string &namespaceUri, size_t &foundIndex)
    {
        String ua(namespaceUri);
        _lastError =  UA_Server_getNamespaceByName(server(), ua, &foundIndex);
        return lastOK();
    }

    /*!
     * \brief UA_Server_getStatistics
     * \return
     */
    UA_ServerStatistics getStatistics()    {
        return UA_Server_getStatistics(server());
    }

    //
    // Async Access
    //
    /* Set the async flag in a method node */

    /*!
     * \brief setMethodNodeAsync
     * \param id
     * \param isAsync
     * \return
     */
    bool setMethodNodeAsync(const NodeId  &id, bool isAsync)
    {

        _lastError =   UA_Server_setMethodNodeAsync(server(),  id, (UA_Boolean) isAsync);
        return lastOK();
    }


    /*!
     * \brief getAsyncOperationNonBlocking
     * \param type
     * \param request
     * \param context
     * \param timeout
     * \return
     */
    bool getAsyncOperationNonBlocking( UA_AsyncOperationType *type,
                                       const UA_AsyncOperationRequest **request,
                                       void **context, UA_DateTime *timeout)
    {
        return UA_Server_getAsyncOperationNonBlocking(server(),type,request, context, timeout) == UA_TRUE;
    }

    /*!
     * \brief setAsyncOperationResult
     * \param response
     * \param context
     */
    void setAsyncOperationResult( const UA_AsyncOperationResponse *response,
                                  void *context)
    {
        UA_Server_setAsyncOperationResult(server(), response, context );
    }

    // object property

    /*!
     * \brief addTimedCallback
     * \param data
     * \param date
     * \param callbackId
     * \return
     */
    bool addTimedEvent(unsigned msDelay, UA_UInt64 &callbackId,std::function<void (Timer &)> func)
    {
        if(_server)
        {
            UA_DateTime dt = UA_DateTime_nowMonotonic() + (UA_DATETIME_MSEC * msDelay);
            TimerPtr t(new Timer(this,0,true,func));
            _lastError = UA_Server_addTimedCallback(_server, Server::timerCallback, t.get(), dt, &callbackId);
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

    bool  addRepeatedTimerEvent(UA_Double interval_ms, UA_UInt64 &callbackId,std::function<void (Timer &)> func)
    {
        if(_server)
        {
            TimerPtr t(new Timer(this,0,false,func));
            _lastError = UA_Server_addRepeatedCallback(_server, Server::timerCallback,t.get(),interval_ms, &callbackId);
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
    bool changeRepeatedTimerInterval(UA_UInt64 callbackId, UA_Double interval_ms)
    {
        if(_server)
        {
            _lastError = UA_Server_changeRepeatedCallbackInterval(_server,callbackId,interval_ms);
            return lastOK();
        }
        return false;
    }
    /*!
     * \brief UA_Client_removeCallback
     * \param client
     * \param callbackId
     */
    void removeTimerEvent(UA_UInt64 callbackId)
    {
        _timerMap.erase(callbackId);
    }

    //
    // Publish Subscribe Support - To be added when it is finished
    //

};
    /**
     * setHistoryDatabase
     * Publish - Subscribe interface
     * @param h
     */
    void setHistoryDatabase(UA_HistoryDatabase& dbHistory) {
        if (m_pConfig) m_pConfig->historyDatabase = dbHistory;
    }
};

} // namespace open62541

#endif // OPEN62541SERVER_H