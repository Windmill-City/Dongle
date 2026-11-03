#include "rf_uart_rx.hpp"
#include "usb_uart.hpp"

static RfUart s_rf_uart;

RfUart &RfUart::get()
{
    return s_rf_uart;
}

__HIGH_CODE
static uint32_t rf_rand16(uint32_t seed)
{
    static uint32_t holdrand;
    uint64_t tmp;

    holdrand += seed;
    holdrand = holdrand * 1664525 + 1013904223;
    tmp = holdrand;
    tmp = (tmp * 0xFFFF) >> 32;
    return tmp;
}

__HIGH_CODE
static uint32_t rf_rand_aa(uint16_t rand)
{
    uint32_t aa = rand;

    aa = (aa << 8) | 0x6E0000B6;
    return aa;
}

__HIGH_CODE
void RfUart::on_rx(rf_packet_t *packet)
{
    get().process_rx(packet);
}

__HIGH_CODE
void RfUart::on_tx(void)
{
    get().process_tx();
}

__HIGH_CODE
void RfUart::on_crc_error(void)
{
    get().process_crc_error();
}

__HIGH_CODE
void RfUart::on_timeout(void)
{
    get().process_timeout();
}

void RfUart::disconnect(void)
{
    Rf &rf = Rf::get();

    RFRole_Shut();
    m_bound_status = BOUND_STATUS_IDLE;
    m_rx_data_status = DATA_STATUS_START;
    TMR_ITCfg(DISABLE, TMR_IT_CYC_END);

    rf.tx_set_sync_word(AA);
    rf.tx_set_frequency(DEF_FREQUENCY);
    rf.rx_set_sync_word(AA);
    rf.rx_set_frequency(DEF_FREQUENCY);
    m_rf_status = RF_STATUS_WAIT;
}

void RfUart::bound(bound_rsp_t *rsp)
{
    Rf &rf = Rf::get();

    m_timeout = 0;
    m_bound_status = BOUND_STATUS_WAIT;
    rf.tx_set_sync_word(rsp->access_addr);
    rf.tx_set_frequency(rsp->channel);
    rf.tx_set_phy_type(rsp->phy);

    rf.rx_set_sync_word(rsp->access_addr);
    rf.rx_set_frequency(rsp->channel);
    rf.rx_set_phy_type(rsp->phy);
}

