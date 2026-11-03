#include "rf.hpp"

static Rf s_rf;

Rf &Rf::get()
{
    return s_rf;
}

extern "C"
{
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
}

__HIGH_CODE
void Rf::tx_set_frequency(uint32_t frequency)
{
    m_tx.frequency = frequency;
}

__HIGH_CODE
void Rf::tx_set_sync_word(uint32_t sync_word)
{
    m_tx.accessAddress = sync_word;
}

__HIGH_CODE
void Rf::tx_set_phy_type(uint8_t phy)
{
    m_tx.properties = (m_tx.properties & ~PHY_MODE_MASK) | (phy << 4);
}

__HIGH_CODE
void Rf::tx_start(void *buf, uint16_t rfon_us)
{
    m_tx.txDMA = (uint32_t)buf;
    m_tx.waitTime = rfon_us * 2;
    RFIP_StartTx(&m_tx);
}

__HIGH_CODE
void Rf::rx_set_sync_word(uint32_t sync_word)
{
    m_rx.accessAddress = sync_word;
}

__HIGH_CODE
void Rf::rx_set_frequency(uint32_t frequency)
{
    m_rx.frequency = frequency;
}

__HIGH_CODE
void Rf::rx_set_phy_type(uint8_t phy)
{
    m_rx.properties = (m_rx.properties & ~PHY_MODE_MASK) | (phy << 4);
}

__HIGH_CODE
void Rf::rx_start(uint32_t rx_us)
{
    m_rx.timeOut = rx_us * 2;
    RFIP_SetRx(&m_rx);
}

__HIGH_CODE
void Rf::process_callback(rfRole_States_t sta, uint8_t id)
{
    Rf &self = get();

    (void)id;

    if (sta & RF_STATE_RX)
    {
        if (self.m_cbs && self.m_cbs->rx_cb)
        {
            self.m_cbs->rx_cb((rf_packet_t *)self.m_rx_buf);
        }
    }
    if (sta & RF_STATE_RX_CRCERR)
    {
        if (self.m_cbs && self.m_cbs->crc_err_cb)
        {
            self.m_cbs->crc_err_cb();
        }
    }
    if (sta & RF_STATE_TX_FINISH)
    {
        if (self.m_cbs && self.m_cbs->tx_cb)
        {
            self.m_cbs->tx_cb();
        }
    }
    if (sta & RF_STATE_TIMEOUT)
    {
        if (self.m_cbs && self.m_cbs->timeout_cb)
        {
            self.m_cbs->timeout_cb();
        }
    }
}

void Rf::set_status_cbs(rf_status_cbs_t *cbs)
{
    m_cbs = cbs;
}

void Rf::init(void)
{
    rfRoleConfig_t conf = {0};

    conf.rfProcessCB = process_callback;
    conf.processMask = RF_STATE_RX | RF_STATE_RX_CRCERR |
                       RF_STATE_TX_FINISH | RF_STATE_TIMEOUT;
    RFRole_BasicInit(&conf);

    m_tx.accessAddress = AA;
    m_tx.accessAddressEx = AA_EX;
    m_tx.frequency = DEF_FREQUENCY;
    m_tx.crcInit = CRC_INIT;
    m_tx.crcPoly = CRC_POLY;
    m_tx.properties = TEST_PHY_MODE;
    m_tx.waitTime = 80 * 2;
    m_tx.txPowerVal = LL_TX_PWR_0_DBM;

    m_rx.accessAddress = AA;
    m_rx.accessAddressEx = AA_EX;
    m_rx.frequency = DEF_FREQUENCY;
    m_rx.crcInit = CRC_INIT;
    m_rx.crcPoly = CRC_POLY;
    m_rx.properties = TEST_PHY_MODE;
    m_rx.rxDMA = (uint32_t)m_rx_buf;
    m_rx.rxMaxLen = DATA_LEN_MAX_RX;

    m_cbs = nullptr;
    PFIC_EnableIRQ(BLEB_IRQn);
    PFIC_EnableIRQ(BLEL_IRQn);
}
