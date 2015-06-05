//--------------------------------------------------------------------------------------------------
/**
 * @file le_media.c
 *
 * This file contains the source code of the high level Audio API for playback / capture
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "pa_audio.h"
#include "le_audio_local.h"
#include "le_media_local.h"
#include "pa_media.h"
#include <math.h>


//--------------------------------------------------------------------------------------------------
/**
 * Values used for DTMF sampling.
 */
//--------------------------------------------------------------------------------------------------
#define SAMPLE_SCALE    (32767)
#define DTMF_AMPLITUDE  (40)
#if !defined (PI)
#define PI 3.14159265358979323846264338327
#endif

//--------------------------------------------------------------------------------------------------
// Data structures.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * DTMF thread resource structure.
 *
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    int            pipefd[2];
    uint32_t       sampleRate; ///< Sample frequency in Hertz
    uint32_t       duration;   ///< The DTMF duration in milliseconds.
    uint32_t       pause;      ///< The pause duration between tones in milliseconds.
    const char*    dtmfPtr;    ///< The DTMFs to play.
}
DtmfThreadCtx_t;

//--------------------------------------------------------------------------------------------------
/**
 * Media AMR context
 *
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    pa_media_AmrDecoderContext_t*   paAmrCtxPtr;    // AMR context in the PA
    int32_t                         fd_in;          // File descriptor of the file to decode
    int32_t                         fd_out;         // pipe output
    int32_t                         fd_pcm;         // pipe input
    le_audio_MediaHandlerRef_t      mediaHandler;   // media handler reference
    le_audio_Stream_t*              streamPtr;      // stream conrtext
}
MediaAmrContext_t;

//--------------------------------------------------------------------------------------------------
//                                       Static declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * The memory pool for the DTMF thread context
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t DtmfThreadContextPool;

//--------------------------------------------------------------------------------------------------
/**
 * The memory pool for the media AMR context
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t AudioAmrContextPool;

//--------------------------------------------------------------------------------------------------
/**
 * Play DTMF thread reference.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t DtmfTreadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 *  Return the low frequency component of a DTMF character.
 *
 */
