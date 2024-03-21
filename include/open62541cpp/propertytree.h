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

#ifndef UA_PROPERTYTREE_H
#define UA_PROPERTYTREE_H

#include <boost/optional/optional.hpp>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#include <boost/bind/bind.hpp>
#include <string>
#include <vector>
#include <map>
#include <boost/tokenizer.hpp>
#include <boost/foreach.hpp>
#include <iterator>
#include <algorithm>
#include <ostream>
#include <functional>

namespace Open62541 {

// Mutexs
typedef boost::shared_mutex ReadWriteMutex;
typedef boost::shared_lock<boost::shared_mutex> ReadLock;
typedef boost::unique_lock<boost::shared_mutex> WriteLock;

// a tree is an addressable set of nodes
// objects of type T must have an assignment operator
//
typedef boost::tokenizer<boost::char_separator<char>> tokenizer;

/**
 * The NodePath template class is a vector of T where T is a string of char or byte
 * that can be split into sub-string using a char separator, by default "."
 */
template <typename T>
class NodePath : public std::vector<T> {
public:
    NodePath()                      {}
    explicit NodePath(const T& str)   { toList(str); }

    /**
     * toList splits the input string and store the sub-strings in the vector
     * @param s is the string to split
     * @param seperator specify the char used as a separator, by default "."
     */
    void toList(const T& str, const char* seperator = ".") {
        boost::char_separator<char> sep(seperator);
        tokenizer tokens(str, sep);
        for (const auto& i : tokens) {
            this->push_back(i);
        }
    }

    /**
     * toString
     * @param[out] s is the output string containing the vector items separated by "."
     */
    void toString(T& str) {
        if (this->size() > 0) {
            NodePath& n = *this;
            str = n[0];
            for (unsigned i = 1; i < this->size(); i++) {
                str += ".";
                str += n[i];
            }
        }
    }

    /**
     * Append a child path to this path
     * Each item of the other NodePath are added to the end of this NodePath
     * @param path the other NodePath that will be appended to this.
     * @return a ref to itself, permitting to chain append calls.
     */
    const NodePath<T>& append(const NodePath<T>& path) {
        for (const auto& path : path) {
            this->push_back(path);
        }
        return *this;
    }
}; // class NodePath

/**
 * The Node template class represent a node in a tree stored in a std::map.
 * A node can only have 1 parent, but can have 0+ children node.
 * @param K specify the type of the key used in the map
 * @param T specify the type of the value used in the map
 * Each node has a name and data which type must matches the one of the map's key-value.
 * Each node also have a pointer to its parent node in the tree,
 * as well as the list of its direct children stored in a map with the same key-value type.
 */
template <typename K, typename T>
class Node {
public:
    typedef std::map<K, Node*>  ChildMap;
    typedef NodePath<K>         Path;

private:
    K         m_name;               /**< the name of the node */
    T         m_data;               /**< the node's data */
    Node*     m_pParent = nullptr;  /**< the node's parent */
    ChildMap  m_children;           /**< a map containing the direct children of this node. Owned by the node */

public:
    class NodeIteratorFunc {
    public:
        virtual void Do(Node*) {}
    };

    /**
     * Default Node constructor
     */
    Node() {}

    /**
     * Specialized Node constructor
     * @param name specify the node's name
     * @param parent specify the node's parent as a raw pointer. The tree root node doesn't have one.
     */
    Node(const K& name, Node* parent = nullptr)
        : m_name(name), m_pParent(parent) {}

    /**
     * Node destructor.
     * Erase itself from its parent children map before calling clear()
     */
    virtual ~Node() {
        if (m_pParent) {
            m_pParent->m_children.erase(name()); // detach
            m_pParent = nullptr;
        }
        clear(); // destroy all the children
    }

    /**
     * Destroy the node's descendants (children, grand-children, ...).
     * Remove itself as a parent from its children and delete them recursively.
     */
    void clear() {
        for (const auto& i : m_children) {
            if (Node* pNode = i.second) {
                pNode->m_pParent = nullptr; // to avoid the children uselessly detaching themselves from their parent.
                delete pNode;
            }
        }
        m_children.clear();
    }

    /** @return a ref to the map with all the direct children node */
    ChildMap&       children()            { return m_children; }

    /** @return a const ref to the map with all the direct children node */
    const ChildMap& constChildren() const { return m_children; }

    /** @return the number of direct children node */
    size_t          totalChildren() const { return m_children.size(); }

    /** @return a ref to the node's data. */
    T&              data()                { return m_data; }

    /** @return a const ref to the node's data. */
    const T&        constData()     const { return m_data; }

