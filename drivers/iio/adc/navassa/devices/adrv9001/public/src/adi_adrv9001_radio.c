/**
* \file
* \brief Contains features related function implementation defined in
* adi_adrv9001_radioctrl.h
*
* ADRV9001 API Version: $ADI_ADRV9001_API_VERSION$
*/

/**
* Copyright 2015 - 2018 Analog Devices Inc.
* Released under the ADRV9001 API license, for more information
* see the "LICENSE.txt" file in this zip file.
*/

#include "adi_adrv9001_user.h"

#include "adi_adrv9001.h"
#include "adi_adrv9001_arm.h"
#include "adi_adrv9001_error.h"
#include "adi_adrv9001_gpio.h"
#include "adi_adrv9001_radio.h"
#include "adi_adrv9001_rx.h"
#include "adi_adrv9001_spi.h"
#include "adi_adrv9001_tx.h"

#include "adrv9001_arm.h"
#include "adrv9001_arm_macros.h"
#include "adrv9001_bf.h"
#include "adrv9001_init.h"
#include "adrv9001_reg_addr_macros.h"
#include "adrv9001_validators.h"

static int32_t __maybe_unused adi_adrv9001_Radio_Carrier_Configure_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                            adi_common_Port_e port,
                                                                            adi_common_ChannelNumber_e channel,
                                                                            adi_adrv9001_Carrier_t *carrier)
{
    static const uint32_t INTERMEDIATE_FREQUENCY_MIN_HZ =   200000; /* 200 kHz */
    static const uint32_t INTERMEDIATE_FREQUENCY_MAX_HZ = 20000000; /* 20 MHz */
    static const uint64_t CARRIER_FREQUENCY_MIN_HZ =   30000000;    /* 30 MHz */
    static const uint64_t CARRIER_FREQUENCY_MAX_HZ = 6000000000;    /* 6 GHz */

    adi_adrv9001_ChannelState_e state = ADI_ADRV9001_CHANNEL_STANDBY;

    ADI_EXPECT(adi_adrv9001_Port_Validate, adrv9001, port);
    ADI_EXPECT(adi_adrv9001_Channel_Validate, adrv9001, channel);

    ADI_RANGE_CHECK(adrv9001, carrier->pllCalibration, ADI_ADRV9001_PLL_CALIBRATION_NORMAL, ADI_ADRV9001_PLL_CALIBRATION_RESERVED);
    ADI_RANGE_CHECK(adrv9001, carrier->loGenOptimization, ADI_ADRV9001_LO_GEN_OPTIMIZATION_PHASE_NOISE, ADI_ADRV9001_LO_GEN_OPTIMIZATION_POWER_CONSUMPTION);
    ADI_RANGE_CHECK(adrv9001, carrier->pllPower, ADI_ADRV9001_PLL_POWER_LOW, ADI_ADRV9001_PLL_POWER_HIGH);
    ADI_RANGE_CHECK_X(adrv9001, carrier->carrierFrequency_Hz, CARRIER_FREQUENCY_MIN_HZ, CARRIER_FREQUENCY_MAX_HZ, "%llu");

    if (0 != carrier->intermediateFrequency_Hz)
    {
        ADI_RANGE_CHECK(adrv9001, carrier->intermediateFrequency_Hz, INTERMEDIATE_FREQUENCY_MIN_HZ, INTERMEDIATE_FREQUENCY_MAX_HZ);
    }

    ADI_EXPECT(adi_adrv9001_Radio_Channel_State_Get, adrv9001, port, channel, &state);
    switch (state)
    {
    case ADI_ADRV9001_CHANNEL_STANDBY:  /* Falls through */
    case ADI_ADRV9001_CHANNEL_CALIBRATED:
        break;
    default:
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_API_FAIL,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         state,
                         "Invalid channel state. State must be STANDBY or CALIBRATED");
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Carrier_Configure(adi_adrv9001_Device_t *adrv9001,
                                             adi_common_Port_e port,
                                             adi_common_ChannelNumber_e channel,
                                             adi_adrv9001_Carrier_t *carrier)
{
    uint8_t armData[16] = { 0 };
    uint8_t extData[2] = { 0 };
    uint32_t offset = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_Carrier_Configure_Validate, adrv9001, port, channel, carrier);

    /* Loading byte array with parsed bytes from carrierFrequency_Hz word */
    adrv9001_LoadEightBytes(&offset, armData, carrier->carrierFrequency_Hz);
    armData[offset++] = carrier->pllCalibration;
    armData[offset++] = 0;
    armData[offset++] = carrier->loGenOptimization;
    armData[offset++] = carrier->pllPower;
    /* Loading byte array with parsed bytes from intermediateFrequency_Hz word */
    adrv9001_LoadFourBytes(&offset, armData, carrier->intermediateFrequency_Hz);

    /* Write carrier Frequency to ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Write, adrv9001, (uint32_t)ADRV9001_ADDR_ARM_MAILBOX_SET, &armData[0], sizeof(armData));

    extData[0] = adi_adrv9001_Radio_MailboxChannel_Get(port, channel);
    extData[1] = ADRV9001_ARM_OBJECTID_CHANNEL_CARRIER_FREQUENCY;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, (uint8_t)ADRV9001_ARM_SET_OPCODE, &extData[0], sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        (uint8_t)ADRV9001_ARM_SET_OPCODE,
                                        extData[1],
                                        (uint32_t)ADI_ADRV9001_SETCARRIER_FREQUENCY_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_SETCARRIER_FREQUENCY_INTERVAL_US);

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_Carrier_Inspect_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                          adi_common_Port_e port,
                                                                          adi_common_ChannelNumber_e channel,
                                                                          adi_adrv9001_Carrier_t *carrier)
{
    ADI_EXPECT(adi_adrv9001_Port_Validate, adrv9001, port);
    ADI_EXPECT(adi_adrv9001_Channel_Validate, adrv9001, channel);
    ADI_NULL_PTR_RETURN(&adrv9001->common, carrier);
    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Carrier_Inspect(adi_adrv9001_Device_t *adrv9001,
                                           adi_common_Port_e port,
                                           adi_common_ChannelNumber_e channel,
                                           adi_adrv9001_Carrier_t *carrier)
{
    uint8_t armData[16] = { 0 };
    uint8_t extData[2] = { 0 };
    uint32_t offset = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_Carrier_Inspect_Validate, adrv9001, port, channel, carrier);

    extData[0] = adi_adrv9001_Radio_MailboxChannel_Get(port, channel);
    extData[1] = ADRV9001_ARM_OBJECTID_CHANNEL_CARRIER_FREQUENCY;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write,
                   adrv9001,
                   (uint8_t)ADRV9001_ARM_GET_OPCODE,
                   &extData[0],
                   sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        (uint8_t)ADRV9001_ARM_GET_OPCODE,
                                        extData[1],
                                        (uint32_t)ADI_ADRV9001_GETCARRIER_FREQUENCY_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_GETCARRIER_FREQUENCY_INTERVAL_US);

    /* Read PLL Frequency from ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Read,
               adrv9001,
               (uint32_t)ADRV9001_ADDR_ARM_MAILBOX_GET,
               &armData[0],
               sizeof(armData),
               ADRV9001_ARM_MEM_READ_AUTOINCR);

    /*Form pllFrequency word with data read back from ARM mailbox*/
    adrv9001_ParseEightBytes(&offset, armData, &carrier->carrierFrequency_Hz);
    carrier->pllCalibration = (adi_adrv9001_PllCalibration_e)armData[offset++];
    offset++;
    carrier->loGenOptimization = (adi_adrv9001_LoGenOptimization_e)armData[offset++];
    carrier->pllPower = (adi_adrv9001_PllPower_e)armData[offset++];
    if (ADI_RX == port)
    {
        adrv9001_ParseFourBytes(&offset, armData, &carrier->intermediateFrequency_Hz);
    }
    else
    {
        carrier->intermediateFrequency_Hz = 0;
    }

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_PllStatus_Get_Validate(adi_adrv9001_Device_t *adrv9001, adi_adrv9001_Pll_e pll,
                                                                        bool *locked)
{
    ADI_RANGE_CHECK(adrv9001, pll, ADI_ADRV9001_PLL_LO1, ADI_ADRV9001_PLL_CLK_LP);
    ADI_NULL_PTR_RETURN(&adrv9001->common, locked);
    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_PllStatus_Get(adi_adrv9001_Device_t *adrv9001, adi_adrv9001_Pll_e pll, bool *locked)
{
    uint8_t pllLockStatusRead = 0;

    static const adrv9001_BfNvsPllMemMap_e instances[] = {
        ADRV9001_BF_RF1_PLL,
        ADRV9001_BF_RF2_PLL,
        ADRV9001_BF_AUX_PLL,
        ADRV9001_BF_CLK_PLL,
        ADRV9001_BF_CLK_PLL_LP
     };

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_PllStatus_Get_Validate, adrv9001, pll, locked);

    ADI_EXPECT(adrv9001_NvsPllMemMap_SynLock_Get, adrv9001, instances[pll], &pllLockStatusRead);
    *locked = (bool)pllLockStatusRead;

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_ChannelEnableMode_Set_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                            adi_common_ChannelNumber_e channel,
                                                                            adi_common_Port_e port,
                                                                            adi_adrv9001_ChannelEnableMode_e mode)
{
    ADI_EXPECT(adi_adrv9001_Channel_Validate, adrv9001, channel);
    ADI_EXPECT(adi_adrv9001_Port_Validate, adrv9001, port);
    ADI_RANGE_CHECK(adrv9001, mode, ADI_ADRV9001_SPI_MODE, ADI_ADRV9001_PIN_MODE);

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_ChannelEnableMode_Set(adi_adrv9001_Device_t *adrv9001,
                                           adi_common_Port_e port,
                                           adi_common_ChannelNumber_e channel,
                                           adi_adrv9001_ChannelEnableMode_e mode)
{
    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_ChannelEnableMode_Set_Validate, adrv9001, channel, port, mode);

    if (port == ADI_RX && channel == ADI_CHANNEL_1)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx1PinMode_Set, adrv9001, mode);
    }
    else if (port == ADI_RX && channel == ADI_CHANNEL_2)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx2PinMode_Set, adrv9001, mode);
    }
    else if (port == ADI_TX && channel == ADI_CHANNEL_1)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx1PinMode_Set, adrv9001, mode);
    }
    else if (port == ADI_TX && channel == ADI_CHANNEL_2)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx2PinMode_Set, adrv9001, mode);
    }
    else
    {
        ADI_SHOULD_NOT_EXECUTE(adrv9001);
    }

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_ChannelEnableMode_Get_Validate(adi_adrv9001_Device_t *adrv9001,
                                        adi_common_Port_e port,
                                        adi_common_ChannelNumber_e channel,
                                        adi_adrv9001_ChannelEnableMode_e *mode)
{
    ADI_EXPECT(adi_adrv9001_Channel_Validate, adrv9001, channel);
    ADI_EXPECT(adi_adrv9001_Port_Validate, adrv9001, port);
    ADI_NULL_PTR_RETURN(&adrv9001->common, mode);

    ADI_API_RETURN(adrv9001);
}
int32_t adi_adrv9001_Radio_ChannelEnableMode_Get(adi_adrv9001_Device_t *adrv9001,
                                           adi_common_Port_e port,
                                           adi_common_ChannelNumber_e channel,
                                           adi_adrv9001_ChannelEnableMode_e *mode)
{
    uint8_t regVal = 0;
    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_ChannelEnableMode_Get_Validate, adrv9001, port, channel, mode);

    if (port == ADI_RX && channel == ADI_CHANNEL_1)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx1PinMode_Get, adrv9001, &regVal);
    }
    else if (port == ADI_RX && channel == ADI_CHANNEL_2)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx2PinMode_Get, adrv9001, &regVal);
    }
    else if (port == ADI_TX && channel == ADI_CHANNEL_1)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx1PinMode_Get, adrv9001, &regVal);
    }
    else if (port == ADI_TX && channel == ADI_CHANNEL_2)
    {
        ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx2PinMode_Get, adrv9001, &regVal);
    }
    else
    {
        ADI_SHOULD_NOT_EXECUTE(adrv9001);
    }

    *mode = (adi_adrv9001_ChannelEnableMode_e)regVal;

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_State_Get(adi_adrv9001_Device_t *adrv9001, adi_adrv9001_RadioState_t *radioState)
{
    uint8_t regValue = 0;

    /* Range checks */
    ADI_ENTRY_PTR_EXPECT(adrv9001, radioState);

    ADRV9001_SPIREADBYTE(adrv9001, "arm_cmd_status_8", ADRV9001_ADDR_ARM_CMD_STATUS_8, &regValue);

    radioState->systemState         = regValue & 0x03;
    radioState->monitorModeState    = (regValue >> 2) & 0x03;
    radioState->bootState           = (regValue >> 4) & 0x0F;

    ADRV9001_SPIREADBYTE(adrv9001, "arm_cmd_status_9", ADRV9001_ADDR_ARM_CMD_STATUS_9, &regValue);

    radioState->channelStates[0][0] = (regValue >> 0) & 0x03;   /* Rx1 */
    radioState->channelStates[0][1] = (regValue >> 2) & 0x03;   /* Rx2 */
    radioState->channelStates[1][0] = (regValue >> 4) & 0x03;   /* Tx1 */
    radioState->channelStates[1][1] = (regValue >> 6) & 0x03;   /* Tx2 */


    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_Channel_State_Get_Validate(adi_adrv9001_Device_t *adrv9001,
                                        adi_common_Port_e port,
                                        adi_common_ChannelNumber_e channel,
                                        adi_adrv9001_ChannelState_e *channelState)
{
    ADI_RANGE_CHECK(adrv9001, port, ADI_RX, ADI_TX);
    ADI_RANGE_CHECK(adrv9001, channel, ADI_CHANNEL_1, ADI_CHANNEL_2);
    ADI_NULL_PTR_RETURN(&adrv9001->common, channelState);
    ADI_API_RETURN(adrv9001);
}


int32_t adi_adrv9001_Radio_Channel_State_Get(adi_adrv9001_Device_t *adrv9001,
                                             adi_common_Port_e port,
                                             adi_common_ChannelNumber_e channel,
                                             adi_adrv9001_ChannelState_e *channelState)
{
    uint8_t regValue = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_Channel_State_Get_Validate, adrv9001, port, channel, channelState);

    ADRV9001_SPIREADBYTE(adrv9001, "arm_cmd_status_9", ADRV9001_ADDR_ARM_CMD_STATUS_9, &regValue);

    if (ADI_RX == port && ADI_CHANNEL_1 == channel)
    {
        *channelState = (regValue >> 0) & 0x03; /* Rx1 */
    }
    else if (ADI_RX == port && ADI_CHANNEL_2 == channel)
    {
        *channelState = (regValue >> 2) & 0x03; /* Rx2 */
    }
    else if (ADI_TX == port && ADI_CHANNEL_1 == channel)
    {
        *channelState = (regValue >> 4) & 0x03; /* Tx1 */
    }
    else if (ADI_TX == port && ADI_CHANNEL_2 == channel)
    {
        *channelState = (regValue >> 6) & 0x03; /* Tx2 */
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channel_Prime(adi_adrv9001_Device_t *adrv9001,
                                         adi_common_Port_e port,
                                         adi_common_ChannelNumber_e channel,
                                         bool prime)
{
    return adi_adrv9001_Radio_Channels_Prime(adrv9001, &port, &channel, 1, prime);
}

static int32_t adi_adrv9001_Channel_DisableRF_Wait(adi_adrv9001_Device_t *adrv9001,
                                                   adi_common_Port_e port,
                                                   adi_common_ChannelNumber_e channel,
                                                   uint8_t numTries);

static int32_t adi_adrv9001_Radio_ToPrimed_Fix(adi_adrv9001_Device_t *adrv9001,
					       const adi_common_Port_e ports[],
					       adi_common_ChannelNumber_e channels[],
					       const uint32_t length)
{
    uint8_t i = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };
    static const uint8_t NUM_TRIES = 5;

    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);

    for (i = 0; i < length; i++)
    {
        uint8_t port_index = 0;
        uint8_t chan_index = 0;

        if (ports[i] != ADI_TX)
            continue;

        adi_common_port_to_index(ADI_RX, &port_index);
        adi_common_channel_to_index(channels[i], &chan_index);
        if (currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_RF_ENABLED)
            continue;

        /* Disable and Enable the RF in the same channel to make sure the capture still works */
        ADI_EXPECT(adi_adrv9001_Channel_DisableRF_Wait, adrv9001, ADI_RX, channels[i], NUM_TRIES);
        ADI_EXPECT(adi_adrv9001_Radio_Channel_EnableRf, adrv9001, ADI_RX, channels[i], true);
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channels_Prime(adi_adrv9001_Device_t *adrv9001,
                                          adi_common_Port_e ports[],
                                          adi_common_ChannelNumber_e channels[],
                                          uint32_t length,
                                          bool prime)
{
    uint8_t i = 0;
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    uint8_t opCode = 0;
    uint8_t mailboxChannelMask = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    ADI_PERFORM_VALIDATION(adi_adrv9001_Channel_State_GenericValidate, adrv9001, ports, channels, length);

    // Validate current state
    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);
    for (i = 0; i < length; i++)
    {
        adi_common_port_to_index(ports[i], &port_index);
        adi_common_channel_to_index(channels[i], &chan_index);
        if(currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_CALIBRATED &&
           currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_PRIMED)
        {
            if (prime)
            {
                ADI_ERROR_REPORT(&adrv9001->common,
                                 ADI_COMMON_ERRSRC_API,
                                 ADI_COMMON_ERR_API_FAIL,
                                 ADI_COMMON_ACT_ERR_CHECK_PARAM,
                                 channelState,
                                 "Error while attempting to prime channel. Channel must be in the CALIBRATED state to be primed.");
            }
            else
            {
                ADI_ERROR_REPORT(&adrv9001->common,
                                 ADI_COMMON_ERRSRC_API,
                                 ADI_COMMON_ERR_API_FAIL,
                                 ADI_COMMON_ACT_ERR_CHECK_PARAM,
                                 channelState,
                                 "Error while attempting to un-prime channel. Channel must be in the PRIMED state to be un-primed.");
            }
            ADI_API_RETURN(adrv9001);
        }
    }

    /* TODO: What if 1 or more channels are already primed? */
    mailboxChannelMask = adi_adrv9001_Radio_MailboxChannelMask_Get(ports, channels, length);

    if (true == prime)
    {
        opCode = ADRV9001_ARM_RADIOON_OPCODE;
    }
    else
    {
        opCode = ADRV9001_ARM_RADIOOFF_OPCODE;
    }

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, opCode, &mailboxChannelMask, 1);

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        opCode,
                                        0,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_INTERVAL_US);

    /*
     * FIXME: Remove this as soon as it is fixed in the arm firmware. Most likely, it will
     * be fixed in the next release and this does not have to be included. Consideraing the
     * following state:
     *    RX1: rf_enabled
     *    RX2: primed
     *    TX1: primed
     *    TX2: rf_enabled
     * In the previous state everything works as expected the data is captured ai RX1.
     * However moving TX1 to calibrated and then back to primed, the signal in RX1 is lost as if
     * the RX1 path was powered down. This seems only to affect ports on the same channel meaning
     * that if TX2 is moved to calibrated and then back to primed or rf_enabled everything works as
     * expected.
     */
    if (prime)
        ADI_EXPECT(adi_adrv9001_Radio_ToPrimed_Fix, adrv9001, ports, channels, length);

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channel_EnableRf(adi_adrv9001_Device_t *adrv9001,
                                            adi_common_Port_e port,
                                            adi_common_ChannelNumber_e channel,
                                            bool enable)
{
    return adi_adrv9001_Radio_Channels_EnableRf(adrv9001, &port, &channel, 1, enable);
}

