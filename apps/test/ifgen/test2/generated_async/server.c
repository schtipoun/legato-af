/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */


#include "local.h"
#include "server.h"


//--------------------------------------------------------------------------------------------------
// Generic Pack/Unpack Functions
//--------------------------------------------------------------------------------------------------

// todo: These functions could be moved to a separate library, to reduce overall code size and RAM
//       usage because they are common to each client and server.  However, they would then likely
//       need to be more generic, and provide better parameter checking and return results.  With
//       the way they are now, they can be customized to the specific needs of the generated code,
//       so for now, they will be kept with the generated code.  This may need revisiting later.

// Unused attribute is needed because this function may not always get used
__attribute__((unused)) static void* PackData(void* msgBufPtr, const void* dataPtr, size_t dataSize)
{
    // todo: should check for buffer overflow, but not sure what to do if it happens
    //       i.e. is it a fatal error, or just return a result

    memcpy( msgBufPtr, dataPtr, dataSize );
    return ( msgBufPtr + dataSize );
}

// Unused attribute is needed because this function may not always get used
__attribute__((unused)) static void* UnpackData(void* msgBufPtr, void* dataPtr, size_t dataSize)
{
    memcpy( dataPtr, msgBufPtr, dataSize );
    return ( msgBufPtr + dataSize );
}

// Unused attribute is needed because this function may not always get used
__attribute__((unused)) static void* PackString(void* msgBufPtr, const char* dataStr)
{
    // todo: should check for buffer overflow, but not sure what to do if it happens
    //       i.e. is it a fatal error, or just return a result

    // Add one for the null character
    size_t dataSize = strlen(dataStr) + 1;

    memcpy( msgBufPtr, dataStr, dataSize );
    return ( msgBufPtr + dataSize );
}

// Unused attribute is needed because this function may not always get used
__attribute__((unused)) static void* UnpackString(void* msgBufPtr, const char** dataStrPtr)
{
    // Add one for the null character
    size_t dataSize = strlen(msgBufPtr) + 1;

    // Strings do not have to be word aligned, so just return a pointer
    // into the message buffer
    *dataStrPtr = msgBufPtr;
    return ( msgBufPtr + dataSize );
}

// todo: This function may eventually replace all usage of UnpackString() above.
//       Maybe there should also be a PackDataString() function as well?
// Unused attribute is needed because this function may not always get used
__attribute__((unused)) static void* UnpackDataString(void* msgBufPtr, void* dataPtr, size_t dataSize)
{
    // Number of bytes copied from msg buffer, not including null terminator
    size_t numBytes;

    // todo: For now, assume the string will always fit in the buffer. This may not always be true.
    le_utf8_Copy( dataPtr, msgBufPtr, dataSize, &numBytes );
    return ( msgBufPtr + (numBytes + 1) );
}


//--------------------------------------------------------------------------------------------------
// Generic Server Types, Variables and Functions
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Type definition for generic function to remove a handler, given the handler ref.
 */
//--------------------------------------------------------------------------------------------------
typedef void(* RemoveHandlerFunc_t)(void *handlerRef);


//--------------------------------------------------------------------------------------------------
/**
 * Server Data Objects
 *
 * This object is used to store additional context info for each request
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_msg_SessionRef_t   clientSessionRef;     ///< The client to send the response to
    void*                 contextPtr;           ///< ContextPtr registered with handler
    le_event_HandlerRef_t handlerRef;           ///< HandlerRef for the registered handler
    RemoveHandlerFunc_t   removeHandlerFunc;    ///< Function to remove the registered handler
}
_ServerData_t;


//--------------------------------------------------------------------------------------------------
/**
 * The memory pool for server data objects
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t _ServerDataPool;


//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for use with Add/Remove handler references
 *
 * @warning Use _Mutex, defined below, to protect accesses to this data.
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t _HandlerRefMap;


//--------------------------------------------------------------------------------------------------
/**
 * Mutex and associated macros for use with the above HandlerRefMap.
 *
 * Unused attribute is needed because this variable may not always get used.
 */
//--------------------------------------------------------------------------------------------------
__attribute__((unused)) static pthread_mutex_t _Mutex = PTHREAD_MUTEX_INITIALIZER;   // POSIX "Fast" mutex.

/// Locks the mutex.
#define _LOCK    LE_ASSERT(pthread_mutex_lock(&_Mutex) == 0);

