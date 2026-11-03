#include "rf.h"
#include "rf_uart_rx.h"
#include "usb_uart.h"

static uint8_t g_rf_buf[RF_BUF_LEN];
static struct byte_buf *g_rf_buf_mgr;
static struct byte_buf g_rf_buf_struct;

static rf_tx_buf_t g_tx_buf;
static uint32_t g_server_data;
static uint32_t g_interval_timer;
static uint16_t g_timeout_max;
static uint16_t g_timeout;
static uint8_t g_data_seq;
static uint8_t g_rf_status;
static uint8_t g_bound_status;
static uint8_t g_rx_data_status;

static void rf_process_rx(rf_packet_t *packet);
static void rf_process_tx(void);
static void rf_process_crc_error(void);
static void rf_process_timeout(void);

static rf_status_cbs_t g_rf_cbs =
{
    rf_process_rx,
    rf_process_tx,
    rf_process_crc_error,
    rf_process_timeout,
};

static void rf_buffer_create(struct byte_buf **buf)
{
    *buf = byte_buf_create(&g_rf_buf_struct, g_rf_buf, sizeof(g_rf_buf));
}

__HIGH_CODE
uint32_t rf_rand16(uint32_t seed)
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
uint32_t rf_rand_aa(uint16_t rand)
{
    uint32_t aa = rand;

    aa = (aa << 8) | 0x6E0000B6;
    return aa;
}

static void rf_disconnect(void)
{
    RFRole_Shut();
    g_bound_status = BOUND_STATUS_IDLE;
    g_rx_data_status = DATA_STATUS_START;
    TMR_ITCfg(DISABLE, TMR_IT_CYC_END);

    rf_tx_set_sync_word(AA);
    rf_tx_set_frequency(DEF_FREQUENCY);
    rf_rx_set_sync_word(AA);
    rf_rx_set_frequency(DEF_FREQUENCY);
    g_rf_status = RF_STATUS_WAIT;
}

static void rf_bound(bound_rsp_t *rsp)
{
    g_timeout = 0;
    g_bound_status = BOUND_STATUS_WAIT;
    rf_tx_set_sync_word(rsp->access_addr);
    rf_tx_set_frequency(rsp->channel);
    rf_tx_set_phy_type(rsp->phy);

    rf_rx_set_sync_word(rsp->access_addr);
    rf_rx_set_frequency(rsp->channel);
    rf_rx_set_phy_type(rsp->phy);
}