int32_t adi_adrv9001_Radio_Channels_EnableRf(adi_adrv9001_Device_t *adrv9001,
                                             adi_common_Port_e ports[],
                                             adi_common_ChannelNumber_e channels[],
                                             uint32_t length,
                                             bool enable)
{
    uint8_t i = 0;
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    adi_adrv9001_ChannelEnableMode_e enableMode = ADI_ADRV9001_SPI_MODE;
    adi_adrv9001_RadioState_t currentState = { 0 };

    ADI_PERFORM_VALIDATION(adi_adrv9001_Channel_State_GenericValidate, adrv9001, ports, channels, length);


    // Validate current state
    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);
    for (i = 0; i < length; i++)
    {
        ADI_EXPECT(adi_adrv9001_Radio_ChannelEnableMode_Get, adrv9001, ports[i], channels[i], &enableMode);
        if (ADI_ADRV9001_SPI_MODE != enableMode)
        {
            ADI_ERROR_REPORT(&adrv9001->common,
                             ADI_COMMON_ERRSRC_API,
                             ADI_COMMON_ERR_API_FAIL,
                             ADI_COMMON_ACT_ERR_CHECK_PARAM,
                             enableMode,
                             "Error while attempting to enable/disable RF for channel. Channel enable mode must be ADI_ADRV9001_SPI_MODE");
            ADI_API_RETURN(adrv9001);
        }

        adi_common_port_to_index(ports[i], &port_index);
        adi_common_channel_to_index(channels[i], &chan_index);
        if (currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_PRIMED &&
            currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_RF_ENABLED)
        {
            if (enable)
            {
                ADI_ERROR_REPORT(&adrv9001->common,
                                 ADI_COMMON_ERRSRC_API,
                                 ADI_COMMON_ERR_API_FAIL,
                                 ADI_COMMON_ACT_ERR_CHECK_PARAM,
                                 channelState,
                                 "Error while attempting to enable RF for channel. Channel must be in the PRIMED state to enable RF.");
            }
            else
            {
                ADI_ERROR_REPORT(&adrv9001->common,
                                 ADI_COMMON_ERRSRC_API,
                                 ADI_COMMON_ERR_API_FAIL,
                                 ADI_COMMON_ACT_ERR_CHECK_PARAM,
                                 channelState,
                                 "Error while attempting to disable RF for channel. Channel must be in the RF_ENABLED state to disable RF.");
            }
            ADI_API_RETURN(adrv9001)
        }

        /* Set the enable field for the specified channel */
        if (ports[i] == ADI_RX && channels[i] == ADI_CHANNEL_1)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx1Enable_Set, adrv9001, enable);
        }
        else if (ports[i] == ADI_RX && channels[i] == ADI_CHANNEL_2)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicRx2Enable_Set, adrv9001, enable);
        }
        else if (ports[i] == ADI_TX && channels[i] == ADI_CHANNEL_1)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx1Enable_Set, adrv9001, enable);
        }
        else if (ports[i] == ADI_TX && channels[i] == ADI_CHANNEL_2)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicTx2Enable_Set, adrv9001, enable);
        }
        /* TODO: Is ORX necessary? */
        else if(ports[i] == ADI_ORX && channels[i] == ADI_CHANNEL_1)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicOrx1Enable_Set, adrv9001, enable);
        }
        else if(ports[i] == ADI_ORX && channels[i] == ADI_CHANNEL_2)
        {
            ADI_EXPECT(adrv9001_NvsRegmapCore2_BbicOrx2Enable_Set, adrv9001, enable);
        }
        else
        {
            ADI_SHOULD_NOT_EXECUTE(adrv9001);
        }
    }

    ADI_API_RETURN(adrv9001);
}

