#ifndef OPEN62541_GLOBAL_H
#define OPEN62541_GLOBAL_H

#define UA_DYNAMIC_LINKING_EXPORT
#if defined(OPEN62541_LIBRARY)
#define OPEN62541SHARED_EXPORT
#else
#define OPEN62541SHARED_EXPORT
#endif

#endif /* OPEN62541_GLOBAL_H */