    /**
     * setData assign a new data to the node
     * @param data specify the new data.
     */
    void setData(const T& data) { m_data = data; }

    /**
     * Get a specific child node. If it doesn't exist in the map, a new entry is created.
     * @param name of the desired child node.
     * @return a pointer to the found child node, if not found a pointer on a newly created one.
     */
    Node* child(const K& name)   { return m_children[name]; }

    /**
     * Test if a child node with a specific name exists.
     * @param name of the child to test
     * @return true if the child exist false otherwise.
     */
    bool hasChild(const K& name) { return m_children[name] != nullptr; }

    /**
     * Add a child node.
     * If a child has the same name as the newly added node,
     * it is removed and replaced by the new one.
     * @param pNode a pointer on the node to add.
     */
    void addChild(Node* pNode) {
        if (hasChild(pNode->name())) {
            delete m_children[pNode->name()];
            m_children.erase(pNode->name());
        }
        m_children[pNode->name()] = pNode;
    }

    /**
     * Create a child with a specific name.
     * @param name of the new child
     * @param pNode specify the parent. If null, the parent is this node.
     * @return a pointer to the created child.
     */
    Node* createChild(const K& name, Node* pNode = nullptr) {
        if (!pNode) pNode = this;
        Node* pNewNode = new Node(name, pNode);
        addChild(pNewNode);
        return pNewNode;
    }

    /**
     * Remove a child node with a specific name
     * @param name of the child node to remove.
     */
    void removeChild(const K& name) {
        if (hasChild(name)) {
            Node* pNode = child(name);  // take the child node
            m_children.erase(name);
            if (pNode) delete pNode;
        }
    }

    // accessors

    const K&    name()        const { return m_name; }
    void        setName(const K& s) { m_name = s; }
    Node*       parent()      const { return m_pParent; }

    /**
     * setParent
     * @param pNode
     */
    void setParent(Node* pNode) {
        if (m_pParent && m_pParent != pNode) {
            m_pParent->m_children.erase(name());
        }

        m_pParent = pNode;
        if (m_pParent) m_pParent->m_children[name()] = this;
    }

    /**
     * Search for a children along a specific path in the children tree.
     * @param path vector of node name ordered from oldest to youngest node in the tree (..., grand-parent, parent, me, child, grand-child, ...)
     * @param depth specify the starting index in the path. 0 by default (first node in the path)
     *        This permit to use a path that doesn't start with the name of a direct child node,
     *        but starts with the parent, or grand...grand-parent.
     * @return nullptr on failure
     */
    Node* find(const Path& path, int depth = 0) {
        // do we have the child at this level?
        if (Node* pChild = child(path[depth])) {
            if (++depth < (int)path.size()) {
                return pChild->find(path, depth);
            }
        }
        return nullptr;
    }

    /**
     * Search for a children along a specific path in the children tree, using string
     * @param path a string that can be split into a regular path. Must start with a direct children.
     * @return nullptr on failure.
     */
    Node* find(const K& path)  
    {
        Path p;
        p.toList(path);
        return find(p);
    }

    /**
     * Add a lineage of children node matching a provided path.
     * Node created only if it does not exist already
     * @param path specify the lineage to create
     * @return the last created node or the last node of the lineage if it already existed.
     */
    Node* add(const Path& path) {
        Node* pNode = find(path);  // only create if the lineage does not exist

        if (!pNode) { // At least one node was missing from the lineage
            // Find the missing part of the path
            pNode = this;
            int depth = 0;
            while (pNode->hasChild(path[depth])) {
                pNode = pNode->child(path[depth++]);
            }
            // Create the rest of the path
            for (unsigned i = depth; i < path.size(); i++) {
                pNode = pNode->createChild(path[i]);
            }
        }

        return pNode;
    }

    /**
     * Add a lineage of children node matching a provided path.
     * @param path a string splittable into a path
     * @return the last created node or the last node of the lineage if it already existed.
     */
    Node* add(const K& path)   { return add(Path(path)); }

    /**
     * remove a node matching a path starting from this node.
     * @param path specify the path to the node to delete. Starts at this node.
     */
    void remove(const Path& path) {
        if (Node* pNode = find(path)) {
            delete pNode;
        }
    }

    /**
     * remove a node matching a path
     * @param path a string splittable into a path
     */
    void remove(const K& path) { remove(Path(path)); }

    /**
     * iterateNodes - iterate this node and all children using a given lambda
     * @param func the function to apply, must have the signature: bool func(Node&).
     *        Its return value specify if the function should also be applied to the node's children.
     * @return true if all nodes where affected, false otherwise.
     */
    bool iterateNodes(std::function<bool (Node&)> func) {
        if (func(*this)) {
            for (auto i = children().begin(); i != children().end(); i++) {
                (i->second)->iterateNodes(func);
            }
            return true;
        }
        return false;
    }
    