static int32_t adi_adrv9001_Channel_DisableRF_Wait(adi_adrv9001_Device_t *adrv9001,
                                                   adi_common_Port_e port,
                                                   adi_common_ChannelNumber_e channel,
                                                   uint8_t numTries)
{
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    uint8_t i = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    adi_common_port_to_index(port, &port_index);
    adi_common_channel_to_index(channel, &chan_index);

    ADI_EXPECT(adi_adrv9001_Radio_Channel_EnableRf, adrv9001, port, channel, false);
    for (i = 0; i < numTries; i++)
    {
        ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);
        if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_PRIMED)
        {
            return ADI_COMMON_ACT_NO_ACTION;
        }
    }

    return ADI_COMMON_ACT_ERR_RESET_MODULE;
}

int32_t adi_adrv9001_Radio_Channel_PowerDown(adi_adrv9001_Device_t *adrv9001,
                                             adi_common_Port_e port,
                                             adi_common_ChannelNumber_e channel)
{
    return adi_adrv9001_Radio_Channels_PowerDown(adrv9001, &port, &channel, 1);
}

static int32_t __maybe_unused adi_adrv9001_Radio_Channels_PowerDown_Validate(adi_adrv9001_Device_t *adrv9001,
                                         adi_common_Port_e ports[],
                                         adi_common_ChannelNumber_e channels[],
                                         uint32_t length)
{
    uint8_t i = 0;
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    // Validate current state
    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);
    for (i = 0; i < length; i++)
    {
        adi_common_port_to_index(ports[i], &port_index);
        adi_common_channel_to_index(channels[i], &chan_index);
        if (currentState.channelStates[port_index][chan_index] != ADI_ADRV9001_CHANNEL_CALIBRATED)
        {
            ADI_ERROR_REPORT(&adrv9001->common,
                             ADI_COMMON_ERRSRC_API,
                             ADI_COMMON_ERR_API_FAIL,
                             ADI_COMMON_ACT_ERR_CHECK_PARAM,
                             channelState,
                             "Error while attempting to power down channel. Channel must be in the CALIBRATED state to be powered down.");
            ADI_API_RETURN(adrv9001);
        }
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channels_PowerDown(adi_adrv9001_Device_t *adrv9001,
                                              adi_common_Port_e ports[],
                                              adi_common_ChannelNumber_e channels[],
                                              uint32_t length)
{

    uint8_t mailboxChannelMask = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Channel_State_GenericValidate, adrv9001, ports, channels, length);
    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_Channels_PowerDown_Validate, adrv9001, ports, channels, length);

    mailboxChannelMask = adi_adrv9001_Radio_MailboxChannelMask_Get(ports, channels, length);

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, ADRV9001_ARM_POWERDOWN_OPCODE, &mailboxChannelMask, 1);

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        ADRV9001_ARM_POWERDOWN_OPCODE,
                                        0,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_INTERVAL_US);

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channel_PowerUp(adi_adrv9001_Device_t *adrv9001,
                                           adi_common_Port_e port,
                                           adi_common_ChannelNumber_e channel)
{
    return adi_adrv9001_Radio_Channels_PowerUp(adrv9001, &port, &channel, 1);
}

