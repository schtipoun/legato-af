/** @file messagingMessage.h
 *
 * @ref c_messaging implementation's "Message" module's inter-module interface definitions.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#ifndef LEGATO_MESSAGING_MESSAGE_H_INCLUDE_GUARD
#define LEGATO_MESSAGING_MESSAGE_H_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Represents a message.
 *
 * This same object type is used to represent messages on both the client side and the server side.
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_msg_Message
{
    le_dls_Link_t               link;       ///< Used to link onto message queues.
    le_msg_SessionRef_t         sessionRef; ///< The session to which this message belongs.

    union
    {
        /// Fields needed on the client side only
        struct
        {
            le_msg_ResponseCallback_t   completionCallback; ///< Function to call when txn finishes.
                                                            ///  NULL if no response expected.
            void*                       contextPtr; ///< Opaque ptr to pass to completion callback.
        }
        client;

        /// Fields needed on the server side only.
        struct
        {
            int responseFd;    ///< fd to send back with the response message. (-1 = no fd)
        }
        server;
    }
    clientServer;

    int                         fd;         ///< File descriptor to send or received (-1 = no fd)
    void*                       txnId;      ///< Safe reference value used as a transaction ID.
    void*                       payload[0]; ///< Variable-length payload buffer appears at the end.
}
Message_t;


//--------------------------------------------------------------------------------------------------
/**
 * Initializes this module.  This must be called only once at start-up, before any other functions
 * in this module are called.
 */
//--------------------------------------------------------------------------------------------------
void msgMessage_Init
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Create a Message Pool.
 *
 * @return  A reference to the pool.
 */
//--------------------------------------------------------------------------------------------------
le_mem_PoolRef_t msgMessage_CreatePool
(
    const char* name,       ///< [in] Name of the pool.
    size_t largestMsgSize   ///< [in] Size of the largest message payload, in bytes.
);


//--------------------------------------------------------------------------------------------------
/**
 * Send a single message over a connected socket.
 *
 * @return
 * - LE_OK if successful.
 * - LE_NO_MEMORY if the socket doesn't have enough send buffer space available right now.
 * - LE_COMM_ERROR if the socket reported an error on the send operation.
 */
//--------------------------------------------------------------------------------------------------
le_result_t msgMessage_Send
(
    int         socketFd,   ///< [IN] Connected socket's file descriptor.
    Message_t*  msgPtr      ///< The Message to be sent.
);


//--------------------------------------------------------------------------------------------------
/**
 * Receive a single message from a connected socket.
 *
 * @return
 * - LE_OK if successful.
 * - LE_WOULD_BLOCK if there's nothing there to receive and the socket is set non-blocking.
 * - LE_CLOSED if the connection has closed.
 * - LE_COMM_ERROR if an error was encountered.
 */
//--------------------------------------------------------------------------------------------------
le_result_t msgMessage_Receive
(
    int                 socketFd,   ///< [IN] The socket's file descriptor.
    le_msg_MessageRef_t msgRef      ///< [IN] Message object to store the received message in.
);


//--------------------------------------------------------------------------------------------------
/**
 * Gets a pointer to the queue link inside a Message object.
 *
 * @return The pointer.
 */
//--------------------------------------------------------------------------------------------------
static inline le_dls_Link_t* msgMessage_GetQueueLinkPtr
(
    le_msg_MessageRef_t msgRef
)
//--------------------------------------------------------------------------------------------------
{
    return &(msgRef->link);
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets a reference to the Message object in which a given queue link exists.
 *
 * @return The reference.
 */
//--------------------------------------------------------------------------------------------------
static inline le_msg_MessageRef_t msgMessage_GetMessageContainingLink
(
    le_dls_Link_t* linkPtr
)
//--------------------------------------------------------------------------------------------------
{
    return CONTAINER_OF(linkPtr, Message_t, link);
}


//--------------------------------------------------------------------------------------------------
/**
 * Sets a Message object's transaction ID.
 */
//--------------------------------------------------------------------------------------------------
static inline void msgMessage_SetTxnId
(
    le_msg_MessageRef_t msgRef,
    void*               txnId
)
//--------------------------------------------------------------------------------------------------
{
    msgRef->txnId = txnId;
}


//--------------------------------------------------------------------------------------------------
/**
 * Gets a Message object's transaction ID.
 *
 * @return The ID.  (Zero = the message is not part of a request-response transaction.)
 */
//--------------------------------------------------------------------------------------------------
static inline void* msgMessage_GetTxnId
(
    le_msg_MessageRef_t msgRef
)
//--------------------------------------------------------------------------------------------------
{
    return msgRef->txnId;
}


//--------------------------------------------------------------------------------------------------
/**
 * Call the completion callback function for a given message.
 */
//--------------------------------------------------------------------------------------------------
void msgMessage_CallCompletionCallback
(
    le_msg_MessageRef_t requestMsgRef,
    le_msg_MessageRef_t responseMsgRef
);


#endif // LEGATO_MESSAGING_MESSAGE_H_INCLUDE_GUARD