__HIGH_CODE
void RfUart::process_rx(rf_packet_t *packet)
{
    Rf &rf = Rf::get();
    rf_packet_t *tx_packet = &m_tx_buf.packet;

    if (m_bound_status == BOUND_STATUS_IDLE)
    {
        if (packet->type == PKT_CMD_BOUND_REQ)
        {
            bool reg = false;
            bound_req_t *req = (bound_req_t *)(packet + 1);

            if (packet->length == sizeof(bound_req_t) + 2)
            {
                int8_t rssi = *(uint8_t *)((uint8_t *)packet + packet->length + 2 + 2);

                if (req->server_data)
                {
                    if (!m_server_data || req->server_data == m_server_data)
                    {
                        reg = true;
                    }
                }
                else
                {
                    if (rssi > -35)
                    {
                        reg = true;
                    }
                }

                if (reg)
                {
                    bound_rsp_t *rsp = (bound_rsp_t *)&m_tx_buf.rsp;

                    m_rf_status = RF_STATUS_TXRSP;

                    m_data_seq = 0;
                    m_server_data = rf_rand16(rssi);
                    tx_packet->type = PKT_CMD_BOUND_RSP;
                    tx_packet->length = PKT_DATA_OFFSET + sizeof(bound_rsp_t);
                    tx_packet->seq = m_data_seq;
                    tx_packet->resv = 0;
                    rsp->access_addr = rf_rand_aa(m_server_data);
                    rsp->channel = m_server_data & 0x3F;
                    rsp->phy = CONN_PHY_TYPE;
                    rsp->server_data = m_server_data;
                    rsp->interval = CONN_INTERVAL;
                    rsp->timeout = CONN_TIMEOUT;
                    rf.tx_start(tx_packet, 20);
                    m_data_seq++;
                    m_interval_timer = req->interval * 1000;
                    m_timeout_max = BOUND_EST_COUNT;
                    return;
                }
            }
        }
        m_rf_status = RF_STATUS_WAIT;
    }
    else
    {
        rf_rsp_t *rsp = &m_tx_buf.rsp;

        if (packet->type == PKT_CMD_GET_STATUS)
        {
            m_rf_status = RF_STATUS_TX;
            if (packet->seq == m_data_seq)
            {
                uint8_t s;
                buf_size_t len = DATA_LEN_MAX_TX;

                tx_packet->type = PKT_CMD_RSP_STATUS;
                if (m_bound_status < BOUND_STATUS_EST)
                {
                    tx_packet->length = PKT_DATA_OFFSET + 1 + sizeof(line_code_t);
                    rsp->opcode = OPCODE_BSP;
                    rsp->uart.baud_rate = uart0_para.baud_rate;
                    rsp->uart.stop_bits = uart0_para.stop_bits;
                    rsp->uart.parity_type = uart0_para.parity_type;
                    rsp->uart.data_bits = uart0_para.data_bits;
                    rsp->uart.io_status = uart0_para.io_status;
                    tx_packet->seq = m_data_seq;
                    tx_packet->resv = 0;
                }
                else
                {
                    s = UsbUart::get().read(rsp->payload.data, &len);
                    if (s == 0)
                    {
                        tx_packet->length = PKT_DATA_OFFSET + 1 + len;
                        rsp->opcode = OPCODE_DATA;
                        tx_packet->seq = m_data_seq;
                        tx_packet->resv = 0;
                    }
                    else if (s == 0x80)
                    {
                        tx_packet->length = PKT_DATA_OFFSET + 1 + sizeof(line_code_t);
                        rsp->opcode = OPCODE_BSP;
                        rsp->uart.baud_rate = uart0_para.baud_rate;
                        rsp->uart.stop_bits = uart0_para.stop_bits;
                        rsp->uart.parity_type = uart0_para.parity_type;
                        rsp->uart.data_bits = uart0_para.data_bits;
                        rsp->uart.io_status = uart0_para.io_status;
                        tx_packet->seq = m_data_seq;
                        tx_packet->resv = 0;
                    }
                    else
                    {
                        tx_packet->length = PKT_DATA_OFFSET;
                        rsp->opcode = OPCODE_ACK;
                        tx_packet->seq = m_data_seq;
                        tx_packet->resv = 0;
                    }
                }
                m_data_seq++;
            }
            rf.tx_start(tx_packet, 20);
        }
        else if (packet->type == PKT_DATA_FLAG)
        {
            m_rf_status = RF_STATUS_TX;
            if (packet->seq == m_data_seq)
            {
                buf_size_t len = packet->length;

                len -= PKT_DATA_OFFSET;
                m_buf.write(packet + 1, len);
                m_rx_data_status = DATA_STATUS_RCV;

                len = DATA_LEN_MAX_TX;
                tx_packet->type = PKT_DATA_RSP_ACK;
                if (m_bound_status == BOUND_STATUS_EST &&
                    !UsbUart::get().read(rsp->payload.data, &len))
                {
                    tx_packet->length = PKT_DATA_OFFSET + 1 + len;
                    rsp->opcode = OPCODE_DATA;
                    tx_packet->seq = m_data_seq;
                    tx_packet->resv = 0;
                }
                else
                {
                    tx_packet->length = PKT_DATA_OFFSET;
                    tx_packet->seq = m_data_seq;
                    tx_packet->resv = 0;
                }
                m_data_seq++;
            }
            rf.tx_start(tx_packet, 20);
        }
        else
        {
            if (++m_timeout > m_timeout_max)
            {
                m_rx_data_status = DATA_STATUS_TIMEOUT;
            }
            else
            {
                m_rf_status = RF_STATUS_WAIT;
            }
            return;
        }

        if (m_bound_status == BOUND_STATUS_WAIT)
        {
            m_bound_status = BOUND_STATUS_EST;
            m_interval_timer = CONN_INTERVAL * 1000;
            m_timeout_max = (CONN_TIMEOUT * 10 / CONN_INTERVAL);
        }
        m_timeout = 0;
    }
}

__HIGH_CODE
void RfUart::process_tx(void)
{
    if (m_rf_status == RF_STATUS_TXRSP)
    {
        bound((bound_rsp_t *)&m_tx_buf.rsp);
    }
    m_rf_status = RF_STATUS_RX;
    Rf::get().rx_start(m_interval_timer);
}

__HIGH_CODE
void RfUart::process_crc_error(void)
{
    m_rf_status = RF_STATUS_WAIT;
}

__HIGH_CODE
void RfUart::process_timeout(void)
{
    if (m_bound_status && ++m_timeout > m_timeout_max)
    {
        m_rx_data_status = DATA_STATUS_TIMEOUT;
    }
    else
    {
        m_rf_status = RF_STATUS_WAIT;
    }
}

__HIGH_CODE
uint8_t RfUart::read(void *buf, buf_size_t *len)
{
    if (m_rx_data_status == DATA_STATUS_RCV)
    {
        *len = m_buf.read(buf, *len);
        if (m_buf.empty())
        {
            PFIC_DisableIRQ(BLEL_IRQn);
            m_rx_data_status = DATA_STATUS_START;
            PFIC_EnableIRQ(BLEL_IRQn);
        }
    }
    else if (m_rx_data_status == DATA_STATUS_TIMEOUT)
    {
        *len = 0;
        disconnect();
    }
    else
    {
        *len = 0;
        if (m_rf_status == RF_STATUS_WAIT)
        {
            m_rf_status = RF_STATUS_RX;
            Rf::get().rx_start(m_interval_timer);
        }
    }
    return *len;
}

void RfUart::init(void)
{
    m_rx_data_status = DATA_STATUS_START;
    m_data_seq = 0;
    m_server_data = 0;
    m_bound_status = BOUND_STATUS_IDLE;
    m_interval_timer = CONN_INTERVAL * 1000;
    m_rf_status = RF_STATUS_WAIT;

    m_cbs = { on_rx, on_tx, on_crc_error, on_timeout };
    Rf::get().set_status_cbs(&m_cbs);
}