int32_t adi_adrv9001_Radio_Channels_PowerUp(adi_adrv9001_Device_t *adrv9001,
                                            adi_common_Port_e ports[],
                                            adi_common_ChannelNumber_e channels[],
                                            uint32_t length)
{
    uint8_t mailboxChannelMask = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Channel_State_GenericValidate, adrv9001, ports, channels, length);

    mailboxChannelMask = adi_adrv9001_Radio_MailboxChannelMask_Get(ports, channels, length);

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, ADRV9001_ARM_POWERUP_OPCODE, &mailboxChannelMask, 1);

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        ADRV9001_ARM_POWERUP_OPCODE,
                                        0,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_RADIOONOFF_INTERVAL_US);

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_Channel_ToCalibrated(adi_adrv9001_Device_t *adrv9001,
                                                adi_common_Port_e port,
                                                adi_common_ChannelNumber_e channel)
{
    static const uint8_t NUM_TRIES = 10;
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    adi_common_port_to_index(port, &port_index);
    adi_common_channel_to_index(channel, &chan_index);

    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);
    if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_STANDBY)
    {
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_INV_PARAM,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         currentState,
                         "Error moving channel to CALIBRATED state - channel is in STANDBY. Use the adi_adrv9001_InitCals_Run() function instead");
        ADI_API_RETURN(adrv9001)
    }
    else if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_CALIBRATED)
    {
        /* Nothing to do, already in CALIBRATED state */
    }
    else
    {
        if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_RF_ENABLED)
        {
            ADI_EXPECT(adi_adrv9001_Channel_DisableRF_Wait, adrv9001, port, channel, NUM_TRIES);
        }
        ADI_EXPECT(adi_adrv9001_Radio_Channel_Prime, adrv9001, port, channel, false);
    }

    ADI_API_RETURN(adrv9001)
}

