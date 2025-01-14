/*
    Copyright (C) 2017 -  B. J. Hill

    This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
    redistribute it and/or modify it under the terms of the Mozilla Public
    License v2.0 as stated in the LICENSE file provided with open62541.

    open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
    A PARTICULAR PURPOSE.
*/

#ifndef EVENTSELECTCLAUSEARRAY_H
#define EVENTSELECTCLAUSEARRAY_H

#include "open62541/types.h"
#include <open62541cpp/objects/UaBaseTypeTemplate.h>
#include <open62541cpp/objects/ArrayTemplate.h>
#include <open62541cpp/objects/NodeTreeTypeDefs.h>


namespace Open62541 {
    class EventSelectClauseArray : public SimpleAttributeOperandArray
    {
    public:
        EventSelectClauseArray(size_t size);
        void setBrowsePath(size_t idx0, const UAPath& fullPath);
        void setBrowsePath(size_t idx0, const std::string& fullPath);
    };
} // namespace Open62541


#endif /* EVENTSELECTCLAUSEARRAY_H */
