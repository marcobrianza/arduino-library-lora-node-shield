#define LORA
#include "LoraShield.h"
#include "SPI.h"
#include "Wire.h"
#include <RTCInt.h>
//#define LORA
LoRaMacPrimitives_t LoRaMacPrimitives;
LoRaMacCallback_t LoRaMacCallbacks;
MibRequestConfirm_t mibReq;

#define LORAWAN_ADR_ON                              1
#define LORAWAN_DUTYCYCLE_ON                        true
#define LORAWAN_PUBLIC_NETWORK                      true

/*!
 * Default datarate
 */
#define LORAWAN_DEFAULT_DATARATE                    DR_0

/*!
 * Device address on the network (big endian)
 *
 * \remark In this application the value is automatically generated using
 *         a pseudo random generator seeded with a value derived from
 *         BoardUniqueId value if LORAWAN_DEVICE_ADDRESS is set to 0
 */
#define LORAWAN_DEVICE_ADDRESS                      ( uint32_t )0xF6897515


/*!
 * User application data buffer size
 */
#define LORAWAN_APP_DATA_MAX_SIZE                           64

/*!
 * IEEE Organizationally Unique Identifier ( OUI ) (big endian)
 */
#define IEEE_OUI                                    0x11, 0x22, 0x33

/*!
 * Mote device IEEE EUI (big endian)
 *
 * \remark In this application the value is automatically generated by calling
 *         BoardGetUniqueId function
 */
#define LORAWAN_DEVICE_EUI                          { 0x00, 0x00, 0x00, 0x00, 0xF6, 0x89, 0x75, 0x15 }
/*!
 * Application IEEE EUI (big endian)
 */
#define LORAWAN_APPLICATION_EUI                    { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x00, 0x19, 0x83 } // { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x00, 0x19, 0x83 }

/*!
 * AES encryption/decryption cipher application key
 */              
#define LORAWAN_APPLICATION_KEY                     { 0x61, 0x17, 0x24, 0x96, 0x18, 0x13, 0x18, 0x0A, 0x54, 0x84, 0xCC, 0x33, 0xA3, 0x3F, 0x22, 0x7B }

static uint8_t DevEui[] = LORAWAN_DEVICE_EUI;
static uint8_t AppEui[] = LORAWAN_APPLICATION_EUI;
static uint8_t AppKey[] = LORAWAN_APPLICATION_KEY;

/*!
 * User application data
 */
static uint8_t AppData[LORAWAN_APP_DATA_MAX_SIZE];

/*!
 * Defines the application data transmission duty cycle
 */
static uint32_t TxDutyCycleTime;

/*!
 * Specifies the state of the application LED
 */
static bool AppLedStateOn = false;

/*!
 * Defines a random delay for application data transmission duty cycle. 1s,
 * value in [ms].
 */
#define APP_TX_DUTYCYCLE_RND                        1000

/*!
 * Defines the application data transmission duty cycle. 5s, value in [ms].
 */
#define APP_TX_DUTYCYCLE                            5000

/*!
 * When set to 1 the application uses the Over-the-Air activation procedure
 * When set to 0 the application uses the Personalization activation procedure
 */
#define OVER_THE_AIR_ACTIVATION                     0

/*!
 * LoRaWAN confirmed messages
 */
#define LORAWAN_CONFIRMED_MSG_ON                    true//false

/*!
 * User application data buffer size
 */
#if defined( USE_BAND_868 )

#define LORAWAN_APP_DATA_SIZE                       16

#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )

#define LORAWAN_APP_DATA_SIZE                       11

#endif

/*!
 * User application data size
 */
static uint8_t AppDataSize = LORAWAN_APP_DATA_SIZE;


/*!
 * AES encryption/decryption cipher network session key
 */
#define LORAWAN_NWKSKEY                             { 0xF6, 0xF3, 0xCF, 0x90, 0xCE, 0x9E, 0x73, 0x11, 0x4E, 0x2B, 0xD3, 0x1C, 0x67, 0x03, 0xD2, 0xA5 }
/*!
 * Indicates if the node is sending confirmed or unconfirmed messages
 */
static uint8_t IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;
/*!
 * AES encryption/decryption cipher application session key
 */