__HIGH_CODE
static void rf_process_rx(rf_packet_t *packet)
{
    rf_packet_t *tx_packet = (rf_packet_t *)g_tx_buf.tx_buf;

    if (g_bound_status == BOUND_STATUS_IDLE)
    {
        if (packet->type == PKT_CMD_BOUND_REQ)
        {
            BOOL reg = 0;
            bound_req_t *req = (bound_req_t *)(packet + 1);

            if (packet->length == sizeof(bound_req_t) + 2)
            {
                int8_t rssi = *(uint8_t *)((uint8_t *)packet + packet->length + 2 + 2);

                if (req->server_data)
                {
                    if (!g_server_data || req->server_data == g_server_data)
                    {
                        reg = 1;
                    }
                }
                else
                {
                    if (rssi > -35)
                    {
                        reg = 1;
                    }
                }

                if (reg)
                {
                    bound_rsp_t *rsp = (bound_rsp_t *)&g_tx_buf.tx_buf[PKT_HEAD_LEN];

                    g_rf_status = RF_STATUS_TXRSP;

                    g_data_seq = 0;
                    g_server_data = rf_rand16(rssi);
                    tx_packet->type = PKT_CMD_BOUND_RSP;
                    tx_packet->length = PKT_DATA_OFFSET + sizeof(bound_rsp_t);
                    tx_packet->seq = g_data_seq;
                    tx_packet->resv = 0;
                    rsp->access_addr = rf_rand_aa(g_server_data);
                    rsp->channel = g_server_data & 0x3F;
                    rsp->phy = CONN_PHY_TYPE;
                    rsp->server_data = g_server_data;
                    rsp->interval = CONN_INTERVAL;
                    rsp->timeout = CONN_TIMEOUT;
                    rf_tx_start(tx_packet, 20);
                    g_data_seq++;
                    g_interval_timer = req->interval * 1000;
                    g_timeout_max = BOUND_EST_COUNT;
                    return;
                }
            }
        }
        g_rf_status = RF_STATUS_WAIT;
    }
    else
    {
        rf_rsp_t *rsp = (rf_rsp_t *)&g_tx_buf.tx_buf[PKT_HEAD_LEN];

        if (packet->type == PKT_CMD_GET_STATUS)
        {
            g_rf_status = RF_STATUS_TX;
            if (packet->seq == g_data_seq)
            {
                uint8_t s;
                buf_size_t len = DATA_LEN_MAX_TX;

                tx_packet->type = PKT_CMD_RSP_STATUS;
                if (g_bound_status < BOUND_STATUS_EST)
                {
                    tx_packet->length = PKT_DATA_OFFSET + 1 + sizeof(line_code_t);
                    rsp->opcode = OPCODE_BSP;
                    rsp->uart.baud_rate = uart0_para.baud_rate;
                    rsp->uart.stop_bits = uart0_para.stop_bits;
                    rsp->uart.parity_type = uart0_para.parity_type;
                    rsp->uart.data_bits = uart0_para.data_bits;
                    rsp->uart.io_status = uart0_para.io_status;
                    tx_packet->seq = g_data_seq;
                    tx_packet->resv = 0;
                }
                else
                {
                    s = usb_uart_read(rsp->payload.data, &len);
                    if (s == 0)
                    {
                        tx_packet->length = PKT_DATA_OFFSET + 1 + len;
                        rsp->opcode = OPCODE_DATA;
                        tx_packet->seq = g_data_seq;
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
                        tx_packet->seq = g_data_seq;
                        tx_packet->resv = 0;
                    }
                    else
                    {
                        tx_packet->length = PKT_DATA_OFFSET;
                        rsp->opcode = OPCODE_ACK;
                        tx_packet->seq = g_data_seq;
                        tx_packet->resv = 0;
                    }
                }
                g_data_seq++;
            }
            rf_tx_start(tx_packet, 20);
        }
        else if (packet->type == PKT_DATA_FLAG)
        {
            g_rf_status = RF_STATUS_TX;
            if (packet->seq == g_data_seq)
            {
                buf_size_t len = packet->length;

                len -= PKT_DATA_OFFSET;
                byte_buf_write(g_rf_buf_mgr, (packet + 1), &len);
                g_rx_data_status = DATA_STATUS_RCV;

                len = DATA_LEN_MAX_TX;
                tx_packet->type = PKT_DATA_RSP_ACK;
                if (g_bound_status == BOUND_STATUS_EST &&
                    !usb_uart_read(rsp->payload.data, &len))
                {
                    tx_packet->length = PKT_DATA_OFFSET + 1 + len;
                    rsp->opcode = OPCODE_DATA;
                    tx_packet->seq = g_data_seq;
                    tx_packet->resv = 0;
                }
                else
                {
                    tx_packet->length = PKT_DATA_OFFSET;
                    tx_packet->seq = g_data_seq;
                    tx_packet->resv = 0;
                }
                g_data_seq++;
            }
            rf_tx_start(tx_packet, 20);
        }
        else
        {
            if (++g_timeout > g_timeout_max)
            {
                g_rx_data_status = DATA_STATUS_TIMEOUT;
            }
            else
            {
                g_rf_status = RF_STATUS_WAIT;
            }
            return;
        }

        if (g_bound_status == BOUND_STATUS_WAIT)
        {
            g_bound_status = BOUND_STATUS_EST;
            g_interval_timer = CONN_INTERVAL * 1000;
            g_timeout_max = (CONN_TIMEOUT * 10 / CONN_INTERVAL);
        }
        g_timeout = 0;
    }
}

__HIGH_CODE
static void rf_process_tx(void)
{
    if (g_rf_status == RF_STATUS_TXRSP)
    {
        rf_bound((bound_rsp_t *)&g_tx_buf.tx_buf[PKT_HEAD_LEN]);
    }
    g_rf_status = RF_STATUS_RX;
    rf_rx_start(g_interval_timer);
}

__HIGH_CODE
static void rf_process_crc_error(void)
{
    g_rf_status = RF_STATUS_WAIT;
}

__HIGH_CODE
static void rf_process_timeout(void)
{
    if (g_bound_status && ++g_timeout > g_timeout_max)
    {
        g_rx_data_status = DATA_STATUS_TIMEOUT;
    }
    else
    {
        g_rf_status = RF_STATUS_WAIT;
    }
}

__HIGH_CODE
uint8_t rf_uart_rx_read(void *buf, buf_size_t *len)
{
    if (g_rx_data_status == DATA_STATUS_RCV)
    {
        if (byte_buf_read(g_rf_buf_mgr, buf, len) == 0)
        {
            PFIC_DisableIRQ(BLEL_IRQn);
            g_rx_data_status = DATA_STATUS_START;
            PFIC_EnableIRQ(BLEL_IRQn);
        }
    }
    else if (g_rx_data_status == DATA_STATUS_TIMEOUT)
    {
        *len = 0;
        rf_disconnect();
    }
    else
    {
        *len = 0;
        if (g_rf_status == RF_STATUS_WAIT)
        {
            g_rf_status = RF_STATUS_RX;
            rf_rx_start(g_interval_timer);
        }
    }
    return *len;
}

__HIGH_CODE
void rf_uart_rx_init(void)
{
    g_rx_data_status = DATA_STATUS_IDLE;
    rf_buffer_create(&g_rf_buf_mgr);
    g_data_seq = 0;
    g_server_data = 0;
    g_bound_status = BOUND_STATUS_IDLE;
    g_tx_buf.status = 0;
    g_interval_timer = CONN_INTERVAL * 1000;
    rf_role_register_status_cbs(&g_rf_cbs);
    g_rf_status = RF_STATUS_WAIT;
}
