#include "rf.h"

__attribute__((__aligned__(4))) static uint8_t g_rx_buf[264];

static rfipTx_t g_tx_param;
static rfipRx_t g_rx_param;
static rf_status_cbs_t *g_cbs;

__INTERRUPT
__HIGH_CODE
void LLE_IRQHandler(void)
{
    LLE_LibIRQHandler();
}

__INTERRUPT
__HIGH_CODE
void BB_IRQHandler(void)
{
    BB_LibIRQHandler();
}

__HIGH_CODE
void rf_tx_set_frequency(uint32_t frequency)
{
    g_tx_param.frequency = frequency;
}

__HIGH_CODE
void rf_tx_set_sync_word(uint32_t sync_word)
{
    g_tx_param.accessAddress = sync_word;
}

__HIGH_CODE
void rf_tx_set_phy_type(uint8_t phy)
{
    g_tx_param.properties = (g_tx_param.properties & ~PHY_MODE_MASK) | (phy << 4);
}

__HIGH_CODE
void rf_tx_start(void *buf, uint16_t rfon_us)
{
    g_tx_param.txDMA = (uint32_t)buf;
    g_tx_param.waitTime = rfon_us * 2;
    RFIP_StartTx(&g_tx_param);
}

__HIGH_CODE
void rf_rx_set_sync_word(uint32_t sync_word)
{
    g_rx_param.accessAddress = sync_word;
}

__HIGH_CODE
void rf_rx_set_frequency(uint32_t frequency)
{
    g_rx_param.frequency = frequency;
}

__HIGH_CODE
void rf_rx_set_phy_type(uint8_t phy)
{
    g_rx_param.properties = (g_rx_param.properties & ~PHY_MODE_MASK) | (phy << 4);
}

__HIGH_CODE
void rf_rx_start(uint32_t rx_us)
{
    g_rx_param.timeOut = rx_us * 2;
    RFIP_SetRx(&g_rx_param);
}

__HIGH_CODE
static void rf_process_callback(rfRole_States_t sta, uint8_t id)
{
    (void)id;

    if (sta & RF_STATE_RX)
    {
        if (g_cbs && g_cbs->rx_cb)
        {
            g_cbs->rx_cb((rf_packet_t *)g_rx_buf);
        }
    }
    if (sta & RF_STATE_RX_CRCERR)
    {
        if (g_cbs && g_cbs->crc_err_cb)
        {
            g_cbs->crc_err_cb();
        }
    }
    if (sta & RF_STATE_TX_FINISH)
    {
        if (g_cbs && g_cbs->tx_cb)
        {
            g_cbs->tx_cb();
        }
    }
    if (sta & RF_STATE_TIMEOUT)
    {
        if (g_cbs && g_cbs->timeout_cb)
        {
            g_cbs->timeout_cb();
        }
    }
}

void rf_role_register_status_cbs(rf_status_cbs_t *cbs)
{
    g_cbs = cbs;
}

void rf_role_init(void)
{
    rfRoleConfig_t conf = {0};

    conf.rfProcessCB = rf_process_callback;
    conf.processMask = RF_STATE_RX | RF_STATE_RX_CRCERR |
                       RF_STATE_TX_FINISH | RF_STATE_TIMEOUT;
    RFRole_BasicInit(&conf);

    TPROPERTIES_CFG properties;
    properties.cfgVal = TEST_PHY_MODE;
#if (TEST_PHY_MODE == PHY_MODE_2G4)
    properties.whitOff = 1;
    properties.lengthCrc = CRC_LEN;
    properties.ctlFiled = CTL_FILED;
    properties.lengthAA = AA_LEN;
    properties.lengthPreamble = PRE_LEN;
    properties.dplEnable = DPL_EN;
    properties.mode2G4 = MODE_2G4;
    properties.bitOrderData = DATA_ORDER;
    properties.crcXOREnable = CRC_XOR_EN;
#endif

    g_tx_param.accessAddress = AA;
    g_tx_param.accessAddressEx = AA_EX;
    g_tx_param.frequency = DEF_FREQUENCY;
    g_tx_param.crcInit = CRC_INIT;
    g_tx_param.crcPoly = CRC_POLY;
    g_tx_param.properties = properties.cfgVal;
    g_tx_param.waitTime = 80 * 2;
    g_tx_param.txPowerVal = LL_TX_PWR_0_DBM;

    g_rx_param.accessAddress = AA;
    g_rx_param.accessAddressEx = AA_EX;
    g_rx_param.frequency = DEF_FREQUENCY;
    g_rx_param.crcInit = CRC_INIT;
    g_rx_param.crcPoly = CRC_POLY;
    g_rx_param.properties = properties.cfgVal;
    g_rx_param.rxDMA = (uint32_t)g_rx_buf;
    g_rx_param.rxMaxLen = DATA_LEN_MAX_RX;

    g_cbs = NULL;
    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);
}
