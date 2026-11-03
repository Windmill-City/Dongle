#ifndef RF_H
#define RF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <ch572rf.h>
#include "ch57x_common.h"
#include "buf.h"

#define DEF_FREQUENCY   17
#define TEST_PHY_MODE   PHY_MODE_PHY_2M

#if (TEST_PHY_MODE == PHY_MODE_2G4)

#if (PHY_2G4_MODE == 0)
#define AA            0x94826E8E
#define AA_EX         0
#define AA_LEN        1
#define PRE_LEN       1
#define CRC_INIT      0xFFFF
#define CRC_POLY      0x8810
#define CRC_LEN       2
#define CTL_FILED     0
#define DPL_EN        0
#define DATA_ORDER    0
#define MODE_2G4      PHY_2G4_1M
#define CRC_XOR_EN    0

#elif (PHY_2G4_MODE == 1)
#define PKT_DET_CFG4(var)     { (*((PUINT32V)0x4000C120)) = var; }
#define AA            0x94826E8E
#define AA_EX         0
#define AA_LEN        1
#define PRE_LEN       1
#define CRC_INIT      0xFFFF
#define CRC_POLY      0x8810
#define CRC_LEN       2
#define CTL_FILED     0
#define DPL_EN        0
#define DATA_ORDER    0
#define MODE_2G4      PHY_2G4_1M
#define CRC_XOR_EN    0

#elif (PHY_2G4_MODE == 2)
#define AA            0x94826E8E
#define AA_EX         0
#define AA_LEN        1
#define PRE_LEN       1
#define CRC_INIT      0xFFFF
#define CRC_POLY      0x8810
#define CRC_LEN       2
#define CTL_FILED     0
#define DPL_EN        0
#define DATA_ORDER    1
#define MODE_2G4      PHY_2G4_1M
#define CRC_XOR_EN    1

#endif

#else

#define AA            0x57250425
#define AA_EX         0
#define CRC_INIT      0x555555
#define CRC_POLY      0x80032d

#endif

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
    DATA_STATUS_IDLE,
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
    RF_STATUS_IDLE,
    RF_STATUS_REQ,
    RF_STATUS_GETS,
    RF_STATUS_WAIT,
    RF_STATUS_RX,
    RF_STATUS_TX,
    RF_STATUS_WAITRSP,
    RF_STATUS_TXRSP,
    RF_STATUS_TXACK,
    RF_STATUS_RETX,
    RF_STATUS_REWAIT,
};

typedef struct
{
    uint8_t tx_buf[BUF_LEN_TX];
    buf_size_t len;
    uint8_t status;
    uint8_t resend_count;
} rf_tx_buf_t;

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
            uint8_t data[BUF_LEN_TX];
        } payload;
    };
} rf_rsp_t;

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

void rf_tx_set_frequency(uint32_t frequency);
void rf_tx_set_sync_word(uint32_t sync_word);
void rf_tx_start(void *buf, uint16_t rfon_us);
void rf_tx_set_phy_type(uint8_t phy);

void rf_rx_set_sync_word(uint32_t sync_word);
void rf_rx_set_frequency(uint32_t frequency);
void rf_rx_start(uint32_t rx_us);
void rf_rx_set_phy_type(uint8_t phy);

void rf_role_init(void);
void rf_role_register_status_cbs(rf_status_cbs_t *cbs);

#ifdef __cplusplus
}
#endif

#endif
