#ifndef USB_UART_HPP
#define USB_UART_HPP

#include <stdint.h>
#include "common.hpp"
#include "buf.hpp"

typedef struct __attribute__((packed))
{
    uint32_t baud_rate;
    uint8_t stop_bits;
    uint8_t parity_type;
    uint8_t data_bits;
    uint8_t io_status;
} line_code_t;

extern line_code_t uart0_para;

class UsbUart
{
public:
    static UsbUart &get();

    void init();
    void poll();
    uint8_t read(void *buf, buf_size_t *len);
    void irq_handler(void);

private:
    static constexpr uint16_t USB_BUF_LEN = 256;
    static constexpr uint8_t USB_PACKET_SIZE = 64;
    static constexpr uint8_t CH341_REG_NUM = 10;
    static constexpr uint8_t USB_IRQ_FLAG_NUM = 4;

    void parse_line_coding(uint8_t reg_uart_ctrl);
    void ch341_reg_write(uint8_t reg_add, uint8_t reg_val);
    uint8_t ch341_reg_read(uint8_t reg_add, uint8_t *reg_val);
    static uint32_t ch341_calc_baud(uint8_t reg1, uint8_t reg2);
    static void ep_set_in_status(uint8_t ep_num, uint8_t type, uint8_t sta);
    void handle_setup(uint8_t len);
    void irq_process(void);
    void vendor_init(void);
    void dev_para_init(void);

    struct usb_dev_info_t
    {
        uint8_t config;
        uint8_t address;
        uint8_t flag;
    };

    struct usb_setup_req_t
    {
        uint8_t request_type;
        uint8_t request;
        uint8_t value_l;
        uint8_t value_h;
        uint8_t index_l;
        uint8_t index_h;
        uint8_t length_l;
        uint8_t length_h;
    };

    usb_dev_info_t m_dev_info;
    uint8_t m_setup_req_code;
    uint8_t m_setup_len;
    const uint8_t *m_descr_ptr;

    __attribute__((aligned(4))) uint8_t m_ep0_buffer[USB_PACKET_SIZE];
    __attribute__((aligned(4))) uint8_t m_ep1_buffer[USB_PACKET_SIZE];
    __attribute__((aligned(4))) uint8_t m_ep2_buffer[2 * USB_PACKET_SIZE];

    uint8_t m_ch341_reg_add[CH341_REG_NUM];
    uint8_t m_ch341_reg_val[CH341_REG_NUM];

    uint8_t m_uart_status;

    uint8_t m_irq_w_idx;
    uint8_t m_irq_r_idx;

    volatile uint8_t m_irq_len[USB_IRQ_FLAG_NUM];
    volatile uint8_t m_irq_pid[USB_IRQ_FLAG_NUM];
    volatile uint8_t m_irq_flag[USB_IRQ_FLAG_NUM];

    ByteBuf<USB_BUF_LEN> m_buf;
};

#endif
