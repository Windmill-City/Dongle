#pragma once

#include <stdint.h>
#include <ch572rf.hpp>
#include "common.hpp"

#define DEF_FREQUENCY   17
#define TEST_PHY_MODE   PHY_MODE_PHY_2M

#define AA            0x57250425
#define AA_EX         0
#define CRC_INIT      0x555555
#define CRC_POLY      0x80032d

typedef struct
{
    uint8_t type;
    uint8_t length;
    uint8_t seq;
    uint8_t resv;
} rf_packet_t;

#define PKT_HEAD_LEN        sizeof(rf_packet_t)
#define PKT_DATA_OFFSET     (PKT_HEAD_LEN - 2)

#define DATA_LEN_MAX_TX     251
#define BUF_LEN_TX          (DATA_LEN_MAX_TX + PKT_HEAD_LEN)
#define DATA_LEN_MAX_RX     BUF_LEN_TX

#define BOUND_EST_COUNT     6

enum data_status
{
    DATA_STATUS_START,
    DATA_STATUS_RCV,
    DATA_STATUS_TIMEOUT,
};

enum bound_status
{
    BOUND_STATUS_IDLE,
    BOUND_STATUS_WAIT,
    BOUND_STATUS_EST,
};

enum rf_status
{
    RF_STATUS_WAIT,
    RF_STATUS_RX,
    RF_STATUS_TX,
    RF_STATUS_TXRSP,
};

typedef struct __attribute__((packed))
{
    uint16_t interval;
    uint16_t server_data;
} bound_req_t;

typedef struct __attribute__((packed))
{
    uint32_t access_addr;
    uint8_t channel;
    uint8_t phy;
    uint16_t server_data;
    uint16_t interval;
    uint16_t timeout;
} bound_rsp_t;

typedef struct __attribute__((packed))
{
    uint8_t opcode;
    union
    {
        struct
        {
            uint32_t baud_rate;
            uint8_t stop_bits;
            uint8_t parity_type;
            uint8_t data_bits;
            uint8_t io_status;
        } uart;

        struct
        {
            uint8_t data[DATA_LEN_MAX_TX];
        } payload;
    };
} rf_rsp_t;

typedef struct
{
    rf_packet_t packet;
    rf_rsp_t rsp;
} rf_tx_buf_t;

#define PKT_CMD_BOUND_REQ     0x01
#define PKT_CMD_GET_STATUS    0x02
#define PKT_DATA_FLAG         0x7E

#define PKT_CMD_BOUND_RSP     (0x80 | PKT_CMD_BOUND_REQ)
#define PKT_CMD_RSP_STATUS    (0x80 | PKT_CMD_GET_STATUS)
#define PKT_DATA_RSP_ACK      (0x80 | PKT_DATA_FLAG)

typedef void (*rf_rx_cb_t)(rf_packet_t *packet);
typedef void (*rf_tx_cb_t)(void);
typedef void (*rf_rx_crc_cb_t)(void);
typedef void (*rf_timeout_cb_t)(void);

typedef struct
{
    rf_rx_cb_t rx_cb;
    rf_tx_cb_t tx_cb;
    rf_rx_crc_cb_t crc_err_cb;
    rf_timeout_cb_t timeout_cb;
} rf_status_cbs_t;

#define OPCODE_DATA           0x00
#define OPCODE_BSP            0x01
#define OPCODE_ACK            0xF0

class Rf
{
public:
    static Rf &get();

    void init();
    void set_status_cbs(rf_status_cbs_t *cbs);

    __HIGH_CODE void tx_set_frequency(uint32_t frequency);
    __HIGH_CODE void tx_set_sync_word(uint32_t sync_word);
    __HIGH_CODE void tx_set_phy_type(uint8_t phy);
    __HIGH_CODE void tx_start(void *buf, uint16_t rfon_us);

    __HIGH_CODE void rx_set_sync_word(uint32_t sync_word);
    __HIGH_CODE void rx_set_frequency(uint32_t frequency);
    __HIGH_CODE void rx_set_phy_type(uint8_t phy);
    __HIGH_CODE void rx_start(uint32_t rx_us);

private:
    static void process_callback(rfRole_States_t sta, uint8_t id);

    rfipTx_t m_tx;
    rfipRx_t m_rx;
    rf_status_cbs_t *m_cbs;
    uint8_t m_rx_buf[264] __attribute__((aligned(4)));
};

