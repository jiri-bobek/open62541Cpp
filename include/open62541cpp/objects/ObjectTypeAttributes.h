/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/

#ifndef OBJECTTYPEATTRIBUTES_H
#define OBJECTTYPEATTRIBUTES_H

#include <string>
#include "open62541/types.h"
#include "open62541/types_generated.h"
#include <open62541cpp/objects/UaBaseTypeTemplate.h>

namespace Open62541 {
/**
 * The attributes for an object type node. ID: 66
 * @class ObjectTypeAttributes open62541objects.h
 * RAII C++ wrapper class for the UA_ObjectTypeAttributes struct.
 * Setters are implemented for all member.
 * No getter, use ->member_name to access them.
 * @see UA_ObjectTypeAttributes in open62541.h
 */
class ObjectTypeAttributes : public TypeBase<UA_ObjectTypeAttributes, UA_TYPES_OBJECTTYPEATTRIBUTES>
{
    public:
         using TypeBase<UA_ObjectTypeAttributes, UA_TYPES_OBJECTTYPEATTRIBUTES>::operator=;
        auto& setDefault()
        {
            *this = UA_ObjectTypeAttributes_default;
            return *this;
        }

        auto& setDisplayName(const std::string& name)
        {
            ref()->displayName = UA_LOCALIZEDTEXT_ALLOC("en_US", name.c_str());
            return *this;
        }
        auto& setDescription(const std::string& descr)
        {
            ref()->description = UA_LOCALIZEDTEXT_ALLOC("en_US", descr.c_str());
            return *this;
        }
        auto& setSpecifiedAttributes(UA_UInt32 attribute)
        {
            ref()->specifiedAttributes = attribute;
            return *this;
        }
        auto& setWriteMask(UA_UInt32 mask)
        {
            ref()->writeMask = mask;
            return *this;
        }
        auto& setUserWriteMask(UA_UInt32 mask)
        {
            ref()->userWriteMask = mask;
            return *this;
        }
        auto& setIsAbstract(bool abstract)
        {
            ref()->isAbstract = abstract;
            return *this;
        }
    };
} // namespace Open62541


#endif /* OBJECTTYPEATTRIBUTES_H */
