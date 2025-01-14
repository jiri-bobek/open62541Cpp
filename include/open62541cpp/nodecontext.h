/*
 * Copyright (C) 2017 -  B. J. Hill
 *
 * This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
 * redistribute it and/or modify it under the terms of the Mozilla Public
 * License v2.0 as stated in the LICENSE file provided with open62541.
 *
 * open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 */

#ifndef NODECONTEXT_H
#define NODECONTEXT_H
#include <open62541cpp/open62541objects.h>
#include <open62541cpp/objects/NodeId.h>

namespace Open62541 {
    /**
 * The NodeContext class
 * Node context objects operate on nodes - the node contexts may be shared between more than one node
 * As of version 0.3 the node context is passed to callbacks and so can be used
 * This aggregates the data source call-backs, value call-backs and life-cycle call-backs.

 */
class NodeContext {
    std::string                 m_name;               /**< Context name */
    static UA_DataSource        m_dataSource;         /**< Data Source call-backs for reading and writing data to the node.
                                                           @see readDataSource() and writeDataSource() static methods. */
    static UA_ValueCallback     m_valueCallback;      /**< Call-backs to get or set the node value.
                                                           @see readValueCallback() and writeValueCallback() static methods. */
    static UA_NodeTypeLifecycle m_nodeTypeLifeCycle;  /**< Call-backs for node life-cycle. */
public:
    typedef std::function<bool(Server&, NodeId&, const UA_NumericRange*, UA_DataValue&)> DataFunc;
    typedef std::function<void(Server&, NodeId&, const UA_NumericRange*, const UA_DataValue*)> ValueFunc;
    typedef std::function<bool(Server&, NodeId&, const UA_NumericRange*, const UA_DataValue&)> ConstDataFunc;
    typedef std::function<void(Server&, NodeId&, const UA_NumericRange*, const UA_DataValue&)> ConstValueFunc;

protected:
    UA_StatusCode               _lastError;
    // Functor read write interface
    DataFunc                    _readData;
    ConstDataFunc               _writeData;
    ValueFunc                   _readValue;
    ConstValueFunc              _writeValue;

public:
    NodeContext(const std::string& name = "") : m_name(name) {}
    virtual ~NodeContext()                                   {}



    const std::string& name() { return m_name; }

    // global life-cycle construct and destruct
    /*!
     * \brief NodeContext
     * \param s
     * \param read
     * \param write
     */
    NodeContext(DataFunc read, ConstDataFunc write, const std::string& s = "")
        : m_name(s)
        , _readData(read)
        , _writeData(write)
    {
        // Data read write node
    }

    /*!
     * \brief NodeContext
     * \param s
     * \param read
     * \param write
     */

    NodeContext(ValueFunc read, ConstValueFunc write, const std::string& s = "")
        : m_name(s)
        , _readValue(read)
        , _writeValue(write)
    {
        // Value read/write node
    }

    /**
     * Overridable hook to specialize the node constructor of an object type in a given server.
     * Called by the Server::constructor() call-back.
     * @param server where the new node will be constructed.
     * @param node can specify the data for the new node in input and/or the new node in output.
     */
    virtual bool construct(Server& server, NodeId& node) {
        return true; // doing nothing is OK
    }
    // accessors
    void setReadData(DataFunc f)           { _readData = f; }
    void setWriteData(ConstDataFunc f)     { _writeData = f; }
    void setReadValue(ValueFunc f)         { _readValue = f; }
    void setWriteValue(ConstValueFunc f)   { _writeValue = f; }

    /*!
        \brief lastError
        \return
    */
    UA_StatusCode lastError() const { return _lastError; }

    /*!
        \brief lastOK
        \return last error code
    */
    bool lastOK() const { return _lastError == UA_STATUSCODE_GOOD; }

    /*!
        \brief find
        \param s
        \return
    */