    /**
     * iterateNodes - iterate this node and all children using a given functor
     * @param func a NodeIteratorFunc "functor" to apply.
     * @see NodeIteratorFunc
     */
    void iterateNodes(NodeIteratorFunc& func) {
        func.Do(this); // action the function for the node
        for (auto i = children().begin(); i != children().end(); i++) {
            i->second->iterateNodes(func);
        }
    }

    /**
     * Serialize the descendant tree to a given output stream
     * @param os the output stream
     */
    template <typename STREAM>
    void write(STREAM& os) {
        os << name();
        os << data();
        os << static_cast<int>(children().size()); // this node's data
        // now recurse
        if (children().size() > 0) {
            for (auto i = children().begin(); i != children().end(); i++) {
                i->second->write(os);
            }
        }
    }

    /**
     * Read an input stream and create a tree starting from this node.
     * @param is the input stream having the structure
     * <name><data><totalChild><<child1Name><child1Data><child1totalChild>...> ...
                           ... <<childnName><childnData><childntotalChild>...>
     */
    template <typename STREAM>
    void read(STREAM& is) {
        int totalChildren = 0;
        clear();
        is >> m_name;
        is >> m_data;
        is >> totalChildren;
        if (totalChildren > 0) {
            for (int i = 0; i < totalChildren; ++i) {
                Node* pNode = new Node();
                pNode->read(is); // recurse
                addChild(pNode); // add subtree to children
            }
        }
    }


    /**
     * Recursively copy this node descendant tree to a destination node.
     * The destination node's already existing tree is completely replaced.
     * @param dest the destination node to modify
     */
    void copyTo(Node* dest) {
        dest->clear(); // destroy all descendants of destination node
        dest->setName(name());
        dest->setData(data());
        if (children().size() > 0) {
            for (auto i = children().begin(); i != children().end(); i++) {
                Node* c = new Node();
                dest->addChild(c);      // add the child
                i->second->copyTo(c);   // now recurse
            }
        }
    }
}; // class Node


/**
 * The PropertyTree template class represent a thread-safe tree stored in a std::map.
 * The node's data must be default constructible.
 * @param K specify the type of the key used in the map
 * @param T specify the type of the value used in the map
 * Each node has a name and data which type must matches the one of the map's key-value.
 * Each node also have a pointer to its parent node in the tree,
 * as well as the list of its direct children stored in a map with the same key-value type.
 */
template <typename K, typename T>
class PropertyTree {
    mutable ReadWriteMutex  m_mutex;
    bool                    m_changed = false;  /**< true if the tree structure or a node's data was modified */

public:
    T                   _defaultData; /**< default data as returned by the default constructor */
    typedef Node<K, T>  PropertyNode;
    typedef NodePath<K> Path;

private:
    PropertyNode m_empty; /**< the empty node, currently never used */
    PropertyNode m_root;  /**< the root node */

public:
    PropertyTree()
    : m_empty("__EMPTY__")
    , m_root("__ROOT__") {
        m_root.clear();
    }

    virtual ~PropertyTree()         { m_root.clear(); }
    ReadWriteMutex& mutex()         { return m_mutex; }
    bool changed()            const { return m_changed; }
    void clearChanged()             { m_changed = false; }
    void setChanged(bool f = true)  { m_changed = f; }
    PropertyNode& root()            { return m_root; }
    PropertyNode* rootNode()        { return &this->m_root; }

    /**
     * Destroy the whole tree, thread-safely.
     */
    void clear() {
        WriteLock l(m_mutex);
        m_root.clear();
        setChanged();
    }
    
    /**
     * Get a reference to the  data of a node matching a given path, thread-safely.
     * @return a reference to default data if the path doesn't exist
     * @see _defaultData
     */
    template <typename P>
    T& get(const P& path) {
        ReadLock l(m_mutex);
        if (auto* pNode = m_root.find(path)) {
            return pNode->data();
        }
        return _defaultData;
    }

    /**
     * Get a pointer on a node matching a given path, thread-safely.
     * @return nullptr if the path doesn't exist
     */
    template <typename P>
    PropertyNode* node(const P& path) {
        ReadLock l(m_mutex);
        return  m_root.find(path);
    }