int32_t adi_adrv9001_Radio_Channel_ToPrimed(adi_adrv9001_Device_t *adrv9001,
                                            adi_common_Port_e port,
                                            adi_common_ChannelNumber_e channel)
{
    static const uint8_t NUM_TRIES = 5;
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    adi_common_port_to_index(port, &port_index);
    adi_common_channel_to_index(channel, &chan_index);

    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);

    if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_STANDBY)
    {
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_INV_PARAM,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         currentState,
                         "Error moving channel to PRIMED state - channel is in STANDBY. Use the adi_adrv9001_InitCals_Run() function to move to CALIBRATED state first");
        ADI_API_RETURN(adrv9001)
    }
    else if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_CALIBRATED)
    {
        ADI_EXPECT(adi_adrv9001_Radio_Channel_Prime, adrv9001, port, channel, true);
    }
    else if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_PRIMED)
    {
        /* Nothing to do, already in PRIMED state */
    }
    else
    {
        ADI_EXPECT(adi_adrv9001_Channel_DisableRF_Wait, adrv9001, port, channel, NUM_TRIES);
    }

    ADI_API_RETURN(adrv9001)
}

int32_t adi_adrv9001_Radio_Channel_ToRfEnabled(adi_adrv9001_Device_t *adrv9001,
                                               adi_common_Port_e port,
                                               adi_common_ChannelNumber_e channel)
{
    uint8_t port_index = 0;
    uint8_t chan_index = 0;
    adi_adrv9001_RadioState_t currentState = { 0 };

    adi_common_port_to_index(port, &port_index);
    adi_common_channel_to_index(channel, &chan_index);

    ADI_EXPECT(adi_adrv9001_Radio_State_Get, adrv9001, &currentState);

    if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_STANDBY)
    {
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_INV_PARAM,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         currentState,
                         "Error moving channel to RF_ENABLED state - channel is in STANDBY. Use the adi_adrv9001_InitCals_Run() function to move to CALIBRATED state first");
        ADI_API_RETURN(adrv9001)
    }
    else if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_RF_ENABLED)
    {
        /* Nothing to do, already in RF_ENABLED state */
    }
    else
    {
        if (currentState.channelStates[port_index][chan_index] == ADI_ADRV9001_CHANNEL_CALIBRATED)
        {
            ADI_EXPECT(adi_adrv9001_Radio_Channel_Prime, adrv9001, port, channel, true);
        }
        ADI_EXPECT(adi_adrv9001_Radio_Channel_EnableRf, adrv9001, port, channel, true);
    }

    ADI_API_RETURN(adrv9001)
}