/// Unlocks the mutex.
#define _UNLOCK  LE_ASSERT(pthread_mutex_unlock(&_Mutex) == 0);


//--------------------------------------------------------------------------------------------------
/**
 * Forward declaration needed by StartServer
 */
//--------------------------------------------------------------------------------------------------
static void ServerMsgRecvHandler
(
    le_msg_MessageRef_t msgRef,
    void*               contextPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Server Service Reference
 */
//--------------------------------------------------------------------------------------------------
static le_msg_ServiceRef_t _ServerServiceRef;


//--------------------------------------------------------------------------------------------------
/**
 * Client Session Reference for the current message received from a client
 */
//--------------------------------------------------------------------------------------------------
static le_msg_SessionRef_t _ClientSessionRef;


//--------------------------------------------------------------------------------------------------
/**
 * Cleanup client data if the client is no longer connected
 */
//--------------------------------------------------------------------------------------------------
static void CleanupClientData
(
    le_msg_SessionRef_t sessionRef,
    void *contextPtr
)
{
    LE_DEBUG("Client %p is closed !!!", sessionRef);

    // Iterate over the server data reference map and remove anything that matches
    // the client session.
    _LOCK

    le_ref_IterRef_t iterRef = le_ref_GetIterator(_HandlerRefMap);
    le_result_t result = le_ref_NextNode(iterRef);
    _ServerData_t const* serverDataPtr;

    while ( result == LE_OK )
    {
        serverDataPtr =  le_ref_GetValue(iterRef);

        if ( sessionRef != serverDataPtr->clientSessionRef )
        {
            LE_DEBUG("Found session ref %p; does not match",
                     serverDataPtr->clientSessionRef);
        }
        else
        {
            LE_DEBUG("Found session ref %p; match found, so needs cleanup",
                     serverDataPtr->clientSessionRef);

            // Remove the handler
            serverDataPtr->removeHandlerFunc( serverDataPtr->handlerRef );

            // Release the server data block
            le_mem_Release((void*)serverDataPtr);

            // Delete the associated safeRef
            le_ref_DeleteRef( _HandlerRefMap, (void*)le_ref_GetSafeRef(iterRef) );

            // Since the reference map was modified, the iterator is no longer valid and
            // so has to be re-initalized.  This means that some values may get revisited,
            // but eventually this will iterate over the whole reference map.
            // todo: Is there an easier way?
            iterRef = le_ref_GetIterator(_HandlerRefMap);
        }

        // Get the next value in the reference mpa
        result = le_ref_NextNode(iterRef);
    }

    _UNLOCK
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t GetServiceRef
(
    void
)
{
    return _ServerServiceRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t GetClientSessionRef
(
    void
)
{
    return _ClientSessionRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize and start the server.
 */
//--------------------------------------------------------------------------------------------------
void StartServer
(
    const char* serviceInstanceName
        ///< [IN]
)
{
    LE_DEBUG("======= Starting Server %s ========", serviceInstanceName);

    le_msg_ProtocolRef_t protocolRef;

    // Create the server data pool
    _ServerDataPool = le_mem_CreatePool("ServerData", sizeof(_ServerData_t));

    // Create safe reference map for handler references.
    // The size of the map should be based on the number of handlers defined for the server.
    // Don't expect that to be more than 2-3, so use 3 as a reasonable guess.
    _HandlerRefMap = le_ref_CreateMap("ServerHandlers", 3);

    // Start the server side of the service
    protocolRef = le_msg_GetProtocolRef(PROTOCOL_ID_STR, sizeof(_Message_t));
    _ServerServiceRef = le_msg_CreateService(protocolRef, serviceInstanceName);
    le_msg_SetServiceRecvHandler(_ServerServiceRef, ServerMsgRecvHandler, NULL);
    le_msg_AdvertiseService(_ServerServiceRef);

    // Register for client sessions being closed
    le_msg_SetServiceCloseHandler(_ServerServiceRef, CleanupClientData, NULL);
}


//--------------------------------------------------------------------------------------------------
// Client Specific Server Code
//--------------------------------------------------------------------------------------------------


static void AsyncResponse_AddTestA
(
    int32_t x,
    void* contextPtr
)
{
    le_msg_MessageRef_t _msgRef;
    _Message_t* _msgPtr;
    _ServerData_t* serverDataPtr = (_ServerData_t*)contextPtr;

    // Will not be used if no data is sent back to client
    __attribute__((unused)) uint8_t* _msgBufPtr;

    // Create a new message object and get the message buffer
    _msgRef = le_msg_CreateMsg(serverDataPtr->clientSessionRef);
    _msgPtr = le_msg_GetPayloadPtr(_msgRef);
    _msgPtr->id = _MSGID_AddTestA;
    _msgBufPtr = _msgPtr->buffer;

    // Always pack the client context pointer first
    _msgBufPtr = PackData( _msgBufPtr, &(serverDataPtr->contextPtr), sizeof(void*) );

    // Pack the input parameters
    _msgBufPtr = PackData( _msgBufPtr, &x, sizeof(int32_t) );

    // Send the async response to the client
    LE_DEBUG("Sending message to client session %p", serverDataPtr->clientSessionRef);
    le_msg_Send(_msgRef);
}


static void Handle_AddTestA
(
    le_msg_MessageRef_t _msgRef

)
{
    // Get the message buffer pointer
    uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Needed if we are returning a result or output values
    uint8_t* _msgBufStartPtr = _msgBufPtr;

    // Unpack the input parameters from the message


    void* contextPtr;
    _msgBufPtr = UnpackData( _msgBufPtr, &contextPtr, sizeof(void*) );

    // Create a new server data object and fill it in
    _ServerData_t* serverDataPtr = le_mem_ForceAlloc(_ServerDataPool);
    serverDataPtr->clientSessionRef = le_msg_GetSession(_msgRef);
    serverDataPtr->contextPtr = contextPtr;
    contextPtr = serverDataPtr;

    // Define storage for output parameters


    // Call the function
    TestARef_t _result;
    _result = AddTestA ( AsyncResponse_AddTestA, contextPtr );

    // Put the handler reference result and a pointer to the associated remove function
    // into the server data object.  This function pointer is needed in case the client
    // is closed and the handlers need to be removed.
    serverDataPtr->handlerRef = (le_event_HandlerRef_t)_result;
    serverDataPtr->removeHandlerFunc = (RemoveHandlerFunc_t)RemoveTestA;

    // Return a safe reference to the server data object as the reference.
    _LOCK
    _result = le_ref_CreateRef(_HandlerRefMap, serverDataPtr);
    _UNLOCK


    // Re-use the message buffer for the response
    _msgBufPtr = _msgBufStartPtr;

    // Pack the result first
    _msgBufPtr = PackData( _msgBufPtr, &_result, sizeof(_result) );

    // Pack any "out" parameters


    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}


static void Handle_RemoveTestA
(
    le_msg_MessageRef_t _msgRef

)
{
    // Get the message buffer pointer
    uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Needed if we are returning a result or output values
    uint8_t* _msgBufStartPtr = _msgBufPtr;

    // Unpack the input parameters from the message
    TestARef_t addHandlerRef;
    _msgBufPtr = UnpackData( _msgBufPtr, &addHandlerRef, sizeof(TestARef_t) );
    // The passed in handlerRef is a safe reference for the server data object.  Need to get the
    // real handlerRef from the server data object and then delete both the safe reference and
    // the object since they are no longer needed.
    _LOCK
    _ServerData_t* serverDataPtr = le_ref_Lookup(_HandlerRefMap, addHandlerRef);
    le_ref_DeleteRef(_HandlerRefMap, addHandlerRef);
    _UNLOCK
    addHandlerRef = (TestARef_t)serverDataPtr->handlerRef;
    le_mem_Release(serverDataPtr);


    // Define storage for output parameters


    // Call the function
    RemoveTestA ( addHandlerRef );


    // Re-use the message buffer for the response
    _msgBufPtr = _msgBufStartPtr;


    // Pack any "out" parameters


    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Server-side respond function for allParameters
 */
//--------------------------------------------------------------------------------------------------
void allParametersRespond
(
    ServerCmdRef_t _cmdRef,
    uint32_t b,
    size_t outputNumElements,
    uint32_t* outputPtr,
    char* response,
    char* more
)
{
    LE_ASSERT(_cmdRef != NULL);

    // Get the message related data
    le_msg_MessageRef_t _msgRef = (le_msg_MessageRef_t)_cmdRef;
    _Message_t* _msgPtr = le_msg_GetPayloadPtr(_msgRef);
    __attribute__((unused)) uint8_t* _msgBufPtr = _msgPtr->buffer;

    // Ensure the passed in msgRef is for the correct message
    LE_ASSERT(_msgPtr->id == _MSGID_allParameters);

    // Ensure that this Respond function has not already been called
    LE_FATAL_IF( !le_msg_NeedsResponse(_msgRef), "Response has already been sent");


    // Pack any "out" parameters
    _msgBufPtr = PackData( _msgBufPtr, &b, sizeof(uint32_t) );
    _msgBufPtr = PackData( _msgBufPtr, &outputNumElements, sizeof(size_t) );
    _msgBufPtr = PackData( _msgBufPtr, outputPtr, outputNumElements*sizeof(uint32_t) );
    _msgBufPtr = PackString( _msgBufPtr, response );
    _msgBufPtr = PackString( _msgBufPtr, more );

    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}

static void Handle_allParameters
(
    le_msg_MessageRef_t _msgRef
)
{
    // Get the message buffer pointer
    __attribute__((unused)) uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Unpack the input parameters from the message
    common_EnumExample_t a;
    _msgBufPtr = UnpackData( _msgBufPtr, &a, sizeof(common_EnumExample_t) );

    size_t dataNumElements;
    _msgBufPtr = UnpackData( _msgBufPtr, &dataNumElements, sizeof(size_t) );

    uint32_t data[dataNumElements];
    _msgBufPtr = UnpackData( _msgBufPtr, data, dataNumElements*sizeof(uint32_t) );

    size_t outputNumElements;
    _msgBufPtr = UnpackData( _msgBufPtr, &outputNumElements, sizeof(size_t) );

    const char* label;
    _msgBufPtr = UnpackString( _msgBufPtr, &label );

    size_t responseNumElements;
    _msgBufPtr = UnpackData( _msgBufPtr, &responseNumElements, sizeof(size_t) );

    size_t moreNumElements;
    _msgBufPtr = UnpackData( _msgBufPtr, &moreNumElements, sizeof(size_t) );

    // Call the function
    allParameters ( (ServerCmdRef_t)_msgRef, a, data, dataNumElements, outputNumElements, label, responseNumElements, moreNumElements );
}


//--------------------------------------------------------------------------------------------------
/**
 * Server-side respond function for TriggerTestA
 */
//--------------------------------------------------------------------------------------------------
void TriggerTestARespond
(
    ServerCmdRef_t _cmdRef
)
{
    LE_ASSERT(_cmdRef != NULL);

    // Get the message related data
    le_msg_MessageRef_t _msgRef = (le_msg_MessageRef_t)_cmdRef;
    _Message_t* _msgPtr = le_msg_GetPayloadPtr(_msgRef);
    __attribute__((unused)) uint8_t* _msgBufPtr = _msgPtr->buffer;

    // Ensure the passed in msgRef is for the correct message
    LE_ASSERT(_msgPtr->id == _MSGID_TriggerTestA);

    // Ensure that this Respond function has not already been called
    LE_FATAL_IF( !le_msg_NeedsResponse(_msgRef), "Response has already been sent");


    // Pack any "out" parameters


    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}

static void Handle_TriggerTestA
(
    le_msg_MessageRef_t _msgRef
)
{
    // Get the message buffer pointer
    __attribute__((unused)) uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Unpack the input parameters from the message


    // Call the function
    TriggerTestA ( (ServerCmdRef_t)_msgRef );
}


static void AsyncResponse_AddBugTest
(
    void* contextPtr
)
{
    le_msg_MessageRef_t _msgRef;
    _Message_t* _msgPtr;
    _ServerData_t* serverDataPtr = (_ServerData_t*)contextPtr;

    // Will not be used if no data is sent back to client
    __attribute__((unused)) uint8_t* _msgBufPtr;

    // Create a new message object and get the message buffer
    _msgRef = le_msg_CreateMsg(serverDataPtr->clientSessionRef);
    _msgPtr = le_msg_GetPayloadPtr(_msgRef);
    _msgPtr->id = _MSGID_AddBugTest;
    _msgBufPtr = _msgPtr->buffer;

    // Always pack the client context pointer first
    _msgBufPtr = PackData( _msgBufPtr, &(serverDataPtr->contextPtr), sizeof(void*) );

    // Pack the input parameters


    // Send the async response to the client
    LE_DEBUG("Sending message to client session %p", serverDataPtr->clientSessionRef);
    le_msg_Send(_msgRef);
}


static void Handle_AddBugTest
(
    le_msg_MessageRef_t _msgRef

)
{
    // Get the message buffer pointer
    uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Needed if we are returning a result or output values
    uint8_t* _msgBufStartPtr = _msgBufPtr;

    // Unpack the input parameters from the message
    const char* newPathPtr;
    _msgBufPtr = UnpackString( _msgBufPtr, &newPathPtr );



    void* contextPtr;
    _msgBufPtr = UnpackData( _msgBufPtr, &contextPtr, sizeof(void*) );

    // Create a new server data object and fill it in
    _ServerData_t* serverDataPtr = le_mem_ForceAlloc(_ServerDataPool);
    serverDataPtr->clientSessionRef = le_msg_GetSession(_msgRef);
    serverDataPtr->contextPtr = contextPtr;
    contextPtr = serverDataPtr;

    // Define storage for output parameters


    // Call the function
    BugTestRef_t _result;
    _result = AddBugTest ( newPathPtr, AsyncResponse_AddBugTest, contextPtr );

    // Put the handler reference result and a pointer to the associated remove function
    // into the server data object.  This function pointer is needed in case the client
    // is closed and the handlers need to be removed.
    serverDataPtr->handlerRef = (le_event_HandlerRef_t)_result;
    serverDataPtr->removeHandlerFunc = (RemoveHandlerFunc_t)RemoveBugTest;

    // Return a safe reference to the server data object as the reference.
    _LOCK
    _result = le_ref_CreateRef(_HandlerRefMap, serverDataPtr);
    _UNLOCK


    // Re-use the message buffer for the response
    _msgBufPtr = _msgBufStartPtr;

    // Pack the result first
    _msgBufPtr = PackData( _msgBufPtr, &_result, sizeof(_result) );

    // Pack any "out" parameters


    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}


static void Handle_RemoveBugTest
(
    le_msg_MessageRef_t _msgRef

)
{
    // Get the message buffer pointer
    uint8_t* _msgBufPtr = ((_Message_t*)le_msg_GetPayloadPtr(_msgRef))->buffer;

    // Needed if we are returning a result or output values
    uint8_t* _msgBufStartPtr = _msgBufPtr;

    // Unpack the input parameters from the message
    BugTestRef_t addHandlerRef;
    _msgBufPtr = UnpackData( _msgBufPtr, &addHandlerRef, sizeof(BugTestRef_t) );
    // The passed in handlerRef is a safe reference for the server data object.  Need to get the
    // real handlerRef from the server data object and then delete both the safe reference and
    // the object since they are no longer needed.
    _LOCK
    _ServerData_t* serverDataPtr = le_ref_Lookup(_HandlerRefMap, addHandlerRef);
    le_ref_DeleteRef(_HandlerRefMap, addHandlerRef);
    _UNLOCK
    addHandlerRef = (BugTestRef_t)serverDataPtr->handlerRef;
    le_mem_Release(serverDataPtr);


    // Define storage for output parameters


    // Call the function
    RemoveBugTest ( addHandlerRef );


    // Re-use the message buffer for the response
    _msgBufPtr = _msgBufStartPtr;


    // Pack any "out" parameters


    // Return the response
    LE_DEBUG("Sending response to client session %p", le_msg_GetSession(_msgRef));
    le_msg_Respond(_msgRef);
}


static void ServerMsgRecvHandler
(
    le_msg_MessageRef_t msgRef,
    void*               contextPtr
)
{
    // Get the message payload so that we can get the message "id"
    _Message_t* msgPtr = le_msg_GetPayloadPtr(msgRef);

    // Get the client session ref for the current message.  This ref is used by the server to
    // get info about the client process, such as user id.  If there are multiple clients, then
    // the session ref may be different for each message, hence it has to be queried each time.
    _ClientSessionRef = le_msg_GetSession(msgRef);

    // Dispatch to appropriate message handler and get response
    switch (msgPtr->id)
    {
        case _MSGID_AddTestA : Handle_AddTestA(msgRef); break;
        case _MSGID_RemoveTestA : Handle_RemoveTestA(msgRef); break;
        case _MSGID_allParameters : Handle_allParameters(msgRef); break;
        case _MSGID_TriggerTestA : Handle_TriggerTestA(msgRef); break;
        case _MSGID_AddBugTest : Handle_AddBugTest(msgRef); break;
        case _MSGID_RemoveBugTest : Handle_RemoveBugTest(msgRef); break;

        default: LE_ERROR("Unknowm msg id = %i", msgPtr->id);
    }

    // Clear the client session ref associated with the current message, since the message
    // has now been processed.
    _ClientSessionRef = 0;
}