#define LORAWAN_APPSKEY                             { 0xCE, 0x4D, 0xB3, 0xC8, 0x97, 0xD3, 0xA0, 0xA1, 0x1D, 0x10, 0x18, 0x11, 0x67, 0xE4, 0xCE, 0xF8 }

#if( OVER_THE_AIR_ACTIVATION == 0 )

static uint8_t NwkSKey[] = LORAWAN_NWKSKEY;
static uint8_t AppSKey[] = LORAWAN_APPSKEY;
/*!
 * Device address
 */
static uint32_t DevAddr = LORAWAN_DEVICE_ADDRESS;

#endif
/*!
 * LoRaWAN application port
 */
#define LORAWAN_APP_PORT                            2

/*!
 * Application port
 */
static uint8_t AppPort = LORAWAN_APP_PORT;

/*!
 * Current network ID
 */
#define LORAWAN_NETWORK_ID                          ( uint32_t )0


/*!
 * Timer to handle the application data transmission duty cycle
 */
static TimerEvent_t TxNextPacketTimer;
/*!
 * Timer to handle the state of LED1
 */
static TimerEvent_t Led1Timer;
/*!
 * Timer to handle the state of LED1
 */
static TimerEvent_t Led2Timer;
/*!
 * Indicates if a new packet can be sent
 */
static bool NextTx = true;

volatile uint32_t tempo;
volatile bool stato;

void setup() {
  // put your setup code here, to run once:
Serial.begin(9600);
delay(3000);
    BoardInitMcu( );
    BoardInitPeriph( );

    DeviceState = DEVICE_STATE_INIT;
    Serial.println("setup done");

}

void loop() {
         switch( DeviceState )
        {
            case DEVICE_STATE_INIT:
            {
              Serial.println("state device init");
                LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
                LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
                LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
                LoRaMacCallbacks.GetBatteryLevel = BoardGetBatteryLevel;
                LoRaMacInitialization( &LoRaMacPrimitives, &LoRaMacCallbacks );

               TimerInit( &TxNextPacketTimer, OnTxNextPacketTimerEvent );
 
                TimerInit( &Led1Timer, OnLed1TimerEvent );
           //     TimerSetValue( &Led1Timer, 1000 );
           

          //      TimerInit( &Led2Timer, OnLed2TimerEvent );
          //      TimerSetValue( &Led2Timer, 25 );

                mibReq.Type = MIB_ADR;
                mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_PUBLIC_NETWORK;
                mibReq.Param.EnablePublicNetwork = LORAWAN_PUBLIC_NETWORK;
                LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( USE_BAND_868 )
                LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );

#if( USE_SEMTECH_DEFAULT_CHANNEL_LINEUP == 1 ) 
                LoRaMacChannelAdd( 3, ( ChannelParams_t )LC4 );
                LoRaMacChannelAdd( 4, ( ChannelParams_t )LC5 );
                LoRaMacChannelAdd( 5, ( ChannelParams_t )LC6 );
                LoRaMacChannelAdd( 6, ( ChannelParams_t )LC7 );
                LoRaMacChannelAdd( 7, ( ChannelParams_t )LC8 );
                LoRaMacChannelAdd( 8, ( ChannelParams_t )LC9 );
                LoRaMacChannelAdd( 9, ( ChannelParams_t )LC10 );
#endif

#endif
                DeviceState = DEVICE_STATE_JOIN;
              Serial.println("state device init end");
                break;
            }
            case DEVICE_STATE_JOIN:
            {
              Serial.println("state device join");
#if( OVER_THE_AIR_ACTIVATION != 0 )
                MlmeReq_t mlmeReq;

                // Initialize LoRaMac device unique ID
                BoardGetUniqueId( DevEui );

                mlmeReq.Type = MLME_JOIN;

                mlmeReq.Req.Join.DevEui = DevEui;
                mlmeReq.Req.Join.AppEui = AppEui;
                mlmeReq.Req.Join.AppKey = AppKey;

                if( NextTx == true )
                {
                    LoRaMacMlmeRequest( &mlmeReq );
                }
                DeviceState = DEVICE_STATE_SLEEP;
#else
                // Choose a random device address if not already defined in Comissioning.h
                if( DevAddr == 0 )
                {
                    // Random seed initialization
                    srand1( BoardGetRandomSeed( ) );

                    // Choose a random device address
                    DevAddr = randr( 0, 0x01FFFFFF );
                }

                mibReq.Type = MIB_NET_ID;
                mibReq.Param.NetID = LORAWAN_NETWORK_ID;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_DEV_ADDR;
                mibReq.Param.DevAddr = DevAddr;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NWK_SKEY;
                mibReq.Param.NwkSKey = NwkSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_APP_SKEY;
                mibReq.Param.AppSKey = AppSKey;
                LoRaMacMibSetRequestConfirm( &mibReq );

                mibReq.Type = MIB_NETWORK_JOINED;
                mibReq.Param.IsNetworkJoined = true;
                LoRaMacMibSetRequestConfirm( &mibReq );

                DeviceState = DEVICE_STATE_SEND;
#endif

              Serial.println("state device join end");
                break;
            }
            case DEVICE_STATE_SEND:
            {
  //            Serial.println("state device send");
                if( NextTx == true )
                {
                    PrepareTxFrame( AppPort );

                    NextTx = SendFrame( );
                }
                if( ComplianceTest.Running == true )
                {
                    // Schedule next packet transmission
                    TxDutyCycleTime = 5000;// ms
                }
                else
                {
                    // Schedule next packet transmission
                    TxDutyCycleTime = APP_TX_DUTYCYCLE + randr( -APP_TX_DUTYCYCLE_RND, APP_TX_DUTYCYCLE_RND );
                }
                DeviceState = DEVICE_STATE_CYCLE;
    //          Serial.println("state device send end");
                break;
            }
            case DEVICE_STATE_CYCLE:
            {
    //          Serial.println("state device cycle");
                DeviceState = DEVICE_STATE_SLEEP;

                // Schedule next packet transmission
                TimerSetValue( &TxNextPacketTimer, TxDutyCycleTime );
                TimerStart( &TxNextPacketTimer );
                              
      //        Serial.println("state device cycle end");
                break;
            }
            case DEVICE_STATE_SLEEP:
            {
   //           Serial.println("state device sleep");
                // Wake up through events
               // TimerLowPowerHandler( );
 //               TimerSetValue( &Led1Timer, 1000 );
 //               TimerStart( &Led1Timer);
//                delay(2000);
 
 //             Serial.println("state device sleep end");
                break;
            }
            default:
            {
              
              Serial.println("default");
                DeviceState = DEVICE_STATE_INIT;
                break;
            }
        }
 }