static int32_t __maybe_unused adi_adrv9001_Radio_Channel_ToState_Validate(adi_adrv9001_Device_t *adrv9001,
                                      adi_common_Port_e port,
                                      adi_common_ChannelNumber_e channel,
                                      adi_adrv9001_ChannelState_e state)
{
    ADI_RANGE_CHECK(adrv9001, port, ADI_RX, ADI_TX);
    ADI_RANGE_CHECK(adrv9001, channel, ADI_CHANNEL_1, ADI_CHANNEL_2);
    ADI_RANGE_CHECK(adrv9001, state, ADI_ADRV9001_CHANNEL_CALIBRATED, ADI_ADRV9001_CHANNEL_RF_ENABLED);
    ADI_API_RETURN(adrv9001)
}

int32_t adi_adrv9001_Radio_Channel_ToState(adi_adrv9001_Device_t *adrv9001,
                                           adi_common_Port_e port,
                                           adi_common_ChannelNumber_e channel,
                                           adi_adrv9001_ChannelState_e state)
{
    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_Channel_ToState_Validate, adrv9001, port, channel, state);

    switch (state)
    {
    case ADI_ADRV9001_CHANNEL_CALIBRATED:
        ADI_EXPECT(adi_adrv9001_Radio_Channel_ToCalibrated, adrv9001, port, channel);
        break;
    case ADI_ADRV9001_CHANNEL_PRIMED:
        ADI_EXPECT(adi_adrv9001_Radio_Channel_ToPrimed, adrv9001, port, channel);
        break;
    case ADI_ADRV9001_CHANNEL_RF_ENABLED:
        ADI_EXPECT(adi_adrv9001_Radio_Channel_ToRfEnabled, adrv9001, port, channel);
        break;
    default:
        ADI_SHOULD_NOT_EXECUTE(adrv9001);
    }

    ADI_API_RETURN(adrv9001)
}

static int32_t __maybe_unused adi_adrv9001_Radio_PllLoopFilter_Set_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                            adi_adrv9001_Pll_e pll,
                                                                            adi_adrv9001_PllLoopFilterCfg_t *pllLoopFilterConfig)
{
    static const uint8_t  MINIMUM_PLL_LOOP_FILTER_PHASE_MARGIN_DEGREES = 40;
    static const uint8_t  MAXIMUM_PLL_LOOP_FILTER_PHASE_MARGIN_DEGREES = 85;
    static const uint16_t MINIMUM_LOOP_FILTER_BANDWIDTH_KHZ = 50;
    static const uint16_t MAXIMUM_LOOP_FILTER_BANDWIDTH_KHZ = 1500;
    static const uint8_t  MINIMUM_POWER_SCALE_FACTOR = 0;
    static const uint8_t  MAXIMUM_POWER_SCALE_FACTOR = 10;

    /* Check adrv9001 pointer is not null */
    ADI_ENTRY_PTR_EXPECT(adrv9001, pllLoopFilterConfig);

    /*Check that PLL selected is valid*/
    ADI_RANGE_CHECK(adrv9001, pll, ADI_ADRV9001_PLL_LO1, ADI_ADRV9001_PLL_AUX);

    /*Check that Loop Filter phase margin is between 40-85 degrees*/
    ADI_RANGE_CHECK(adrv9001,
                        pllLoopFilterConfig->phaseMargin_degrees,
                        MINIMUM_PLL_LOOP_FILTER_PHASE_MARGIN_DEGREES,
                        MAXIMUM_PLL_LOOP_FILTER_PHASE_MARGIN_DEGREES);

    /*Check that loop filter bandwidth is between 50Khz - 1500Khz*/
    ADI_RANGE_CHECK(adrv9001,
                        pllLoopFilterConfig->loopBandwidth_kHz,
                        MINIMUM_LOOP_FILTER_BANDWIDTH_KHZ,
                        MAXIMUM_LOOP_FILTER_BANDWIDTH_KHZ);

    /*Check that power scale factor is between 0-10*/
    ADI_RANGE_CHECK(adrv9001,
                        pllLoopFilterConfig->powerScale,
                        MINIMUM_POWER_SCALE_FACTOR,
                        MAXIMUM_POWER_SCALE_FACTOR);

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_PllLoopFilter_Set(adi_adrv9001_Device_t *adrv9001,
                                             adi_adrv9001_Pll_e pll,
                                             adi_adrv9001_PllLoopFilterCfg_t *pllLoopFilterConfig)
{
    uint8_t armData[4] = { 0 };
    uint8_t extData[3] = { 0 };

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_PllLoopFilter_Set_Validate, adrv9001, pll, pllLoopFilterConfig);

    /* Loading byte array with parsed bytes from pllLoopFilterConfig struct */
    armData[0] = pllLoopFilterConfig->phaseMargin_degrees;
    armData[1] = (uint8_t)(pllLoopFilterConfig->loopBandwidth_kHz & 0x00FF);
    armData[2] = (uint8_t)((pllLoopFilterConfig->loopBandwidth_kHz >> 8) & 0x00FF);
    armData[3] = pllLoopFilterConfig->powerScale;

    /* Write PLL Frequency to ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Write,
                   adrv9001,
                   (uint32_t)ADRV9001_ADDR_ARM_MAILBOX_SET,
                   &armData[0],
                   sizeof(armData));

    /* Executing the SET PLL Freq command */
    extData[0] = 0;
    extData[1] = ADRV9001_ARM_OBJECTID_PLL_LOOPFILTER;
    extData[2] = (uint8_t)pll;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write,
                   adrv9001,
                   (uint8_t)ADRV9001_ARM_SET_OPCODE,
                   &extData[0],
                   sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
        (uint8_t)ADRV9001_ARM_SET_OPCODE,
        extData[1],
        (uint32_t)ADI_ADRV9001_SETLOOPFILTER_TIMEOUT_US,
        (uint32_t)ADI_ADRV9001_SETLOOPFILTER_INTERVAL_US);

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_PllLoopFilter_Get_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                            adi_adrv9001_Pll_e pll,
                                                                            adi_adrv9001_PllLoopFilterCfg_t *pllLoopFilterConfig)
{
    ADI_RANGE_CHECK(adrv9001, pll, ADI_ADRV9001_PLL_LO1, ADI_ADRV9001_PLL_AUX);
    ADI_NULL_PTR_RETURN(&adrv9001->common, pllLoopFilterConfig);
    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_PllLoopFilter_Get(adi_adrv9001_Device_t *adrv9001,
                                             adi_adrv9001_Pll_e pll,
                                             adi_adrv9001_PllLoopFilterCfg_t *pllLoopFilterConfig)
{
    uint8_t armData[6] = { 0 };
    uint8_t extData[3] = { 0 };

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_PllLoopFilter_Get_Validate, adrv9001, pll, pllLoopFilterConfig);

    /* Check adrv9001 pointer is not null */
    ADI_ENTRY_PTR_EXPECT(adrv9001, pllLoopFilterConfig);

    /* Executing the GET PLL Freq command */
    extData[0] = 0;
    extData[1] = ADRV9001_ARM_OBJECTID_PLL_LOOPFILTER;
    extData[2] = (uint8_t)pll;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, (uint8_t)ADRV9001_ARM_GET_OPCODE, &extData[0], sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
        (uint8_t)ADRV9001_ARM_GET_OPCODE,
        extData[1],
        (uint32_t)ADI_ADRV9001_GETLOOPFILTER_TIMEOUT_US,
        (uint32_t)ADI_ADRV9001_GETLOOPFILTER_INTERVAL_US);

    /* Read PLL Loop Filter from ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Read,
                   adrv9001,
                   (uint32_t)ADRV9001_ADDR_ARM_MAILBOX_GET,
                   &armData[0],
                   sizeof(armData),
                   false);

    /*Deserialize ARM Data into pllLoopFilterConfig Structure*/
    pllLoopFilterConfig->phaseMargin_degrees = armData[0];
    pllLoopFilterConfig->loopBandwidth_kHz = (((uint16_t)armData[1]) |
                                              ((uint16_t)armData[2] << 8));
    pllLoopFilterConfig->powerScale = armData[3];
    pllLoopFilterConfig->effectiveLoopBandwidth_kHz = (((uint16_t)armData[4]) |
                                                       ((uint16_t)armData[5] << 8));

    ADI_API_RETURN(adrv9001);
}

