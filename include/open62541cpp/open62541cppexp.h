/*
 * Copyright (C) 2017 -  Alexandru Dobinda
 *
 * This file is part of open62541 C++ classes. open62541 C++ classes are free software: you can
 * redistribute it and/or modify it under the terms of the Mozilla Public
 * License v2.0 as stated in the LICENSE file provided with open62541.
 *
 * open62541 C++ classes are distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.
 */

#ifndef _OPEN62541CPP_EXPORTS_H
#define _OPEN62541CPP_EXPORTS_H

#if defined(_WIN32)
#if defined(open62541cpp_EXPORTS)
#define UA_EXPORT __declspec(dllexport)
#else
#define UA_EXPORT __declspec(dllimport)
#endif /* open62541cpp_EXPORTS */
#else  /* defined (_WIN32) */
#define UA_EXPORT
#endif

#endif /* _OPEN62541CPP_EXPORTS */
