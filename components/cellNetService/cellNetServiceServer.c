// -------------------------------------------------------------------------------------------------
/**
 *  Cellular Network Services Server
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
// -------------------------------------------------------------------------------------------------

#include "legato.h"

#include "interfaces.h"
#include "mdmCfgEntries.h"

#include "le_print.h"



//--------------------------------------------------------------------------------------------------
/**
 * ConfigDb max attempt number.
 *
 */
//--------------------------------------------------------------------------------------------------
#define CONFIGDB_ATTEMPT_MAX   5

//--------------------------------------------------------------------------------------------------
/**
 * Definitions for sending request/release commands to CellNet thread
 */
//--------------------------------------------------------------------------------------------------
#define REQUEST_COMMAND 1
#define RELEASE_COMMAND 2

static le_event_Id_t CommandEvent;

//--------------------------------------------------------------------------------------------------
/**
 * Count the number of requests
 */
//--------------------------------------------------------------------------------------------------
static uint32_t RequestCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Is the Radio powered on
 */
//--------------------------------------------------------------------------------------------------
static bool IsOn = false;

//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for the request reference
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t RequestRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Event for sending Cellular Network Registration state to applications
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t CellNetStateEvent;


//--------------------------------------------------------------------------------------------------
/**
 * Load the SIM configuration from the config DB
 */
//--------------------------------------------------------------------------------------------------
static void LoadSimFromConfigDb
(
    le_sim_Id_t simId
)
{
    uint32_t attemptCounter = CONFIGDB_ATTEMPT_MAX;
    // Get the configuration path for the SIM.
    char configPath[LIMIT_MAX_PATH_BYTES];
    snprintf(configPath, sizeof(configPath), "%s/%d",
             CFG_MODEMSERVICE_SIM_PATH,
             simId);

    LE_DEBUG("Start reading SIM-%d information in ConfigDB",simId);

    le_result_t result;
    le_sim_States_t simState;

    do
    {
        simState = le_sim_GetState(simId);

        switch (simState)
        {
            case LE_SIM_INSERTED:
            {
                // Check that the app has a configuration value.
                le_cfg_IteratorRef_t simCfg = le_cfg_CreateReadTxn(configPath);

                char simPin[LIMIT_MAX_PATH_BYTES] = {0};

                result = le_cfg_GetString(simCfg,CFG_NODE_PIN,simPin,sizeof(simPin),"");
                if ( result != LE_OK )
                {
                    LE_WARN("PIN string too large for SIM-%d",simId);
                    le_cfg_CancelTxn(simCfg);
                    return;
                }
                if ( strncmp(simPin,"",sizeof(simPin))==0 )
                {
                    LE_WARN("PIN not set for SIM-%d",simId);
                    le_cfg_CancelTxn(simCfg);
                    return;
                }
                if ( (result = le_sim_EnterPIN(simId,simPin)) != LE_OK )
                {
                    LE_ERROR("Error.%d Failed to enter SIM pin for SIM-%d",result,simId);
                    le_cfg_CancelTxn(simCfg);
                    return;
                }
                LE_DEBUG("Sim-%d is unlocked", simId);

                le_cfg_CancelTxn(simCfg);
                attemptCounter = 1;
                break;
            }
            case LE_SIM_BLOCKED:
            {
                LE_EMERG("Be carefull the sim-%d is BLOCKED, need to enter PUK code",simId);
                attemptCounter = 1;
                break;
            }
            case LE_SIM_BUSY:
                if (attemptCounter==1)
                {
                    LE_WARN("Could not load the configuration because "
                            "the SIM is still busy after %d attempts", CONFIGDB_ATTEMPT_MAX);
                }
                else
                {
                    LE_WARN("Sim-%d was busy when loading configuration,"
                            "retry in 1 seconds",simId);
                }
                sleep(1); // Retry in 1 second.
                break;
            case LE_SIM_READY:
                LE_DEBUG("Sim-%d is ready",simId);
                attemptCounter = 1;
                break;
            case LE_SIM_ABSENT:
                LE_WARN("Sim-%d is absent",simId);
                attemptCounter = 1;
                break;
            case LE_SIM_STATE_UNKNOWN:
                break;
        }
    } while (--attemptCounter);

    LE_DEBUG("Load SIM information is done");
}

//--------------------------------------------------------------------------------------------------
/**
 * Translate MRC states to CellNet states
 */