uint8_t adi_adrv9001_Radio_MailboxChannel_Get(adi_common_Port_e port, adi_common_ChannelNumber_e channel)
{
    return adi_adrv9001_Radio_MailboxChannelMask_Get(&port, &channel, 1);
}

uint8_t adi_adrv9001_Radio_MailboxChannelMask_Get(adi_common_Port_e ports[],
                                                  adi_common_ChannelNumber_e channels[],
                                                  uint32_t length)
{
    uint8_t i = 0;
    uint8_t channelMask = 0;

    for (i = 0; i < length; i++)
    {
        if (ports[i] == ADI_RX && channels[i] == ADI_CHANNEL_1)
        {
            channelMask |= ADI_ADRV9001_RX1;
        }
        else if (ports[i] == ADI_RX && channels[i] == ADI_CHANNEL_2)
        {
            channelMask |= ADI_ADRV9001_RX2;
        }
        else if (ports[i] == ADI_TX && channels[i] == ADI_CHANNEL_1)
        {
            channelMask |= ADI_ADRV9001_TX1;
        }
        else if (ports[i] == ADI_TX && channels[i] == ADI_CHANNEL_2)
        {
            channelMask |= ADI_ADRV9001_TX2;
        }
        else if (ports[i] == ADI_ORX && channels[i] == ADI_CHANNEL_1)
        {
            channelMask |= ADI_ADRV9001_ORX1;
        }
        else if (ports[i] == ADI_ORX && channels[i] == ADI_CHANNEL_2)
        {
            channelMask |= ADI_ADRV9001_ORX2;
        }
        else
        {
        }
    }

    return channelMask;
}

static int32_t __maybe_unused adi_adrv9001_Radio_ChannelEnablementDelays_Configure_Validate(adi_adrv9001_Device_t *adrv9001,
                                                adi_common_Port_e port,
                                                adi_common_ChannelNumber_e channel,
                                                adi_adrv9001_ChannelEnablementDelays_t *delays)
{
    static const uint32_t MAX_DELAY = 0x00FFFFFF;
    static const adi_adrv9001_GpioSignal_e frontendControlSignals[2][2] = {
        { ADI_ADRV9001_GPIO_SIGNAL_RX1_EXT_FRONTEND_CONTROL, ADI_ADRV9001_GPIO_SIGNAL_RX2_EXT_FRONTEND_CONTROL },
        { ADI_ADRV9001_GPIO_SIGNAL_TX1_EXT_FRONTEND_CONTROL, ADI_ADRV9001_GPIO_SIGNAL_TX2_EXT_FRONTEND_CONTROL }
    };

    adi_adrv9001_GpioCfg_t gpioConfig = { 0 };
    uint8_t port_idx, chan_idx = 0;
    adi_adrv9001_ChannelState_e state = ADI_ADRV9001_CHANNEL_STANDBY;

    ADI_RANGE_CHECK(adrv9001, port, ADI_RX, ADI_TX);
    ADI_RANGE_CHECK(adrv9001, channel, ADI_CHANNEL_1, ADI_CHANNEL_2);

    ADI_NULL_PTR_RETURN(&adrv9001->common, delays);
    ADI_RANGE_CHECK(adrv9001, delays->riseToOnDelay,          0, MAX_DELAY);
    ADI_RANGE_CHECK(adrv9001, delays->riseToAnalogOnDelay,    0, MAX_DELAY);
    ADI_RANGE_CHECK(adrv9001, delays->fallToOffDelay,         0, MAX_DELAY);
    ADI_RANGE_CHECK(adrv9001, delays->guardDelay,             0, MAX_DELAY);
    ADI_RANGE_CHECK(adrv9001, delays->holdDelay,              0, MAX_DELAY);

    if (ADI_TX == port)
    {
        ADI_RANGE_CHECK(adrv9001, delays->holdDelay, 0, delays->fallToOffDelay);
    }
    if (ADI_RX == port)
    {
        ADI_RANGE_CHECK(adrv9001, delays->fallToOffDelay, 0, delays->holdDelay);
    }

    adi_common_port_to_index(port, &port_idx);
    adi_common_channel_to_index(channel, &chan_idx);
    ADI_EXPECT(adi_adrv9001_gpio_Inspect, adrv9001, frontendControlSignals[port_idx][chan_idx], &gpioConfig);
    if (ADI_ADRV9001_GPIO_UNASSIGNED != gpioConfig.pin)
    {
        ADI_RANGE_CHECK(adrv9001, delays->riseToAnalogOnDelay, 0, delays->riseToOnDelay);
    }

    ADI_EXPECT(adi_adrv9001_Radio_Channel_State_Get, adrv9001, port, channel, &state);
    switch (state)
    {
    case ADI_ADRV9001_CHANNEL_STANDBY:      /* Falls through */
    case ADI_ADRV9001_CHANNEL_CALIBRATED:
        break;
    default:
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_INV_PARAM,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         state,
                         "Invalid channel state. Channel state must be one of STANDBY, CALIBRATED");
        break;
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_ChannelEnablementDelays_Configure(adi_adrv9001_Device_t *adrv9001,
                                                             adi_common_Port_e port,
                                                             adi_common_ChannelNumber_e channel,
                                                             adi_adrv9001_ChannelEnablementDelays_t *delays)
{
    uint8_t armData[20] = { 0 };
    uint8_t extData[2] = { 0 };
    uint32_t offset = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_ChannelEnablementDelays_Configure_Validate, adrv9001, port, channel, delays);

    /* Serialize struct to bytes */
    adrv9001_LoadFourBytes(&offset, armData, delays->riseToOnDelay);
    adrv9001_LoadFourBytes(&offset, armData, delays->riseToAnalogOnDelay);
    adrv9001_LoadFourBytes(&offset, armData, delays->fallToOffDelay);
    adrv9001_LoadFourBytes(&offset, armData, delays->guardDelay);
    adrv9001_LoadFourBytes(&offset, armData, delays->holdDelay);

    /* Write timing parameters to ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Write, adrv9001, ADRV9001_ADDR_ARM_MAILBOX_SET, &armData[0], sizeof(armData));

    extData[0] = adi_adrv9001_Radio_MailboxChannel_Get(port, channel);

    /* Executing the SET command */
    extData[1] = ADRV9001_ARM_OBJECTID_TDD_TIMING_PARAMS;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, (uint8_t)ADRV9001_ARM_SET_OPCODE, &extData[0], sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        (uint8_t)ADRV9001_ARM_SET_OPCODE,
                                        extData[1],
                                        (uint32_t)ADI_ADRV9001_DEFAULT_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_DEFAULT_INTERVAL_US);

    ADI_API_RETURN(adrv9001);
}