    /**
     * Overridable hook to specialize the node destructor of an object type in a given server.
     * Called by the Server::destructor() call-back.
     * @param server where the new node will be destroyed.
     * @param node specify the id of the node to destroy. Can also return the destroyed node.
     */
    virtual void destruct(Server& server, NodeId& node) {
    }

    // type life-cycle

    /**
    * Register the set of life-cycle call-backs in the server,
    * enabling the constructor and destructor for the node of this type.
    * @param server
    * @param node storing the type for which life cycle call-backs will be set.
    * @return true on success
    */
    bool setTypeLifeCycle(Server& server, NodeId& node);

    /**
     * Call-back used to create a node type on a server
     * Internally calls typeConstruct() if every argument are valid
     * @param server
     * @param sessionId
     * @param sessionContext
     * @param typeNodeId
     * @param typeNodeContext
     * @param nodeId
     * @param nodeContext
     * @return error code
     */
    static UA_StatusCode typeConstructor(UA_Server* server,
                                         const UA_NodeId* sessionId,
                                         void* sessionContext,
                                         const UA_NodeId* typeNodeId,
                                         void* typeNodeContext,
                                         const UA_NodeId* nodeId,
                                         void** nodeContext);

    /**
     * Hook called by typeConstructor that can be overridden in children classes
     * to specialize the node constructor.
     * @param server of the node
     * @param node specify the node to create
     * @param type specify the node storing the type of the node
     * @return true on success
     */
    virtual bool typeConstruct(Server& /*server*/, NodeId& /*n*/, NodeId& /*t*/) { return true; }

    /* Can be NULL. May replace the nodeContext. */
    /*!
     * \brief typeDestructor
     * \param server
     * \param sessionId
     * \param sessionContext
     * \param typeNodeId
     * \param typeNodeContext
     * \param nodeId
     * \param nodeContext
     */
    static void typeDestructor(UA_Server* server,
                               const UA_NodeId* sessionId,
                               void* sessionContext,
                               const UA_NodeId* typeNodeId,
                               void* typeNodeContext,
                               const UA_NodeId* nodeId,
                               void** nodeContext);



    /**
     * Hook called by typeDestructor that can be overridden in children classes
     * to specialize the node destructor.
     * @param server of the node
     * @param node specify the node to destroy
     * @param type specify the node storing the type of the node
     */
    virtual void typeDestruct(Server& server, NodeId& node, NodeId& type) {
    }

    // Set up the data and value callbacks

    /**
     * Hook called by the readDataSource call-back that can be overridden in children classes
     * to specialize how data are read from the node.
     * @param node
     * @param range
     * @param value
     * @return true on success
     */
    virtual bool readData(
        Server& server, 
        NodeId& node, 
        const UA_NumericRange* range, 
        UA_DataValue& value) {
        if (_readData) return _readData(server, node, range, value);
        return false;
    }

    /**
     * Hook called by the writeDataSource call-back that can be overridden in children classes
     * to specialize how data are written to the node.
     * @param server
     * @param node
     * @param range
     * @param value
     * @return true on success
     */
    virtual bool writeData(
        Server& server, 
        NodeId& node, 
        const UA_NumericRange* range, 
        const UA_DataValue& value) {
        if (_writeData) return _writeData(server, node, range, value);
        return false;
    }

    /**
     * Establish the node as a data source provider.
     * A data source doesn't need a variable node attached to it to store the data.
     * It is like a virtual variable and only have a public representation
     * with a set of getter and/or setter, but doesn't have an actual variable in the node,
     * only an access to an underlying process that can be interrogated by the getter or setter.
     * It's a bit like the difference between a public member actually storing a value
     * and a getter dynamically calculating a value based on other variable.
     * @param server
     * @param node
     * @return true on success
     */
    bool setAsDataSource(Server& server, NodeId& node);

