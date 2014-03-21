/**
 * @page c_audio Audio Service
 *
 * @ref le_audio.h "Click here for the API reference documentation."
 *
 *@ref le_audio_interfaces <br>
 *@ref le_audio_streams <br>
 *@ref le_audio_connectors <br>
 *@ref le_audio_formats <br>
 *@ref le_audio_samples <br>
 *
 * The Audio API handles audio interfaces including play or listen supported formats.
 *
 * A Legato device can use several audio interfaces. You choose the input and output
 * interfaces to tie together. The Audio stream related to a particular interface is
 * represented with an 'Audio Stream Reference'.
 *
 * You can create your own audio path by connecting several audio streams together using
 * audio connectors.
 *
 * An audio path can support more than two audio interfaces. You can have a basic output audio
 * path of a voice call to connect the Modem Voice Received interface with the Speaker
 * interface, and at the same time, the Modem Voice Received interface can be also connected to a
 * Recorder Device interface.
 *
 * @section le_audio_interfaces Open/Close an Audio Interface
 *
 * The following functions let you select the desired interface:
 * - @c le_audio_OpenMic(): returns an Audio Stream Reference of the analog audio signal coming from
 *                       the microphone input.
 * - @c le_audio_OpenSpeaker(): returns an Audio Stream Reference of the analog audio signal routed
 *                           to the Speaker output.
 * - @c le_audio_OpenUsbRx(): returns an Audio Stream Reference of the digitized audio signal coming
 *                         from an external device connected via USB Audio Class.
 * - @c le_audio_OpenUsbTx(): returns an Audio Stream Reference of the digitized audio signal routed
 *                         to an external device connected via USB Audio Class.
 * - @c le_audio_OpenModemVoiceRx(): returns an Audio Stream Reference of the digitized audio signal
 *                                coming from a voice call. The audio format is negotiated with the
 *                                network when the call is established.
 * - @c le_audio_OpenModemVoiceTx(): returns an Audio Stream Reference of the digitized audio signal
 *                                routed to a voice call. The audio format is negotiated with the
 *                                network when the call is established.
 *
 * Multiple users can own the same stream at the same time.
 *
 * @c le_audio_GetFormat() can be called to get the audio format of an input or output
 * stream.
 *
 * Call @c le_audio_Close() to release it. If
 * several users own the same, corresponding stream reference, the interface will
 * close only after the last user releases the audio stream.
 *
 * @section le_audio_streams Control an Audio Stream
 *
 * Once the users get an Audio Stream reference, they can control it with the following functions:
 *
 * - le_audio_SetGain(): allows the user to adjust the gain of an audio stream (0 means 'muted',
 *                       100 is the maximum gain value).
 * - le_audio_GetGain(): allows the User to retrieve the gain of an audio stream (0 means
 *                       'muted', 100 is the maximum gain value).
 * - le_audio_Mute(): allows the user to mute an audio stream.
 * - le_audio_Unmute(): allows the user to unmute an audio stream.
 *
 * @note The hardware may or may not support the full 0-100 resolution, and if you want to see what
 * was actually set call le_audio_GetGain() after le_audio_SetGain() to get the real value.
 *
 * @section le_audio_connectors Create Audio connectors
 *
 * You can create your own audio path by connecting several audio streams together.
 *
 * @c le_audio_CreateConnector() function creates a reference to an audio connector.
 *
 * You can tie an audio stream to a connector by calling the le_audio_Connect()
 * function.
 *
 * You can remove an audio stream from a connector by calling the le_audio_Disconnect()
 * function.
 *
 * When finished with it, delete it using the @c le_audio_DeleteConnector() function.
 *
 * @section le_audio_formats Specifying audio formats
 *
 * The encoding audio format can be specified to (or retrieved from) the audio interfaces that
 * support this option. The format parameter is a string containing the IANA specified encoding
 * format name for Real-Time protocol (RTP).
 *
 * The complete list of the audio encoding format names can be found on the IANA organization web
 * site (http://www.iana.org).
 *
 * Function that gets the current audio format for an open interface is:
 * - @c le_audio_GetFormat()
 *
 * @warning Ensure to check the list of supported audio formats for your specific
 * platform.
 *
 * Some examples of audio formats:
 * - "L16-8K": Linear PCM 16-bit audio @ 8KHz
 * - "L16-16K": Linear PCM 16-bit audio @ 16KHz
 * - "GSM": European GSM Full Rate @ 8KHz (GSM 06.10)
 * - "GSM-EFR": ITU-T GSM Enhanced Full Rate @ 8KHz (GSM 06.60)
 * - "GSM-HR-08": ITU-T GSM Half Rate @ 8KHz (GSM 06.20)
 * - "AMR": Adaptive Multi-Rate - Full Rate @ 8KHz
 * - "AMR-HR": Adaptive Multi-Rate - Half Rate @ 8KHz
 * - "AMR-WB": Adaptive Multi-Rate Wideband @ 16KHz
 *
 * @note The string is not case sensitive.
 *
 * @section le_audio_samples Code samples
 *
 * The following two code samples show how to create different audio paths for an
 * incoming voice call.
 *
 * The first example has the audio path depending on an USB Audio handset. The
 * @b ConnectVoiceCallAudio() function creates this audio path. A USB Audio handset
 * plugged in and fully operationnal, will redirect the call audio to the handset; otherwise, it will
 * redirect the call to the default microphone and speaker.
 *
 * Use @c DisconnectVoiceCallAudio() to delete the audio path.
 *
 * @code
 *
 * le_result_t ConnectVoiceCallAudio
 * (
 *     le_audio_ConnectorRef_t*  audioInputConnectorRefPtr,  // [OUT] Input connector.
 *     le_audio_ConnectorRef_t*  audioOutputConnectorRefPtr, // [OUT] Output connector.
 *     le_audio_StreamRef_t*     mdmRxAudioRefPtr,           // [OUT] Received voice call audio stream.
 *     le_audio_StreamRef_t*     mdmTxAudioRefPtr,           // [OUT] Transmitted voice call audio stream.
 *     le_audio_StreamRef_t*     deviceRxAudioRefPtr,        // [OUT] Received device audio stream.
 *     le_audio_StreamRef_t*     deviceTxAudioRefPtr         // [OUT] Transmitted device audio stream.
 * )
 * {
 *     // I get the audio from the voice call, I don't care which audio format.
 *     *mdmRxAudioRefPtr = le_audio_OpenModemVoiceRx();
 *     *mdmTxAudioRefPtr = le_audio_OpenModemVoiceTx();
 *
 *     // If I cannot get the audio from the voice call, I return an error.
 *     if ((*mdmRxAudioRefPtr == NULL) || (*mdmTxAudioRefPtr == NULL))
 *     {
 *             // I close the audio interfaces that have failed to open.
 *             le_audio_Close(*mdmRxAudioRefPtr);
 *             le_audio_Close(*mdmTxAudioRefPtr);
 *             return LE_NOT_POSSIBLE;
 *     }
 *
 *     // I create the audio connectors.
 *     *audioInputConnectorRefPtr = le_audio_CreateConnector();
 *     *audioOutputConnectorRefPtr = le_audio_CreateConnector();
 *
 *     // I verify if my Audio USB handset is plugged and operational before trying to use it.
 *     if(IsMyUSBHandsetPlugged() == TRUE)
 *     {
 *         // I can redirect the audio to my USB handset using linear PCM audio format (PCM 16bits @ 16KHz)
 *         *deviceRxAudioRefPtr = le_audio_OpenUsbRx();
 *         *deviceTxAudioRefPtr = le_audio_OpenUsbTx();
 *
 *         le_audio_Connect(*audioInputConnectorRefPtr, *deviceRxAudioRefPtr);
 *         le_audio_Connect(*audioOutputConnectorRefPtr, *deviceTxAudioRefPtr);
 *     }
 *     else
 *     {
 *         // There is no USB Audio handset, I redirect the audio to the in-built Microphone and Speaker.
 *         *deviceRxAudioRefPtr = le_audio_OpenMic();
 *         *deviceTxAudioRefPtr = le_audio_OpenSpeaker();
 *
 *         le_audio_Connect(*audioInputConnectorRefPtr, *deviceRxAudioRefPtr);
 *         le_audio_Connect(*audioOutputConnectorRefPtr, *deviceTxAudioRefPtr);
 *     }
 *
 *     // I tie the audio from the voice call to the connectors.
 *     le_audio_Connect (*audioInputConnectorRefPtr, *mdmTxAudioRefPtr);
 *     le_audio_Connect (*audioOutputConnectorRefPtr, *mdmRxAudioRefPtr);
 *
 *     return LE_OK;
 * }
 *
 *
 * void DisconnectVoiceCallAudio
 * (
 *     le_audio_ConnectorRef_t  audioInputConnectorRef,  // [IN] Input connector.
 *     le_audio_ConnectorRef_t  audioOutputConnectorRef, // [IN] Output connector.
 *     le_audio_StreamRef_t     mdmRxAudioRef,           // [IN] Received voice call audio stream.
 *     le_audio_StreamRef_t     mdmTxAudioRef,           // [IN] Transmitted voice call audio stream.
 *     le_audio_StreamRef_t     deviceRxAudioRef,        // [IN] Received device audio stream.
 *     le_audio_StreamRef_t     deviceTxAudioRef         // [IN] Transmitted device audio stream.
 * )
 * {
 *     // The call is terminated, I can close its audio interfaces.
 *     le_audio_Close(mdmRxAudioRef);
 *     le_audio_Close(mdmTxAudioRef);
 *
 *     // I close all the device interfaces.
 *     le_audio_Close(deviceRxAudioRef);
 *     le_audio_Close(deviceTxAudioRef);
 *
 *     // I delete the Audio connector references.
 *     le_audio_DeleteConnector(audioInputConnectorRef);
 *     le_audio_DeleteConnector(audioOutputConnectorRef);
 * }
 *
 * @endcode
 *
 * The next code sample uses specific functions to deal with a new 'Incoming call' event during a call
 * already in progress.
 *
 * If no call is in progress,  use the @c ConnectVoiceCallAudio() function to redirect the
 * call audio to the in-built Microphone and Speaker.
 *
 * If a new call is incoming, and considered a high priority call, you must mute the audio
 * of the first call, and then connect the new call to your current audio path.
 * Use @c SwitchVoiceCallAudio() for these actions.
 *
 * When the high priority call terminates, you can return back to your previous call and reactivate its
 * audio with the @c SwitchBackVoiceCallAudio() function.
 *
 * @code
 *
 * le_result_t ConnectVoiceCallAudio
 * (
 *     le_audio_ConnectorRef_t*  audioInputConnectorRefPtr,  // [OUT] Input connector.
 *     le_audio_ConnectorRef_t*  audioOutputConnectorRefPtr, // [OUT] Output connector.
 *     le_audio_StreamRef_t*     mdmRxAudioRefPtr,           // [OUT] Received voice call audio stream.
 *     le_audio_StreamRef_t*     mdmTxAudioRefPtr,           // [OUT] Transmitted voice call audio stream.
 *     le_audio_StreamRef_t*     micRefPtr,                  // [OUT] Microphone stream.
 *     le_audio_StreamRef_t*     speakerRefPtr               // [OUT] Speaker stream.
 * )
 * {
 *     *mdmRxAudioRefPtr = le_audio_OpenModemVoiceRx();
 *     *mdmTxAudioRefPtr = le_audio_OpenModemVoiceTx();
 *
 *     // If I cannot get the audio from the voice call, I return an error.
 *     if ((*mdmRxAudioRefPtr == NULL) || (*mdmTxAudioRefPtr == NULL))
 *     {
 *             // I close the audio interfaces that  have failed to open.
 *             le_audio_Close(*mdmRxAudioRefPtr);
 *             le_audio_Close(*mdmTxAudioRefPtr);
 *             return LE_NOT_POSSIBLE;
 *     }
 *
 *     *audioInputConnectorRefPtr = le_audio_CreateConnector();
 *     *audioOutputConnectorRefPtr = le_audio_CreateConnector();
 *
 *     // Redirect audio to the in-built Microphone and Speaker.
 *     *speakerRefPtr = le_audio_OpenSpeaker();
 *     *micRefPtr = le_audio_OpenMic();
 *
 *     le_audio_Connect(*audioInputConnectorRefPtr, *micRefPtr);
 *     le_audio_Connect (*audioInputConnectorRefPtr, *mdmTxAudioRefPtr);
 *     le_audio_Connect(*audioOutputConnectorRefPtr, *speakerRefPtr);
 *     le_audio_Connect (*audioOutputConnectorRefPtr, *mdmRxAudioRefPtr);
 *
 *     return LE_OK;
 * }
 *
 *
 * le_result_t SwitchVoiceCallAudio
 * (
 *     le_audio_ConnectorRef_t  audioInputConnectorRef,  // [IN] Input connector.
 *     le_audio_ConnectorRef_t  audioOutputConnectorRef, // [IN] Output connector.
 *     le_audio_StreamRef_t     oldMdmRxAudioRef,        // [IN] Received audio stream of the previous voice call.
 *     le_audio_StreamRef_t     oldMdmTxAudioRef,        // [IN] Transmitted audio stream of the previous voice call.
 *     le_audio_StreamRef_t*    newMdmRxAudioRefPtr,     // [OUT] Received audio stream of the new voice call.
 *     le_audio_StreamRef_t*    newMdmTxAudioRefPtr      // [OUT] Transmitted audio stream of the new voice call.
 * )
 * {
 *     if ((newMdmRxAudioRefPtr == NULL)     ||
 *         (newMdmTxAudioRefPtr == NULL))
 *     {
 *         return LE_BAD_PARAMETER;
 *     }
 *
 *     *newMdmRxAudioRefPtr = le_audio_OpenModemVoiceRx();
 *     *newMdmTxAudioRefPtr = le_audio_OpenModemVoiceTx();
 *
 *     // If I cannot get the audio from the voice call, I return an error.
 *     if ((*newMdmRxAudioRefPtr == NULL) || (*newMdmTxAudioRefPtr == NULL))
 *     {
 *         // I close the audio interfaces that have failed to open.
 *         le_audio_Close(*newMdmRxAudioRefPtr);
 *         le_audio_Close(*newMdmTxAudioRefPtr);
 *         return LE_NOT_POSSIBLE;
 *     }
 *
 *     // I mute the previous call.
 *     le_audio_Mute(oldMdmRxAudioRef);
 *     le_audio_Mute(oldMdmTxAudioRef);
 *
 *     // I connect the new incoming call.
 *     le_audio_Connect (audioInputConnectorRef, *newMdmTxAudioRefPtr);
 *     le_audio_Connect (audioOutputConnectorRef, *newMdmRxAudioRefPtr);
 *
 *     return LE_OK;
 * }
 *
 *
 * le_result_t SwitchBackVoiceCallAudio
 * (
 *     le_audio_StreamRef_t  oldMdmRxAudioRef, // [IN] Received audio stream of the previous voice call.
 *     le_audio_StreamRef_t  oldMdmTxAudioRef, // [IN] Transmitted audio stream of the previous voice call.
 *     le_audio_StreamRef_t  newMdmRxAudioRef, // [IN] Received audio stream of the new voice call.
 *     le_audio_StreamRef_t  newMdmTxAudioRef  // [IN] Transmitted audio stream  of the new voice call.
 * )
 * {
 *     // I can delete the new call audio interfaces.
 *     le_audio_Close(newMdmRxAudioRef);
 *     le_audio_Close(newMdmTxAudioRef);
 *
 *     // I can re-open the previous call streaming.
 *     if (le_audio_Unmute(oldMdmRxAudioRef) != LE_OK)
 *     {
 *         return LE_FAULT;
 *     }
 *     if (le_audio_Unmute(oldMdmTxAudioRef) != LE_OK)
 *     {
 *         return LE_FAULT;
 *     }
 *
 *     return LE_OK;
 * }
 *
 * @endcode
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */


/** @file le_audio.h
 *
 * Legato @ref c_audio include file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 */


#ifndef LEGATO_AUDIO_INCLUDE_GUARD
#define LEGATO_AUDIO_INCLUDE_GUARD

#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Reference type for Audio Stream
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_audio_Stream*   le_audio_StreamRef_t;

//--------------------------------------------------------------------------------------------------
/**
 * Reference type for Audio Connector
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_audio_Connector*   le_audio_ConnectorRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Open the Microphone.
 *
 * @return  Reference to the input audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenMic
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the Speakerphone.
 *
 * @return Reference to the output audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenSpeaker
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the received audio stream of an USB audio class.
 *
 * @return  Reference to the input audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenUsbRx
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the transmitted audio stream of an USB audio class.
 *
 * @return Reference to the output audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenUsbTx
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the received audio stream of the PCM interface.
 *
 * @return A Reference to the input audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenPcmRx
(
    uint32_t timeslot  ///< [IN] The time slot number.
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the transmitted audio stream of the PCM interface.
 *
 * @return A Reference to the output audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenPcmTx
(
    uint32_t timeslot  ///< [IN] The time slot number.
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the received audio stream of a voice call.
 *
 * @return Reference to the input audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenModemVoiceRx
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Open the transmitted audio stream of a voice call.
 *
 * @return Reference to the output audio stream, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_StreamRef_t le_audio_OpenModemVoiceTx
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the audio format of an input or output stream.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Audio stream reference is invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_GetFormat
(
    le_audio_StreamRef_t streamRef,  ///< [IN] The audio stream reference.
    char*                formatPtr,  ///< [OUT] The name of the audio encoding as used by the
                                     ///  Real-Time Protocol (RTP), specified by the IANA organisation.
    size_t               len         ///< [IN]  The length of format string.
);

//--------------------------------------------------------------------------------------------------
/**
 * Close an audio stream.
 * If several users own the stream reference, the interface closes only after
 * the last user closes the audio stream.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
void le_audio_Close
(
    le_audio_StreamRef_t streamRef  ///< [IN] Audio stream reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the Gain value of an input or output stream.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Audio stream reference is invalid.
 * @return LE_OUT_OF_RANGE  Gain parameter is not between 0 and 100
 * @return LE_OK            Function succeeded.
 *
 * @note The hardware may or may not support the full 0-100 resolution, and if you want to see what
 * was actually set call le_audio_GetGain() after le_audio_SetGain() to get the real value.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_SetGain
(
    le_audio_StreamRef_t streamRef,  ///< [IN] Audio stream reference.
    uint32_t             gain        ///< [IN] Gain value [0..100] (0 means 'muted', 100 is the
                                     ///       maximum gain value)
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the Gain value of an input or output stream.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Audio stream reference is invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note The hardware may or may not support the full 0-100 resolution, and if you want to see what
 * was actually set call le_audio_GetGain() after le_audio_SetGain() to get the real value.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_GetGain
(
    le_audio_StreamRef_t streamRef,  ///< [IN] Audio stream reference.
    uint32_t            *gainPtr     ///< [OUT] Gain value [0..100] (0 means 'muted', 100 is the
                                     ///        maximum gain value)
);

//--------------------------------------------------------------------------------------------------
/**
 * Mute an audio stream.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Audio stream reference is invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_Mute
(
    le_audio_StreamRef_t streamRef  ///< [IN] The audio stream reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Unmute an audio stream.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Audio stream reference is invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_Unmute
(
    le_audio_StreamRef_t streamRef  ///< [IN] Audio stream reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Create an audio connector reference.
 *
 * @return Reference to the audio connector, NULL if the function fails.
 */
//--------------------------------------------------------------------------------------------------
le_audio_ConnectorRef_t le_audio_CreateConnector
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Delete an audio connector reference.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
void le_audio_DeleteConnector
(
    le_audio_ConnectorRef_t connectorRef    ///< [IN] Connector reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Connect an audio stream to the connector reference.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BUSY           Insufficient DSP resources available.
 * @return LE_BAD_PARAMETER Connector and/or the audio stream references are invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_audio_Connect
(
    le_audio_ConnectorRef_t connectorRef,   ///< [IN] Connector reference.
    le_audio_StreamRef_t    streamRef       ///< [IN] Audio stream reference.
);

//--------------------------------------------------------------------------------------------------
/**
 * Disconnect an audio stream from the connector reference.
 *
 * @note If the caller is passing a bad reference into this function, it is a fatal error, the
 *       function will not return.
 */
//--------------------------------------------------------------------------------------------------
void le_audio_Disconnect
(
    le_audio_ConnectorRef_t connectorRef,   ///< [IN] Connector reference.
    le_audio_StreamRef_t    streamRef       ///< [IN] Audio stream reference.
);


#endif // LEGATO_AUDIO_INCLUDE_GUARD