static int32_t __maybe_unused adi_adrv9001_Radio_ChannelEnablementDelays_Inspect_Validate(adi_adrv9001_Device_t *adrv9001,
                                                                                          adi_common_Port_e port,
                                                                                          adi_common_ChannelNumber_e channel,
                                                                                          adi_adrv9001_ChannelEnablementDelays_t *delays)
{
    adi_adrv9001_ChannelState_e state = ADI_ADRV9001_CHANNEL_STANDBY;

    ADI_RANGE_CHECK(adrv9001, port, ADI_RX, ADI_TX);
    ADI_RANGE_CHECK(adrv9001, channel, ADI_CHANNEL_1, ADI_CHANNEL_2);
    ADI_NULL_PTR_RETURN(&adrv9001->common, delays);

    ADI_EXPECT(adi_adrv9001_Radio_Channel_State_Get, adrv9001, port, channel, &state);
    switch (state)
    {
    case ADI_ADRV9001_CHANNEL_PRIMED:       /* Falls through */
    case ADI_ADRV9001_CHANNEL_RF_ENABLED:
        break;
    default:
        ADI_ERROR_REPORT(&adrv9001->common,
                         ADI_COMMON_ERRSRC_API,
                         ADI_COMMON_ERR_INV_PARAM,
                         ADI_COMMON_ACT_ERR_CHECK_PARAM,
                         state,
                         "Invalid channel state. Channel state must be one of PRIMED, RF_ENABLED");
        break;
    }

    ADI_API_RETURN(adrv9001);
}

int32_t adi_adrv9001_Radio_ChannelEnablementDelays_Inspect(adi_adrv9001_Device_t *adrv9001,
                                                           adi_common_Port_e port,
                                                           adi_common_ChannelNumber_e channel,
                                                           adi_adrv9001_ChannelEnablementDelays_t *delays)
{
    uint8_t armData[20] = { 0 };
    uint8_t extData[2] = { 0 };
    uint32_t offset = 0;

    ADI_PERFORM_VALIDATION(adi_adrv9001_Radio_ChannelEnablementDelays_Inspect_Validate, adrv9001, port, channel, delays);

    extData[0] = adi_adrv9001_Radio_MailboxChannel_Get(port, channel);

    /* Executing the GET command */
    extData[1] = ADRV9001_ARM_OBJECTID_TDD_TIMING_PARAMS;

    ADI_EXPECT(adi_adrv9001_arm_Cmd_Write, adrv9001, (uint8_t)ADRV9001_ARM_GET_OPCODE, &extData[0], sizeof(extData));

    /* Wait for command to finish executing */
    ADRV9001_ARM_CMD_STATUS_WAIT_EXPECT(adrv9001,
                                        (uint8_t)ADRV9001_ARM_GET_OPCODE,
                                        extData[1],
                                        (uint32_t)ADI_ADRV9001_DEFAULT_TIMEOUT_US,
                                        (uint32_t)ADI_ADRV9001_DEFAULT_INTERVAL_US);

    /* Read timing parameters from ARM mailbox */
    ADI_EXPECT(adi_adrv9001_arm_Memory_Read, adrv9001, ADRV9001_ADDR_ARM_MAILBOX_GET, &armData[0], sizeof(armData), ADRV9001_ARM_MEM_READ_AUTOINCR);

    /* Parse data to struct */
    adrv9001_ParseFourBytes(&offset, armData, &delays->riseToOnDelay);
    adrv9001_ParseFourBytes(&offset, armData, &delays->riseToAnalogOnDelay);
    adrv9001_ParseFourBytes(&offset, armData, &delays->fallToOffDelay);
    adrv9001_ParseFourBytes(&offset, armData, &delays->guardDelay);
    adrv9001_ParseFourBytes(&offset, armData, &delays->holdDelay);

    ADI_API_RETURN(adrv9001);
}