    /*!
        \brief readDataSource
        \param server
        \param sessionId
        \param sessionContext
        \param nodeId
        \param nodeContext
        \param includeSourceTimeStamp
        \param range
        \param value
        \return error code
    */
    static UA_StatusCode readDataSource(UA_Server* server,
                                        const UA_NodeId* sessionId,
                                        void* sessionContext,
                                        const UA_NodeId* nodeId,
                                        void* nodeContext,
                                        UA_Boolean includeSourceTimeStamp,
                                        const UA_NumericRange* range,
                                        UA_DataValue* value);

    /*!
        \brief writeDataSource
        \param server
        \param sessionId
        \param sessionContext
        \param nodeId
        \param nodeContext
        \param range
        \param value
        \return error code
    */
    static UA_StatusCode writeDataSource(UA_Server* server,
                                         const UA_NodeId* sessionId,
                                         void* sessionContext,
                                         const UA_NodeId* nodeId,
                                         void* nodeContext,
                                         const UA_NumericRange* range,
                                         const UA_DataValue* value);

    /**
     * Register the value call-backs in the server for a given variable node
     * @param server
     * @param node
     * @return true on success
     */
    bool setValueCallback(Server& server, NodeId& node);

    /**
     * Hook called by the readValueCallback() call-back
     * that can be overridden in children classes
     * to specialize how value are read from the variable node.
     * @param node
     */
    virtual void readValue(
        Server& server,
        NodeId& node,
        const UA_NumericRange* range,
        const UA_DataValue* value) {
        if (_readValue) _readValue(server, node, range, value);
    }

    /**
     * Hook called by the readValueCallback() call-back
     * that can be overridden in children classes
     * to specialize how value are written to the variable node.
     * @param node
     */
    virtual void writeValue(
        Server& server,
        NodeId& node,
        const UA_NumericRange* range,
        const UA_DataValue& value) {
        if (_writeValue) _writeValue(server, node, range, value);
    }

    // Value Callbacks

    /**
     * Call-back used to read from a variable node in given server.
     * Internally calls readValue() if every argument are valid
     * @param server
     * @param sessionId
     * @param sessionContext
     * @param nodeid
     * @param nodeContext
     * @param range
     * @param value
     */
    static void readValueCallback(
        UA_Server* server,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeid,    void* nodeContext,
        const UA_NumericRange* range,
        const UA_DataValue* value);

    /**
     * Call-back used to write to a variable node in given server.
     * Internally calls writeValue() if every argument are valid
     * @param server
     * @param sessionId
     * @param sessionContext
     * @param nodeId
     * @param nodeContext
     * @param range
     * @param data
     */
    static void writeValueCallback(
        UA_Server* server,
        const UA_NodeId* sessionId, void* sessionContext,
        const UA_NodeId* nodeId,    void* nodeContext,
        const UA_NumericRange* range,
        const UA_DataValue* data);
};

/**
* Helper class used to provide self-registering context.
* All context derived from this class will be self-registering in this class static map,
* thus providing a mean for all children to find other registered context by name.
* Can be used to setup reusable call-backs sets.
*/
class RegisteredNodeContext : public NodeContext
{
    typedef std::map<std::string, NodeContext*>   NodeContextMap;    /**< map of contexts */
    static NodeContextMap                         m_map; /**< map of registered contexts - typically a static instance is used to self register */

public:
    /**
     * Self-register the context at construction in the class static map
     * @param name of the registered context
     */
    RegisteredNodeContext(const std::string& name)
        : NodeContext(name) 
    {
        m_map[name] = this; 
    }

    /**
     * Dtor unregister on delete
     */
    virtual ~RegisteredNodeContext() 
    { 
        m_map.erase(name()); 
    }

    /**
     * Find a registered context with a specified name.
     * @param name of the registered context to find.
     * @return a pointer on the found context.
     */
    static NodeContext* findRef(const std::string& name) { return m_map[name]; }
};

} // namespace Open62541

#endif // METHODCONTEXT_H