    /**
     * Set the data of a node matching a given path, thread-safely.
     * If the node did not exist, it is created.
     * @param path of the node to modify/create
     * @param data of the modified/created node.
     * @return a pointer on the modified/created node.
     */
    template <typename P>
    PropertyNode* set(const P& path, const T& data) {
        auto pNode = m_root.find(path);
        if (!pNode) {
            WriteLock l(m_mutex);
            pNode = m_root.add(path);
        }
        if (pNode) {
            WriteLock l(m_mutex);
            pNode->setData(data);
        }
        setChanged();
        return pNode;
    }

    /**
     * Test if a path of nodes starting from the root exists in the tree
     * @param path specify the path to test
     * @warning: not thread-safe.
     * @return true if the path exist in the tree
     */
    template <typename P>
    bool exists(const P& path) {
        return m_root.find(path) != nullptr;
    }
    
    /**
     * Remove a node identified by its path from the tree, thread-safely..
     * @param path specify the path, starting at the root, of the node to delete
     */
    template <typename P>
    void remove(const P& path) {
        WriteLock l(m_mutex);
        setChanged();
        m_root.remove(path);
    }

    /**
     * Return the absolute path of a given node, thread-safely.
     * @param node specify the node.
     * @return an empty path if the node doesn't exist, the node absolute path otherwise.
     */
    Path absolutePath(PropertyNode* node) const {
        if (!node) return Path();

        Path path;
        ReadLock l(m_mutex);
        do
        {
            path.push_back(node->name());
            node = node->parent();
        } while (node != nullptr);

        std::reverse(std::begin(path), std::end(path));
        return path; // NRVO
    }

    /**
     * Get the data of a specified child from a given node, thread-safely.
     * If the node doesn't exist or doesn't have the specified child, a default value is returned
     * @param node specify the node whom child will be tested
     * @param name specify the child name
     * @param default specify the default data in case of failure
     * @return the child data or the default data if the child doesn't exist.
     */
    T& getChild(PropertyNode* node, const K& name, T& default_) {
        ReadLock l(m_mutex);
        if (node && node->hasChild(name)) {
            return node->child(name)->data();
        }
        return default_;
    }

    /**
     * Set the data of a child from a given node, thread-safely.
     * If the child doesn't exist it is created.
     * @param node specify the node whose child will be modified/created.
     * @param name specify the name of the child
     * @param data specify the new value of the child
     */
    void setChild(PropertyNode* node, const K& name, const T& data) {
        if (!node)
        return;

        WriteLock l(m_mutex);
        if (node->hasChild(name)) {
            node->child(name)->setData(data);
        }
        else {
            node->createChild(name)->setData(data);
        }
        setChanged();
    }

    /**
     * Apply a function to each node of the tree, thread-safely.
     * The function decides if it should also be applied to the current node's children.
     * @param func the function to apply. Must have the bool func(PropertyNode&) signature,
     *        and return if the children must be affected or not.
     * @return true if all nodes where affected, false otherwise.
     * @warning if func modifies the node, don't forget to call setChanged().
     */
    bool iterateNodes(std::function<bool (PropertyNode&)> func) {
        WriteLock l(m_mutex);
        return m_root.iterateNodes(func);
    }

    /**
     * Serialize the tree to a given output stream, thread-safely.
     * @param os the output stream.
     */
    template <typename S>
    void write(S& os) {
        ReadLock l(m_mutex);
        m_root.write(os);
    }

    /**
    * Create the tree by de-serializing an input stream, thread-safely.
    * @param is the input stream having the structure
    * <name><data><totalChild><<child1Name><child1Data><child1totalChild>...> ...
    ... <<childnName><childnData><childntotalChild>...>
    */
    template <typename S> void read(S& is) {
        WriteLock l(m_mutex);
        m_root.read(is);
        setChanged();
    }

    /**
     * Copy this tree to another one, thread-safely.
     * @param dest the destination tree
     */
    void copyTo(PropertyTree& dest) {
        if (this == &dest) return;

        ReadLock l(m_mutex);
        WriteLock w(dest.mutex());
        m_root.copyTo(&dest.m_root);
        dest.setChanged();
    }

    /**
     * Build the list of the direct children's name for a given node, thread-safely.
     * @param path identifying the node
     * @param[out] list the output vector to which the children's name list will be appended.
     * @return the list new size.
     */
    template <typename P>
    int listChildren(const P& path, std::vector<K>& list) {
        if (auto pNode = node(path)) {
            ReadLock l(m_mutex);
            list.reserve(list.size() + pNode->children().size());
            for (const auto& child : pNode->children()) {
                list.push_back(child.first);
            }
        }
        return list.size();
    }
}; // class PropertyTree

} // namespace Open62541

#endif // PROPERTYTREE_H