/*!
 * \brief   MCPS-Confirm event function
 *
 * \param   [IN] mcpsConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void McpsConfirm( McpsConfirm_t *mcpsConfirm )
{Serial.print("McpsConfirm = ");
Serial.println(mcpsConfirm->Status);
    if( mcpsConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        switch( mcpsConfirm->McpsRequest )
        {
            case MCPS_UNCONFIRMED:
            {
              Serial.println("EVENT UNCONFIRMED RECEIVED");
                // Check Datarate
                // Check TxPower
                break;
            }
            case MCPS_CONFIRMED:
            {
              
              Serial.println("EVENT CONFIRMED RECEIVED");
                // Check Datarate
                // Check TxPower
                // Check AckReceived
                // Check NbTrials
                break;
            }
            case MCPS_PROPRIETARY:
            {
             Serial.println("proprietaty RECEIVED");
             break;
            }
            default:
                  Serial.println("default XXX");
                break;
        }

        // Switch LED 1 ON
 //       GpioWrite( &Led1, 0 );
 //digitalWrite(13, HIGH);
  //      TimerStart( &Led1Timer );
    }
    else
      Serial.println("status not ok");
    NextTx = true;
}


/*!
 * \brief   MCPS-Indication event function
 *
 * \param   [IN] mcpsIndication - Pointer to the indication structure,
 *               containing indication attributes.
 */