//--------------------------------------------------------------------------------------------------
static le_cellnet_State_t TranslateToCellNetState
(
    le_mrc_NetRegState_t  state
)
{
    switch(state)
    {
        case LE_MRC_REG_NONE:
        {
            le_onoff_t  radioState;
            le_result_t result;

            // In this state, the radio should be OFF.
            if ((result = le_mrc_GetRadioPower(&radioState)) != LE_OK)
            {
                LE_WARN("Failed to get the radio power. Result: %d", result);
                return LE_CELLNET_REG_UNKNOWN;
            }
            else
            {
                if (radioState == LE_OFF)
                {
                    // The radio is OFF
                    return LE_CELLNET_RADIO_OFF;
                }
                else
                {
                    // The radio is ON
                    return LE_CELLNET_REG_EMERGENCY;
                }
            }
        }
        case LE_MRC_REG_SEARCHING:
        case LE_MRC_REG_DENIED:
            return LE_CELLNET_REG_EMERGENCY;
        case LE_MRC_REG_HOME:
            return LE_CELLNET_REG_HOME;
        case LE_MRC_REG_ROAMING:
            return LE_CELLNET_REG_ROAMING;
        default:
            return LE_CELLNET_REG_UNKNOWN;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Send connection state event
 */
//--------------------------------------------------------------------------------------------------
static void SendCellNetStateEvent
(
    void
)
{
    le_mrc_NetRegState_t  state;
    le_mrc_GetNetRegState(&state);
    le_cellnet_State_t cellNetState = TranslateToCellNetState(state);
    LE_PRINT_VALUE("%i", cellNetState);

    // Send the event to interested applications
    le_event_Report(CellNetStateEvent, &cellNetState, sizeof(cellNetState));
}

//--------------------------------------------------------------------------------------------------
/**
 * Start Cellular Network Service Timer Handler.
 * When the timer expires, verify if the radio is ON, if NOT retry to power it up and rearm the
 * timer.
 */
//--------------------------------------------------------------------------------------------------
static void StartCellNetTimerHandler
(
    le_timer_Ref_t timerRef
)
{
    le_onoff_t  radioState;
    le_result_t result;

    if(RequestCount == 0)
    {
        // Release has been requested in the meantime, I must cancel the Request command process
        le_timer_Delete(timerRef);
    }
    else
    {
        result=le_mrc_GetRadioPower(&radioState);
        if ((result == LE_OK) && (radioState == LE_ON))
        {
            IsOn = true;
            // The radio is ON, stop and delete the Timer.
            le_timer_Delete(timerRef);

            // Load SIM configuration from Config DB
            le_sim_Id_t simSelected = le_sim_GetSelectedCard();

            if (le_sim_IsPresent(simSelected))
            {
                LoadSimFromConfigDb(simSelected);
                SendCellNetStateEvent();
            }

        }
        else
        {
            le_mrc_SetRadioPower(LE_ON);
            // TODO: find a solution to get off of this infinite loop
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Start Cellular Network Service.
 * Loads the SIM configuration into the modem for the first SIM slot with a SIM present.
 * Turns on the radio first if it is off or if the radio power setting cannot be detected.
 */
//--------------------------------------------------------------------------------------------------
static void StartCellularNetwork
(
    void
)
{
    le_onoff_t  radioState;
    le_result_t result;

    result=le_mrc_GetRadioPower(&radioState);
    if ((result == LE_OK) && (radioState == LE_ON))
    {
        IsOn = true;
        // Load SIM configuration from Config DB
        le_sim_Id_t simSelected = le_sim_GetSelectedCard();

        if (le_sim_IsPresent(simSelected))
        {
            LoadSimFromConfigDb(simSelected);
            SendCellNetStateEvent();
        }

    }
    else
    {
        // Try to power ON the radio anyway
        le_mrc_SetRadioPower(LE_ON);

        // Set a timer that gets the current position.
        le_timer_Ref_t startCellNetTimer = le_timer_Create("StartCellNetTimer");
        le_clk_Time_t interval = {15, 0}; // 15 seconds

        if ( (le_timer_SetHandler(startCellNetTimer, StartCellNetTimerHandler) != LE_OK) ||
             (le_timer_SetRepeat(startCellNetTimer, 0) != LE_OK) ||
             (le_timer_SetInterval(startCellNetTimer, interval) != LE_OK) ||
             (le_timer_Start(startCellNetTimer) != LE_OK) )
        {
            LE_ERROR("Could not start the StartCellNet timer!");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop Cellular Network Service Timer Handler.
 * When the timer expires, verify if the radio is OFF, if NOT retry to shutdown it and rearm the
 * timer.
 */
//--------------------------------------------------------------------------------------------------
static void StopCellNetTimerHandler
(
    le_timer_Ref_t timerRef
)
{
    le_onoff_t  radioState;
    le_result_t result;

    if(RequestCount != 0)
    {
        // Request has been requested in the meantime, I must cancel the Release command process
        le_timer_Delete(timerRef);
    }
    else
    {
        result=le_mrc_GetRadioPower(&radioState);
        if ((result == LE_OK) && (radioState == LE_OFF))
        {
            IsOn = false;
            // The radio is OFF, stop and delete the Timer.
            le_timer_Delete(timerRef);
        }
        else
        {
            le_mrc_SetRadioPower(LE_OFF);
            // TODO: find a solution to get off of this infinite loop
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Power Off the radio
 */
//--------------------------------------------------------------------------------------------------
static void StopCellularNetwork
(
    void
)
{
    le_onoff_t  radioState;
    le_result_t result;

    result=le_mrc_GetRadioPower(&radioState);
    if ((result == LE_OK) && (radioState == LE_OFF))
    {
        IsOn = false;
        return;
    }
    else
    {
        // Try to shutdown the radio anyway
        le_mrc_SetRadioPower(LE_OFF);

        // Set a timer that gets the current position.
        le_timer_Ref_t stopCellNetTimer = le_timer_Create("StopCellNetTimer");
        le_clk_Time_t interval = {5, 0}; // 5 seconds

        if ( (le_timer_SetHandler(stopCellNetTimer, StopCellNetTimerHandler) != LE_OK) ||
             (le_timer_SetRepeat(stopCellNetTimer, 0) != LE_OK) ||
             (le_timer_SetInterval(stopCellNetTimer, interval) != LE_OK) ||
             (le_timer_Start(stopCellNetTimer) != LE_OK) )
        {
            LE_ERROR("Could not start the StopCellNet timer!");
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler to process a command
 */
//--------------------------------------------------------------------------------------------------
static void ProcessCommand
(
    void* reportPtr
)
{
    uint32_t command = *(uint32_t*)reportPtr;

    LE_PRINT_VALUE("%i", command);

    if (command == REQUEST_COMMAND)
    {
        RequestCount++;
        if (!IsOn)
        {
            StartCellularNetwork();
        }
        else
        {
            SendCellNetStateEvent();
        }
    }
    else if (command == RELEASE_COMMAND)
    {
        // Don't decrement below zero, as it will wrap-around.
        if ( RequestCount > 0 )
        {
            RequestCount--;
        }

        if ( (RequestCount == 0) && IsOn )
        {
            StopCellularNetwork();
        }
    }
    else
    {
        LE_ERROR("Command %i is not valid", command);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for SIM States Notifications.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SimStateHandler
(
    le_sim_Id_t     simId,
    le_sim_States_t simState,
    void*           contextPtr
)
{
    if (simState == LE_SIM_INSERTED)
    {
        LoadSimFromConfigDb(simId);
        SendCellNetStateEvent();
    }
}

// -------------------------------------------------------------------------------------------------
/**
 *  Event callback for Cellular Network Registration state changes.
 */
// -------------------------------------------------------------------------------------------------
static void MrcNetRegHandler
(
    le_mrc_NetRegState_t state,
    void*  contextPtr
)
{
    le_cellnet_State_t cellNetState = TranslateToCellNetState(state);

    LE_PRINT_VALUE("Cellular Network Registration state.%d", cellNetState);

    // Send the state event to applications
    le_event_Report(CellNetStateEvent, &cellNetState, sizeof(cellNetState));
}

//--------------------------------------------------------------------------------------------------
/**
 * This thread does the actual work of starting/stopping a cellular network
 */
//--------------------------------------------------------------------------------------------------
static void* CellNetThread
(
    void* contextPtr
)
{
    // Connect to the services required by this thread
    le_cfg_ConnectService();
    le_mrc_ConnectService();
    le_sim_ConnectService();

    LE_INFO("CellNet Thread Started");

    // Register for command events
    le_event_AddHandler("ProcessCommand",
                        CommandEvent,
                        ProcessCommand);


    // Register for SIM state changes
    le_sim_AddNewStateHandler(SimStateHandler, NULL);

    // Register for MRC Network Registration state changes
    le_mrc_AddNetRegStateEventHandler(MrcNetRegHandler, NULL);


    // Run the event loop
    le_event_RunLoop();
    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Connection State Handler
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerCellNetStateHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    le_cellnet_State_t* eventDataPtr = reportPtr;
    le_cellnet_StateHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    clientHandlerFunc(*eventDataPtr,
                      le_event_GetContextPtr());
}


//--------------------------------------------------------------------------------------------------
/**
 * This function adds a handler ...
 */
//--------------------------------------------------------------------------------------------------
le_cellnet_StateEventHandlerRef_t le_cellnet_AddStateEventHandler
(
    le_cellnet_StateHandlerFunc_t handlerPtr,
    void* contextPtr
)
{
    LE_PRINT_VALUE("%p", handlerPtr);
    LE_PRINT_VALUE("%p", contextPtr);

    le_event_HandlerRef_t handlerRef = le_event_AddLayeredHandler(
                                                    "CellNetState",
                                                    CellNetStateEvent,
                                                    FirstLayerCellNetStateHandler,
                                                    handlerPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_cellnet_StateEventHandlerRef_t)(handlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * This function removes a handler ...
 */
//--------------------------------------------------------------------------------------------------
void le_cellnet_RemoveStateEventHandler
(
    le_cellnet_StateEventHandlerRef_t addHandlerRef
)
{
    LE_PRINT_VALUE("%p", addHandlerRef);

    le_event_RemoveHandler((le_event_HandlerRef_t)addHandlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * Request a cellular network
 *
 * @return Reference to the cellular network
 */
//--------------------------------------------------------------------------------------------------
le_cellnet_RequestObjRef_t le_cellnet_Request
(
    void
)
{
    uint32_t command = REQUEST_COMMAND;
    le_event_Report(CommandEvent, &command, sizeof(command));

    // Need to return a unique reference that will be used by Release.
    return le_ref_CreateRef(RequestRefMap, (void*)1);
}


//--------------------------------------------------------------------------------------------------
/**
 * Release a previously requested cellular network
 */
//--------------------------------------------------------------------------------------------------
void le_cellnet_Release
(
    le_cellnet_RequestObjRef_t requestRef ///< Reference to a previously requested cellular network
)
{
    // Look up the reference.  If it is NULL, then the reference is not valid.
    // Otherwise, delete the reference and send the release command to the CellNet thread.
    void* cellNetPtr = le_ref_Lookup(RequestRefMap, requestRef);
    if ( cellNetPtr == NULL )
    {
        LE_ERROR("Invalid cellular network request reference %p", requestRef);
    }
    else
    {
        LE_PRINT_VALUE("%p", requestRef);
        le_ref_DeleteRef(RequestRefMap, requestRef);

        uint32_t command = RELEASE_COMMAND;
        le_event_Report(CommandEvent, &command, sizeof(command));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the PIN code in the config tree.
 *
 * @return
 *    LE_OUT_OF_RANGE    Invalid simId
 *  - LE_FORMAT_ERROR    PIN code is not in string format.
 *  - LE_UNDERFLOW       The PIN code is not long enough (min 4 digits).
 *  - LE_OK              The function succeeded.
 *  - LE_FAULT           The function failed on any other errors
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cellnet_SetSimPinCode
(
    le_sim_Id_t simId,
        ///< [IN]
        ///< SIM identifier.

    const char* pinCodePtr
        ///< [IN]
        ///< PIN code to insert in the config tree.
)
{

    le_result_t result=LE_OK;
    size_t pinCodeLength = strlen(pinCodePtr);

    LE_DEBUG("simId= %d, pinCode= %s",simId,pinCodePtr);

    if (simId >= LE_SIM_ID_MAX)
    {
        LE_ERROR("Invalid simId (%d) provided!", simId);
        result = LE_OUT_OF_RANGE;
    }
    else
    {
        //void entry is taken into account
        if (strncmp(pinCodePtr,"",LE_SIM_PIN_MAX_LEN)!=0)
        {
            if (pinCodeLength > LE_SIM_PIN_MAX_LEN)
            {
                LE_KILL_CLIENT("PIN code exceeds %d", LE_SIM_PIN_MAX_LEN);
                return LE_FAULT;
            }
            else if (pinCodeLength < LE_SIM_PIN_MIN_LEN)
            {
                LE_ERROR("SIM PIN code is not long enough (min 4 digits)");
                result = LE_UNDERFLOW;
            }
            else
            {
                // test SIM pincode format
                int i;
                bool test_ok = true;
                for (i=0; ((i<pinCodeLength) && test_ok); i++)
                {
                    if ((pinCodePtr[i] < 0x30) || (pinCodePtr[i] > 0x39))
                    {
                        test_ok = false;
                        break;
                    }
                }
                if (false == test_ok)
                {
                    LE_ERROR("SIM PIN code format error");
                    result = LE_FORMAT_ERROR;
                }
            }
        }
    }
    if (LE_OK == result)
    {
        // Set the configuration path for the SIM.
        char configPath[LIMIT_MAX_PATH_BYTES];
        snprintf(configPath, sizeof(configPath), "%s/%d",
                 CFG_MODEMSERVICE_SIM_PATH,
                 simId);

        le_cfg_IteratorRef_t simCfgRef = le_cfg_CreateWriteTxn(configPath);
        le_cfg_SetString(simCfgRef, CFG_NODE_PIN, pinCodePtr);
        le_cfg_CommitTxn(simCfgRef);

        LE_DEBUG("SIM PIN code (%s) inserted OK in config Tree",pinCodePtr);

        // New SIM pincode is taken into account
        LoadSimFromConfigDb(simId);
        SendCellNetStateEvent();
    }

    return result;
}



//--------------------------------------------------------------------------------------------------
/**
 * Retreive the PIN code from the config tree.
 *
 * @return
 *    LE_OUT_OF_RANGE    Invalid simId
 *  - LE_NOT_FOUND       SIM PIN node isn't found in the config tree.
 *  - LE_OVERFLOW        PIN code exceeds the maximum length of 8 digits.
 *  - LE_UNDERFLOW       The PIN code is not long enough (min 4 digits).
 *  - LE_OK              The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_cellnet_GetSimPinCode
(
    le_sim_Id_t simId,
        ///< [IN]
        ///< SIM identifier.

    char* pinCodePtr,
        ///< [OUT]
        ///< Read the PIN code from the config tree.

    size_t pinCodeNumElements
        ///< [IN]

)
{
    le_result_t result=LE_OK;

    LE_DEBUG("simId= %d",simId);

    if (simId >= LE_SIM_ID_MAX)
    {
        LE_ERROR("Invalid simId (%d) provided!", simId);
        result = LE_OUT_OF_RANGE;
    }
    else
    {
        // Set the configuration path for the SIM.
        char configPath[LIMIT_MAX_PATH_BYTES];
        char simPin[LE_SIM_PIN_MAX_BYTES] = {0};
        le_cfg_IteratorRef_t simCfgRef;

        snprintf(configPath,
                 sizeof(configPath),
                 "%s/%d",
                 CFG_MODEMSERVICE_SIM_PATH, simId);

        // Check that the app has a configuration value.
        simCfgRef = le_cfg_CreateReadTxn(configPath);

        // test if the node exists
        if (!le_cfg_NodeExists(simCfgRef, CFG_NODE_PIN))
        {
            LE_ERROR("SIM PIN node isn't found in the config tree");
            result = LE_NOT_FOUND;
        }
        else
        {
            //read config tree
            result = le_cfg_GetString(simCfgRef,CFG_NODE_PIN,simPin,sizeof(simPin),"");
            if (result != LE_OK)
            {
                LE_ERROR("retrieved SIM PIN code exceeds the supplied buffer");
                result = LE_OVERFLOW;
            }
            else
            {
                 //void entry is taken into account
                 if ((strncmp(simPin,"",LE_SIM_PIN_MAX_LEN)!=0) &&
                     (strlen(simPin) < LE_SIM_PIN_MIN_LEN))
                {
                    LE_ERROR("retrieved SIM PIN code is not long enough (min 4 digits) ");
                    result = LE_UNDERFLOW;
                }
                else
                {
                    //copy pincode
                    strncpy ( pinCodePtr, simPin, sizeof(simPin) );
                    LE_DEBUG("SIM PIN code= %s retrieved OK",pinCodePtr);
                }
            }
        }
        le_cfg_CancelTxn(simCfgRef);
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Server Init
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Init the various events
    CommandEvent = le_event_CreateId("CellNet Command", sizeof(uint32_t));
    CellNetStateEvent = le_event_CreateId("CellNet State", sizeof(le_cellnet_State_t));

    // Create safe reference map for request references. The size of the map should be based on
    // the expected number of simultaneous cellular network requests, so take a reasonable guess.
    RequestRefMap = le_ref_CreateMap("CellNet Requests", 5);

    // Start the cellular network thread
    le_thread_Start( le_thread_Create("CellNet Thread", CellNetThread, NULL) );

    LE_INFO("Cellular Network Server is ready");
}

