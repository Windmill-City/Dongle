#ifndef RF_UART_RX_HPP
#define RF_UART_RX_HPP

#include <stdint.h>
#include <ch572rf.hpp>
#include "common.hpp"
#include "buf.hpp"
#include "rf.hpp"

#define RF_BUF_LEN       512

#define CONN_INTERVAL    10
#define CONN_TIMEOUT     100
#define CONN_PHY_TYPE    1

class RfUart
{
public:
    static RfUart &get();

    void init();
    uint8_t read(void *buf, buf_size_t *len);

private:
    static void on_rx(rf_packet_t *packet);
    static void on_tx(void);
    static void on_crc_error(void);
    static void on_timeout(void);

    void process_rx(rf_packet_t *packet);
    void process_tx(void);
    void process_crc_error(void);
    void process_timeout(void);
    void disconnect(void);
    void bound(bound_rsp_t *rsp);

    ByteBuf<RF_BUF_LEN> m_buf;
    rf_tx_buf_t m_tx_buf;
    rf_status_cbs_t m_cbs;
    uint32_t m_server_data;
    uint32_t m_interval_timer;
    uint16_t m_timeout_max;
    uint16_t m_timeout;
    uint8_t m_data_seq;
    uint8_t m_rf_status;
    uint8_t m_bound_status;
    uint8_t m_rx_data_status;
};

#endif