static void McpsIndication( McpsIndication_t *mcpsIndication )
{
  Serial.println("MCPS Indication");
    if( mcpsIndication->Status != LORAMAC_EVENT_INFO_STATUS_OK )
    {
        return;
    }

    switch( mcpsIndication->McpsIndication )
    {
        case MCPS_UNCONFIRMED:
        {
            break;
        }
        case MCPS_CONFIRMED:
        {
            break;
        }
        case MCPS_PROPRIETARY:
        {
            break;
        }
        case MCPS_MULTICAST:
        {
            break;
        }
        default:
            break;
    }

    // Check Multicast
    // Check Port
    // Check Datarate
    // Check FramePending
    // Check Buffer
    // Check BufferSize
    // Check Rssi
    // Check Snr
    // Check RxSlot
  Serial.println("RECEIVED SOMETHING!!!");
    if( ComplianceTest.Running == true )
    {
        ComplianceTest.DownLinkCounter++;
    }

    if( mcpsIndication->RxData == true )
    {
        switch( mcpsIndication->Port )
        {
        case 1: // The application LED can be controlled on port 1 or 2
        case 2:
            if( mcpsIndication->BufferSize == 1 )
            {
                AppLedStateOn = mcpsIndication->Buffer[0] & 0x01;
             //   GpioWrite( &Led3, ( ( AppLedStateOn & 0x01 ) != 0 ) ? 0 : 1 );
            }
            break;
        case 224:
            if( ComplianceTest.Running == false )
            {
                // Check compliance test enable command (i)
                if( ( mcpsIndication->BufferSize == 4 ) &&
                    ( mcpsIndication->Buffer[0] == 0x01 ) &&
                    ( mcpsIndication->Buffer[1] == 0x01 ) &&
                    ( mcpsIndication->Buffer[2] == 0x01 ) &&
                    ( mcpsIndication->Buffer[3] == 0x01 ) )
                {
                    IsTxConfirmed = true;//false;
                    AppPort = 224;
                    AppDataSize = 2;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.LinkCheck = false;
                    ComplianceTest.DemodMargin = 0;
                    ComplianceTest.NbGateways = 0;
                    ComplianceTest.Running = true;
                    ComplianceTest.State = 1;
                    
                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = true;
                    LoRaMacMibSetRequestConfirm( &mibReq );

#if defined( USE_BAND_868 )
                    LoRaMacTestSetDutyCycleOn( false );
#endif
           //         GpsStop( );
                }
            }
            else
            {
                ComplianceTest.State = mcpsIndication->Buffer[0];
                switch( ComplianceTest.State )
                {
                case 0: // Check compliance test disable command (ii)
                    IsTxConfirmed = LORAWAN_CONFIRMED_MSG_ON;
                    AppPort = LORAWAN_APP_PORT;
                    AppDataSize = LORAWAN_APP_DATA_SIZE;
                    ComplianceTest.DownLinkCounter = 0;
                    ComplianceTest.Running = true;//false;
                    
                    MibRequestConfirm_t mibReq;
                    mibReq.Type = MIB_ADR;
                    mibReq.Param.AdrEnable = LORAWAN_ADR_ON;
                    LoRaMacMibSetRequestConfirm( &mibReq );
#if defined( USE_BAND_868 )
                    LoRaMacTestSetDutyCycleOn( LORAWAN_DUTYCYCLE_ON );
#endif
                //    GpsStart( );
                    break;
                case 1: // (iii, iv)
                    AppDataSize = 2;
                    break;
                case 2: // Enable confirmed messages (v)
                    IsTxConfirmed = true;
                    ComplianceTest.State = 1;
                    break;
                case 3:  // Disable confirmed messages (vi)
                    IsTxConfirmed = false;
                    ComplianceTest.State = 1;
                    break;
                case 4: // (vii)
                    AppDataSize = mcpsIndication->BufferSize;

                    AppData[0] = 4;
                    for( uint8_t i = 1; i < AppDataSize; i++ )
                    {
                        AppData[i] = mcpsIndication->Buffer[i] + 1;
                    }
                    break;
                case 5: // (viii)
                    {
                        MlmeReq_t mlmeReq;
                        mlmeReq.Type = MLME_LINK_CHECK;
                        LoRaMacMlmeRequest( &mlmeReq );
                    }
                    break;
                case 6: // (ix)
                    {
                        MlmeReq_t mlmeReq;

                        mlmeReq.Type = MLME_JOIN;

                        mlmeReq.Req.Join.DevEui = DevEui;
                        mlmeReq.Req.Join.AppEui = AppEui;
                        mlmeReq.Req.Join.AppKey = AppKey;

                        LoRaMacMlmeRequest( &mlmeReq );
                        DeviceState = DEVICE_STATE_SLEEP;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

    // Switch LED 2 ON for each received downlink
  //  GpioWrite( &Led2, 0 );
 // digitalWrite(38, HIGH);
 //   TimerStart( &Led2Timer );
}



/*!
 * \brief   MLME-Confirm event function
 *
 * \param   [IN] mlmeConfirm - Pointer to the confirm structure,
 *               containing confirm attributes.
 */
static void MlmeConfirm( MlmeConfirm_t *mlmeConfirm )
{
    if( mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK )
    {
        switch( mlmeConfirm->MlmeRequest )
        {
            case MLME_JOIN:
            {
                // Status is OK, node has joined the network
                DeviceState = DEVICE_STATE_SEND;
                NextTx = true;
                break;
            }
            case MLME_LINK_CHECK:
            {
                // Check DemodMargin
                // Check NbGateways
                if( ComplianceTest.Running == true )
                {
                    ComplianceTest.LinkCheck = true;
                    ComplianceTest.DemodMargin = mlmeConfirm->DemodMargin;
                    ComplianceTest.NbGateways = mlmeConfirm->NbGateways;
                }
                break;
            }
            default:
                break;
        }
    }
    NextTx = true;
}


/*!
 * \brief Function executed on Led 1 Timeout event
 */
static void OnLed1TimerEvent( void )
{
    TimerStop( &Led1Timer );
    // Switch LED 1 OFF
    Serial.println("Timer 1 EVENT");
 //   GpioWrite( &Led1, 1 );
}

/*!
 * \brief Function executed on Led 2 Timeout event
 */
static void OnLed2TimerEvent( void )
{
    TimerStop( &Led2Timer );
    // Switch LED 2 OFF
  //  GpioWrite( &Led2, 1 );
}

/*!
 * \brief Function executed on TxNextPacket Timeout event
 */
static void OnTxNextPacketTimerEvent( void )
{
    MibRequestConfirm_t mibReq;
    LoRaMacStatus_t status;
    TimerStop( &TxNextPacketTimer );

    mibReq.Type = MIB_NETWORK_JOINED;
    status = LoRaMacMibGetRequestConfirm( &mibReq );

    if( status == LORAMAC_STATUS_OK )
    {
        if( mibReq.Param.IsNetworkJoined == true )
        {
            DeviceState = DEVICE_STATE_SEND;
            NextTx = true;
        }
        else
        {
            DeviceState = DEVICE_STATE_JOIN;
        }
    }
}


/*!
 * \brief   Prepares the payload of the frame
 */
static void PrepareTxFrame( uint8_t port )
{
    switch( port )
    {//LoRaMacDownlinkStatus.DownlinkCounter
    case 2:
        {
          Serial.print("downlink counter = ");
          Serial.println(ComplianceTest.DownLinkCounter);
#if defined( USE_BAND_868 )
            uint16_t pressure = 0;
            int16_t altitudeBar = 0;
            int16_t temperature = 0;
            int32_t latitude, longitude = 0;
            int16_t altitudeGps = 0xFFFF;
            uint8_t batteryLevel = 0;

 //           pressure = ( uint16_t )( MPL3115ReadPressure( ) / 10 );             // in hPa / 10
 //           temperature = ( int16_t )( MPL3115ReadTemperature( ) * 100 );       // in °C * 100
 //           altitudeBar = ( int16_t )( MPL3115ReadAltitude( ) * 10 );           // in m * 10
 //           batteryLevel = BoardGetBatteryLevel( );                             // 1 (very low) to 254 (fully charged)
 //           GpsGetLatestGpsPositionBinary( &latitude, &longitude );
 //           altitudeGps = GpsGetLatestGpsAltitude( );                           // in m

            AppData[0] = 0x4A;//AppLedStateOn;
            AppData[1] = 0x11;//temperature;                                           // Signed degrees Celcius in half degree units. So,  +/-63 C
            AppData[2] = 0x22;//batteryLevel;                                          // Per LoRaWAN spec; 0=Charging; 1...254 = level, 255 = N/A
            AppData[3] = 0x33;//( latitude >> 16 ) & 0xFF;
            AppData[4] = 0x44;//( latitude >> 8 ) & 0xFF;
            AppData[5] = 0x66;//latitude & 0xFF;
            AppData[6] = 0x77;//( longitude >> 16 ) & 0xFF;
            AppData[7] = 0x88;//( longitude >> 8 ) & 0xFF;
            AppData[8] = 0x99;//longitude & 0xFF;
            AppData[9] = 0xAA;//( altitudeGps >> 8 ) & 0xFF;
            AppData[10] = 0xBB;//altitudeGps & 0xFF;
            AppData[11] = 0xCC;//( longitude >> 16 ) & 0xFF;
            AppData[12] = 0xDD;//( longitude >> 8 ) & 0xFF;
            AppData[13] = 0x12;//longitude & 0xFF;
            AppData[14] = 0x23;//( altitudeGps >> 8 ) & 0xFF;
            AppData[15] = 0x34;//altitudeGps & 0xFF;
#elif defined( USE_BAND_915 ) || defined( USE_BAND_915_HYBRID )
            int16_t temperature = 0;
            int32_t latitude, longitude = 0;
            uint16_t altitudeGps = 0xFFFF;
            uint8_t batteryLevel = 0;
        
  //          temperature = ( int16_t )( MPL3115ReadTemperature( ) * 100 );       // in °C * 100
        
   //         batteryLevel = BoardGetBatteryLevel( );                             // 1 (very low) to 254 (fully charged)
   //         GpsGetLatestGpsPositionBinary( &latitude, &longitude );
   //         altitudeGps = GpsGetLatestGpsAltitude( );                           // in m
        
            AppData[0] = 0x55;//AppLedStateOn;
            AppData[1] = 0x11;//temperature;                                           // Signed degrees Celcius in half degree units. So,  +/-63 C
            AppData[2] = 0x22;//batteryLevel;                                          // Per LoRaWAN spec; 0=Charging; 1...254 = level, 255 = N/A
            AppData[3] = 0x33;//( latitude >> 16 ) & 0xFF;
            AppData[4] = 0x44;//( latitude >> 8 ) & 0xFF;
            AppData[5] = 0x66;//latitude & 0xFF;
            AppData[6] = 0x77;//( longitude >> 16 ) & 0xFF;
            AppData[7] = 0x88;//( longitude >> 8 ) & 0xFF;
            AppData[8] = 0x99;//longitude & 0xFF;
            AppData[9] = 0xAA;//( altitudeGps >> 8 ) & 0xFF;
            AppData[10] = 0xBB;//altitudeGps & 0xFF;
#endif
        }
        break;
    case 224:
        if( ComplianceTest.LinkCheck == true )
        {
            ComplianceTest.LinkCheck = false;
            AppDataSize = 3;
            AppData[0] = 5;
            AppData[1] = ComplianceTest.DemodMargin;
            AppData[2] = ComplianceTest.NbGateways;
            ComplianceTest.State = 1;
        }
        else
        {
            switch( ComplianceTest.State )
            {
            case 4:
                ComplianceTest.State = 1;
                break;
            case 1:
                AppDataSize = 2;
                AppData[0] = ComplianceTest.DownLinkCounter >> 8;
                AppData[1] = ComplianceTest.DownLinkCounter;
                break;
            }
        }
        break;
    default:
        break;
    }
}

/*!
 * \brief   Prepares the payload of the frame
 *
 * \retval  [0: frame could be send, 1: error]
 */
static bool SendFrame( void )
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;
    
    if( LoRaMacQueryTxPossible( AppDataSize, &txInfo ) != LORAMAC_STATUS_OK )
    {
      Serial.println("empty frame sent");
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
    }
    else
    {
        if( IsTxConfirmed == false )
        {
          Serial.println("data pck unconfirmed prepared");
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = AppPort;
            mcpsReq.Req.Unconfirmed.fBuffer = AppData;
            mcpsReq.Req.Unconfirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Unconfirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
        else
        {
          Serial.println("data pck confirmed prepared");
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = AppPort;
            mcpsReq.Req.Confirmed.fBuffer = AppData;
            mcpsReq.Req.Confirmed.fBufferSize = AppDataSize;
            mcpsReq.Req.Confirmed.NbTrials = 8;
            mcpsReq.Req.Confirmed.Datarate = LORAWAN_DEFAULT_DATARATE;
        }
    }
    int res = LoRaMacMcpsRequest( &mcpsReq );
    if( res/* = LoRaMacMcpsRequest( &mcpsReq )*/ == LORAMAC_STATUS_OK )
    {
      Serial.println("send OK");
        return false;
    }
      Serial.print("send ERROR = ");
      Serial.println(res);
    return true;
}