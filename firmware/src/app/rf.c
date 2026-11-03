#include "rf.h"

#define  ALIGNED4(x)       ((x+3)/4*4)
__attribute__((__aligned__(4))) uint8_t RxBuf[264];

rfipTx_t gTxParam;
rfipRx_t gRxParam;
rfStatusCBs_t *pCbs;

__INTERRUPT
__HIGH_CODE
void LLE_IRQHandler( void )
{
    LLE_LibIRQHandler( );
}

__INTERRUPT
__HIGH_CODE
void BB_IRQHandler( void )
{
    BB_LibIRQHandler( );
}

__HIGH_CODE
void rf_tx_set_frequency( uint32_t f )
{
    gTxParam.frequency = f;
}

__HIGH_CODE
void rf_tx_set_sync_word( uint32_t sync_word )
{
    gTxParam.accessAddress = sync_word;
}

__HIGH_CODE
void rf_tx_set_phy_type( uint8_t phy )
{
    gTxParam.properties = (gTxParam.properties&~PHY_MODE_MASK)|(phy<<4);
}

__HIGH_CODE
void rf_tx_start( void *pBuf, uint16_t rfon_us )
{

    gTxParam.txDMA = (uint32_t)pBuf;
    gTxParam.waitTime = rfon_us*2;
    RFIP_StartTx( &gTxParam );
}

__HIGH_CODE
void rf_rx_set_sync_word( uint32_t sync_word )
{
    gRxParam.accessAddress = sync_word;
}

__HIGH_CODE
void rf_rx_set_frequency( uint32_t f )
{
    gRxParam.frequency = f;
}

__HIGH_CODE
void rf_rx_set_phy_type( uint8_t phy )
{
    gRxParam.properties = (gRxParam.properties&~PHY_MODE_MASK)|(phy<<4);
}

__HIGH_CODE
void rf_rx_start( uint32_t rx_us )
{

    gRxParam.timeOut = rx_us*2;
    RFIP_SetRx( &gRxParam );
}

__HIGH_CODE
void RF_ProcessCallBack( rfRole_States_t sta,uint8_t id  )
{
    if( sta&RF_STATE_RX )
    {
        if( pCbs && pCbs->pfnRxCB ) pCbs->pfnRxCB( (rfPackage_t *)RxBuf );
    }
    if( sta&RF_STATE_RX_CRCERR )
    {
        if(  pCbs && pCbs->pfnCrcErrCB ) pCbs->pfnCrcErrCB( );
    }
    if( sta&RF_STATE_TX_FINISH )
    {
        if(  pCbs && pCbs->pfnTxCB ) pCbs->pfnTxCB( );
    }
    if( sta&RF_STATE_TIMEOUT )
    {
        if(  pCbs && pCbs->pfnTimeoutCB )pCbs->pfnTimeoutCB( );
    }
}

void RFRole_RegisterStatusCbs( rfStatusCBs_t * p )
{
    pCbs = p;
}

void RFRole_Init(void)
{
    {
        rfRoleConfig_t conf ={0};
        conf.rfProcessCB = RF_ProcessCallBack;
        conf.processMask = RF_STATE_RX|RF_STATE_RX_CRCERR|RF_STATE_TX_FINISH|RF_STATE_TIMEOUT;
        RFRole_BasicInit( &conf );
    }
    TPROPERTIES_CFG Properties;
    {
        Properties.cfgVal = TEST_PHY_MODE;
#if(TEST_PHY_MODE == PHY_MODE_2G4 )
        Properties.whitOff = 1;
        Properties.lengthCrc = CRC_LEN;
        Properties.ctlFiled = CTL_FILED;
        Properties.lengthAA = AA_LEN;
        Properties.lengthPreamble = PRE_LEN;
        Properties.dplEnable = DPL_EN;
        Properties.mode2G4 = MODE_2G4;
        Properties.bitOrderData = DATA_ORDER;
        Properties.crcXOREnable = CRC_XOR_EN;
#endif
        PRINT("cfgVal=%x\n",Properties.cfgVal);
    }

    {
        gTxParam.accessAddress = AA;
        gTxParam.accessAddressEx = AA_EX;
        gTxParam.frequency = DEF_FREQUENCY;
        gTxParam.crcInit = CRC_INIT;
        gTxParam.crcPoly = CRC_POLY;
        gTxParam.properties = Properties.cfgVal;
        gTxParam.waitTime = 80*2;
        gTxParam.txPowerVal = LL_TX_PWR_0_DBM;
    }

    {
        gRxParam.accessAddress = AA;
        gRxParam.accessAddressEx = AA_EX;
        gRxParam.frequency = DEF_FREQUENCY;
        gRxParam.crcInit = CRC_INIT;
        gRxParam.crcPoly = CRC_POLY;
        gRxParam.properties = Properties.cfgVal;
        gRxParam.rxDMA = (uint32_t)RxBuf;
        gRxParam.rxMaxLen = DATA_LEN_MAX_RX;
    }
    pCbs = NULL;
    PFIC_EnableIRQ( BLEB_IRQn );
    PFIC_EnableIRQ( BLEL_IRQn );
}