//--------------------------------------------------------------------------------------------------
static inline uint32_t Digit2LowFreq
(
    int digit
)
{
    switch (digit)
    {

        case '1':
        case '2':
        case '3':
        case 'a':
        case 'A':
            return 697;

        case '4':
        case '5':
        case '6':
        case 'b':
        case 'B':
            return 770;

        case '7':
        case '8':
        case '9':
        case 'c':
        case 'C':
            return 852;

        case '*':
        case '0':
        case '#':
        case 'd':
        case 'D':
            return 941;

        default:
            return 0;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Return the high frequency component of a DTMF character.
 *
 */
//--------------------------------------------------------------------------------------------------
static inline uint32_t Digit2HighFreq
(
    int digit
)
{
    switch (digit)
    {

        case '1':
        case '4':
        case '7':
        case '*':
            return 1209;

        case '2':
        case '5':
        case '8':
        case '0':
            return 1336;

        case '3':
        case '6':
        case '9':
        case '#':
            return 1477;

        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
        case 'd':
        case 'D':
            return 1633;

        default:
            return 0;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Add two 16-bit values.
 *
 */
//--------------------------------------------------------------------------------------------------
static inline int16_t SaturateAdd16
(
    int32_t a,
    int32_t b
)
{
    int32_t tot=a+b;

    if (tot > 32767)
    {
        return 32767;
    }
    else if (tot < -32768)
    {
        return -32768;
    }
    else
    {
        return (tot & 0xFFFF);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Play Tone function.
 *
 */
//--------------------------------------------------------------------------------------------------
static void PlayTone
(
    int32_t  fd,
    uint32_t sampleRate,
    uint32_t freq1,
    int32_t  amp1,
    uint32_t freq2,
    int32_t  amp2,
    uint32_t duration
)
{
    double   d1, d2;
    uint32_t i;
    // TODO: check why I need to X2 for correct duration
    uint32_t samplesCount = sampleRate*duration*2/1000;
    int16_t  data[samplesCount];

    d1 = 1.0f * freq1 / sampleRate;
    d2 = 1.0f * freq2 / sampleRate;

    for (i=0; i<samplesCount; i++)
    {
        int16_t s1, s2;

        s1 = (int16_t)(SAMPLE_SCALE * amp1 / 100.0f * sin(2 * PI * d1 * i));
        s2 = (int16_t)(SAMPLE_SCALE * amp2 / 100.0f * sin(2 * PI * d2 * i));
        data[i] = SaturateAdd16(s1, s2);
    }
    int32_t wroteLen = write(fd, data, samplesCount);
    if (wroteLen <= 0)
    {
        LE_ERROR("write error %d", wroteLen);
        return;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 *  Play DTMF thread.
 *
 */
//--------------------------------------------------------------------------------------------------
static void* PlayDtmfThread
(
    void* contextPtr
)
{
    DtmfThreadCtx_t *threadCtxPtr = (DtmfThreadCtx_t*) contextPtr;
    size_t           dtmfLen = strlen(threadCtxPtr->dtmfPtr);
    uint32_t         i;

    for (i=0 ; i< dtmfLen ; i++)
    {
        LE_INFO("Play digit.%c", threadCtxPtr->dtmfPtr[i]);

        PlayTone(threadCtxPtr->pipefd[1], threadCtxPtr->sampleRate,
                 Digit2LowFreq(threadCtxPtr->dtmfPtr[i]), DTMF_AMPLITUDE,
                 Digit2HighFreq(threadCtxPtr->dtmfPtr[i]), DTMF_AMPLITUDE,
                 threadCtxPtr->duration);

        if (threadCtxPtr->pause)
        {
            // Play silence
            PlayTone(threadCtxPtr->pipefd[1], threadCtxPtr->sampleRate,
                     0, 0,
                     0, 0,
                     threadCtxPtr->pause);
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Play DTMF thread destructor.
 *
 */
//--------------------------------------------------------------------------------------------------
static void DestroyPlayDtmfThread
(
    void *contextPtr
)
{
    DtmfThreadCtx_t *threadCtxPtr = (DtmfThreadCtx_t*) contextPtr;

    if(threadCtxPtr)
    {
        LE_DEBUG("Close pipe Tx");
        close(threadCtxPtr->pipefd[1]);
        le_mem_Release(threadCtxPtr);
    }

    DtmfTreadRef = NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Media handler used to catch player event.
 *
 */
//--------------------------------------------------------------------------------------------------
static void MyMediaAmrHandler
(
    le_audio_StreamRef_t          streamRef,
    le_audio_MediaEvent_t         mediaEvent,
    void*                         contextPtr
)
{
    MediaAmrContext_t * amrCtxtPtr = (MediaAmrContext_t *) contextPtr;

    LE_DEBUG("MyMediaHandler %d", mediaEvent);

    if (amrCtxtPtr)
    {

        close(amrCtxtPtr->fd_pcm);
        le_audio_RemoveMediaHandler(amrCtxtPtr->mediaHandler);

        // set the initial fd in the stream context
        amrCtxtPtr->streamPtr->fd = amrCtxtPtr->fd_in;

        le_mem_Release(amrCtxtPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * AMR decoding thread.
 *
 */
//--------------------------------------------------------------------------------------------------
static void* PlayAmrThread
(
    void* contextPtr
)
{
    MediaAmrContext_t * amrCtxtPtr = (MediaAmrContext_t *) contextPtr;

    LE_DEBUG("PlayAmrThread");

    while (1)
    {
        uint8_t outBuffer[amrCtxtPtr->paAmrCtxPtr->bufferSize];

        /* Decode the packet */
        if ( pa_media_DecodeAmrFrames( amrCtxtPtr->paAmrCtxPtr,
                                        amrCtxtPtr->fd_in,
                                        outBuffer ) == LE_OK )
        {
            write(amrCtxtPtr->fd_out, outBuffer, amrCtxtPtr->paAmrCtxPtr->bufferSize);
        }
        else
        {
            break;
        }
    }

    close(amrCtxtPtr->fd_out);
    pa_media_ReleaseAmrDecoder(amrCtxtPtr->paAmrCtxPtr);

    LE_DEBUG("PlayAmrThread end");

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function check the file header to detect a WAV file, and get the PCM configuration.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t TreatWavFile
(
    int32_t                     fd,
    pa_audio_SamplePcmConfig_t* samplePcmConfigPtr
)
{
    WavHeader_t      hdr;

    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
    {
        LE_WARN("WAV detection: cannot read header");
        return LE_FAULT;
    }

    if ((hdr.riffId != ID_RIFF)  ||
        (hdr.riffFmt != ID_WAVE) ||
        (hdr.fmtId != ID_FMT)    ||
        (hdr.audioFormat != FORMAT_PCM) ||
        (hdr.fmtSize != 16))
    {
        LE_WARN("WAV detection: unrecognized wav format");
        lseek(fd, -sizeof(hdr), SEEK_CUR);
        return LE_FAULT;
    }

    samplePcmConfigPtr->sampleRate = hdr.sampleRate;
    samplePcmConfigPtr->channelsCount = hdr.channelsCount;
    samplePcmConfigPtr->bitsPerSample = hdr.bitsPerSample;
    samplePcmConfigPtr->fileSize = hdr.dataSize;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function check the file header to detect an AMR file, and get the PCM configuration.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_result_t TreatAmrFile
(
    le_audio_Stream_t*         streamPtr,
    pa_audio_SamplePcmConfig_t* samplePcmConfigPtr
)
{
    char header[10] = {0};

    // Try to detect the 5th first characters
    if ( read(streamPtr->fd, header, 9) != 9 )
    {
        LE_WARN("AMR detection: cannot read header");
        return LE_FAULT;
    }

    if ( strncmp(header, "#!AMR", 5) == 0 )
    {
        MediaAmrContext_t * amrCtxtPtr = le_mem_ForceAlloc(AudioAmrContextPool);
        pa_media_AmrFormat_t format = PA_MEDIA_AMR_MAX;
        int pipefd[2];

        memset(amrCtxtPtr, 0, sizeof(MediaAmrContext_t));

        if ( strncmp(header+5, "-WB\n", 4) == 0 )
        {
            LE_DEBUG("AMR-WB found");
            format = PA_MEDIA_AMR_WB;
        }
        else if ( strncmp(header+5, "-NB\n", 4) == 0 )
        {
            LE_DEBUG("AMR-NB found");
            format = PA_MEDIA_AMR_NB;
        }
        else if ( strncmp(header+5, "\n", 1) == 0 )
        {
            LE_DEBUG("AMR-NB found");
            format = PA_MEDIA_AMR_NB;
            lseek(streamPtr->fd, -3, SEEK_CUR);
        }
        else
        {
            LE_ERROR("Not an AMR file");
            le_mem_Release(amrCtxtPtr);
            return LE_FAULT;
        }

        if ( pa_media_InitAmrDecoder(format, &amrCtxtPtr->paAmrCtxPtr) != LE_OK )
        {
            LE_ERROR("Failed to init AMR Decoder");
            le_mem_Release(amrCtxtPtr);
            return LE_FAULT;
        }

        if (format == PA_MEDIA_AMR_WB)
        {
            samplePcmConfigPtr->sampleRate = 16000;
        }
        else
        {
            samplePcmConfigPtr->sampleRate = 8000;
        }

        samplePcmConfigPtr->channelsCount = 1;
        samplePcmConfigPtr->bitsPerSample = 16;
        samplePcmConfigPtr->fileSize = (-1);

        if (pipe(pipefd) == -1)
        {
            LE_ERROR("Failed to create the pipe");
            le_mem_Release(amrCtxtPtr);
            return LE_FAULT;
        }

        amrCtxtPtr->fd_in = streamPtr->fd;
        amrCtxtPtr->fd_out = pipefd[1];
        streamPtr->fd = pipefd[0];
        amrCtxtPtr->fd_pcm = pipefd[0];
        amrCtxtPtr->streamPtr = streamPtr;

        amrCtxtPtr->mediaHandler = le_audio_AddMediaHandler(streamPtr->streamRef,
                                       MyMediaAmrHandler, amrCtxtPtr);


        le_thread_Start(le_thread_Create("PlaySamples", PlayAmrThread, amrCtxtPtr));
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
//                                       Public declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to play a DTMF on a specific audio stream.
 *
 * @return LE_FORMAT_ERROR  The DTMF characters are invalid.
 * @return LE_BUSY          A DTMF playback is already in progress on the playback stream.
 * @return LE_FAULT         The function failed to play the DTMFs.
 * @return LE_OK            The funtion succeeded.
 *
 * @note The process exits, if an invalid audio stream reference is given.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_media_PlayDtmf
(
    le_audio_Stream_t*   streamPtr, ///< [IN] Stream object
    const char*          dtmfPtr,   ///< [IN] The DTMFs to play.
    uint32_t             duration,  ///< [IN] The DTMF duration in milliseconds.
    uint32_t             pause      ///< [IN] The pause duration between tones in milliseconds.
)
{
    le_result_t       res;
    DtmfThreadCtx_t*  threadCtxPtr = le_mem_ForceAlloc(DtmfThreadContextPool);

    memset(threadCtxPtr, 0, sizeof(DtmfThreadCtx_t));

    streamPtr->samplePcmConfig.sampleRate = 16000;
    streamPtr->samplePcmConfig.bitsPerSample = 16;
    streamPtr->samplePcmConfig.channelsCount = 1;
    streamPtr->samplePcmConfig.fileSize = -1;
    streamPtr->samplePcmConfig.pcmFormat = PCM_RAW;

    threadCtxPtr->duration = duration;
    threadCtxPtr->pause = pause;
    threadCtxPtr->sampleRate = 16000;
    threadCtxPtr->dtmfPtr = dtmfPtr;
    if (pipe(threadCtxPtr->pipefd) == -1)
    {
        LE_ERROR("Failed to create the pipe");
        le_mem_Release(threadCtxPtr);
        return LE_FAULT;
    }

    streamPtr->fd = threadCtxPtr->pipefd[0];

    if ((res=pa_audio_PlaySamples(streamPtr->audioInterface,
                                  streamPtr->fd,
                                  &streamPtr->samplePcmConfig)) == LE_OK)
    {
        LE_INFO("Spawn DTMF thread");
        if (DtmfTreadRef == NULL)
        {
            DtmfTreadRef = le_thread_Create("PlayDtmfs", PlayDtmfThread, threadCtxPtr);

            le_thread_AddChildDestructor(DtmfTreadRef,
                                        DestroyPlayDtmfThread,
                                        threadCtxPtr);

            le_thread_Start(DtmfTreadRef);
        }
    }
    else
    {
        le_mem_Release(threadCtxPtr);
        LE_ERROR("Cannot spawn DTMF thread!");
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start media service: check the header, start decoder if needed.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_media_Open
(
    le_audio_Stream_t*          streamPtr,          ///< [IN] Stream object
    pa_audio_SamplePcmConfig_t* samplePcmConfigPtr  ///< [IN] Sample configuration
)
{
    // First, check if the file is a wav
    if (TreatWavFile(streamPtr->fd, samplePcmConfigPtr) == LE_OK)
    {
        // wav file found
        return LE_OK;
    }
    else if (TreatAmrFile(streamPtr, samplePcmConfigPtr) == LE_OK)
    {
        return LE_OK;
    }
    else
    {
        return LE_FAULT;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the media service.
 *
 */
//--------------------------------------------------------------------------------------------------
void le_media_Init
(
    void
)
{
    // Allocate the AMR context pool.
    AudioAmrContextPool = le_mem_CreatePool("AudioAmrContextPool", sizeof(MediaAmrContext_t));

    // Allocate the DTMF thread context pool.
    DtmfThreadContextPool = le_mem_CreatePool("DtmfThreadContextPool", sizeof(DtmfThreadCtx_t));
}