#include "common.hpp"
#include "usb_uart.hpp"
#include "rf_uart_rx.hpp"
#include "rf.hpp"

#define THIS_ENDP0_SIZE       64
#define DEF_IC_PRG_VER        0x31

#define ENDP0                 0x00
#define ENDP1                 0x01

#define ENDP_TYPE_IN          0x00

#define IN_ACK                0
#define IN_NAK                2

#define DEF_BIT_USB_ADDRESS   0x04

#define DEF_VEN_DEBUG_READ    0x95
#define DEF_VEN_DEBUG_WRITE   0x9A
#define DEF_VEN_UART_INIT     0xA1
#define DEF_VEN_UART_M_OUT    0xA4
#define DEF_VEN_BUF_CLEAR     0xB2
#define DEF_VEN_I2C_CMD_X     0x54
#define DEF_VEN_DELAY_MS      0x5E
#define DEF_VEN_GET_VER       0x5F

#define DEF_SET_LINE_CODING        0x20
#define DEF_GET_LINE_CODING        0x21
#define DEF_SET_CONTROL_LINE_STATE 0x22
#define DEF_SERIAL_STATE           0x20

#define HAL_UART_5_BITS_PER_CHAR   5
#define HAL_UART_6_BITS_PER_CHAR   6
#define HAL_UART_7_BITS_PER_CHAR   7
#define HAL_UART_8_BITS_PER_CHAR   8

#define HAL_UART_ONE_STOP_BIT      1
#define HAL_UART_TWO_STOP_BITS     2

#define HAL_UART_NO_PARITY         0x00
#define HAL_UART_ODD_PARITY        0x01
#define HAL_UART_EVEN_PARITY       0x02
#define HAL_UART_MARK_PARITY       0x03
#define HAL_UART_SPACE_PARITY      0x04

static const uint8_t usb_vendor_dev_desc[] =
{
    0x12, 0x01,
    0x10, 0x01,
    0xFF, 0x00, 0x02, 0x40,
    0x86, 0x1A, 0x23, 0x75,
    0x00, DEF_IC_PRG_VER,
    0x01, 0x02, 0x03,
    0x01
};

static const uint8_t usb_vendor_cfg_desc[] =
{
    0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0x80, 0x30,
    0x09, 0x04, 0x00, 0x00, 0x03, 0xFF, 0x01, 0x02, 0x00,
    0x07, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00,
    0x07, 0x05, 0x02, 0x02, 0x20, 0x00, 0x00,
    0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x01
};

static const uint8_t usb_qualifier_desc[] =
{
    0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00
};

static const uint8_t usb_lang_str_desc[] =
{
    0x04, 0x03, 0x09, 0x04
};

static const uint8_t usb_manufacturer_str[] =
{
    0x0E, 0x03,
    'w', 0x00, 'c', 0x00, 'h', 0x00, '.', 0x00, 'c', 0x00, 'n', 0x00
};

static const uint8_t usb_product_str[] =
{
    0x2C, 0x03,
    'U', 0x00, 'S', 0x00, 'B', 0x00, '2', 0x00, '.', 0x00, '0', 0x00,
    ' ', 0x00, 'T', 0x00, 'o', 0x00, ' ', 0x00, 'S', 0x00, 'e', 0x00,
    'r', 0x00, 'i', 0x00, 'a', 0x00, 'l', 0x00, ' ', 0x00, 'P', 0x00,
    'o', 0x00, 'r', 0x00, 't', 0x00
};

static const uint8_t usb_serial_str[] =
{
    0x1A, 0x03,
    'W', 0x00, 'C', 0x00, 'H', 0x00, '4', 0x00, '5', 0x00, '4', 0x00,
    '5', 0x00, '4', 0x00, '5', 0x00, 'T', 0x00, 'S', 0x00, '2', 0x00
};

line_code_t uart0_para;

static UsbUart s_usb_uart;

UsbUart &UsbUart::get()
{
    return s_usb_uart;
}

extern "C"
{
__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    UsbUart::get().irq_handler();
}
}

void UsbUart::parse_line_coding(uint8_t reg_uart_ctrl)
{
    uint8_t data_bit_val = reg_uart_ctrl & 0x03;
    uint8_t parity_val = reg_uart_ctrl & 0x38;

    if (data_bit_val == 0x00) data_bit_val = HAL_UART_5_BITS_PER_CHAR;
    else if (data_bit_val == 0x01) data_bit_val = HAL_UART_6_BITS_PER_CHAR;
    else if (data_bit_val == 0x02) data_bit_val = HAL_UART_7_BITS_PER_CHAR;
    else data_bit_val = HAL_UART_8_BITS_PER_CHAR;

    if (parity_val == 0x00) parity_val = HAL_UART_NO_PARITY;
    else if (parity_val == 0x08) parity_val = HAL_UART_ODD_PARITY;
    else if (parity_val == 0x18) parity_val = HAL_UART_EVEN_PARITY;
    else if (parity_val == 0x28) parity_val = HAL_UART_MARK_PARITY;
    else parity_val = HAL_UART_SPACE_PARITY;

    uart0_para.stop_bits = (reg_uart_ctrl & 0x04) ?
                           HAL_UART_TWO_STOP_BITS : HAL_UART_ONE_STOP_BIT;
    uart0_para.parity_type = parity_val;
    uart0_para.data_bits = data_bit_val;
}

void UsbUart::ch341_reg_write(uint8_t reg_add, uint8_t reg_val)
{
    uint8_t find_idx = 0;
    uint8_t find_flag = 0;
    uint8_t i;

    for (i = 0; i < CH341_REG_NUM; i++)
    {
        if (m_ch341_reg_add[i] == reg_add)
        {
            find_flag = 1;
            break;
        }
        if (m_ch341_reg_add[i] == 0xFF)
        {
            break;
        }
    }
    find_idx = i;

    if (find_flag)
    {
        m_ch341_reg_val[find_idx] = reg_val;
    }
    else
    {
        m_ch341_reg_add[find_idx] = reg_add;
        m_ch341_reg_val[find_idx] = reg_val;
    }

    if (reg_add == 0x18)
    {
        parse_line_coding(reg_val);
    }
}

uint8_t UsbUart::ch341_reg_read(uint8_t reg_add, uint8_t *reg_val)
{
    uint8_t find_flag = 0;
    uint8_t i;

    *reg_val = 0;
    for (i = 0; i < CH341_REG_NUM; i++)
    {
        if (m_ch341_reg_add[i] == reg_add)
        {
            find_flag = 1;
            *reg_val = m_ch341_reg_val[i];
            break;
        }
        if (m_ch341_reg_add[i] == 0xFF)
        {
            break;
        }
    }

    if (reg_add == 0x18)
    {
        *reg_val = R8_UART_LCR | (*reg_val & 0x40);
    }
    else if (reg_add == 0x06 || reg_add == 0x07)
    {
        *reg_val = 0;
    }

    return find_flag;
}

uint32_t UsbUart::ch341_calc_baud(uint8_t reg1, uint8_t reg2)
{
    uint32_t ref_clock;
    uint8_t divider;

    if ((reg1 == 0x87) && (reg2 == 0xF3)) return 921600;
    if ((reg1 == 0x87) && (reg2 == 0xD9)) return 307200;

    divider = 0 - reg2;

    switch (reg1)
    {
        case 0x88:
        case 0xC8:
            ref_clock = 36923077 / 8;
            break;
        case 0x89:
        case 0xC9:
            ref_clock = 36923077 / 8 / 256;
            break;
        case 0x8A:
        case 0xCA:
            ref_clock = 32000000 / 8;
            break;
        case 0x8B:
        case 0xCB:
            ref_clock = 32000000 / 8 / 256;
            break;
        default:
            switch (reg1 & 0x7F)
            {
                case 3: ref_clock = 6000000; break;
                case 2: ref_clock = 750000; break;
                case 1: ref_clock = 93750; break;
                case 0: ref_clock = 11719; break;
                default: return 115200;
            }
            break;
    }

    if (!divider)
    {
        return 115200;
    }
    return ref_clock / divider;
}

void UsbUart::ep_set_in_status(uint8_t ep_num, uint8_t type, uint8_t sta)
{
    volatile uint8_t *p_uepn_ctrl;

    p_uepn_ctrl = reinterpret_cast<volatile uint8_t *>(USB_BASE_ADDR + 0x22 + ep_num * 4);
    if (type == ENDP_TYPE_IN)
    {
        *p_uepn_ctrl = (*p_uepn_ctrl & ~0x03) | sta;
    }
    else
    {
        *p_uepn_ctrl = (*p_uepn_ctrl & ~(0x03 << 2)) | (sta << 2);
    }
}

__HIGH_CODE
void UsbUart::irq_handler(void)
{
    if (R8_USB_INT_FG & RB_UIF_TRANSFER)
    {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            m_irq_flag[m_irq_w_idx] = 1;
            m_irq_pid[m_irq_w_idx] = R8_USB_INT_ST;
            m_irq_len[m_irq_w_idx] = R8_USB_RX_LEN;

            switch (m_irq_pid[m_irq_w_idx] & 0x3F)
            {
                case UIS_TOKEN_OUT | 2:
                {
                    if (R8_USB_INT_FG & RB_U_TOG_OK)
                    {
                        buf_size_t data_len;

                        R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        data_len = m_irq_len[m_irq_w_idx];
                        m_buf.write(m_ep2_buffer, data_len);
                        if (m_buf.size() > (USB_BUF_LEN - USB_PACKET_SIZE))
                        {
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_NAK;
                        }
                    }
                    else
                    {
                        m_irq_flag[m_irq_w_idx] = 0;
                    }
                } break;

                case UIS_TOKEN_IN | 2:
                {
                    R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | IN_NAK;
                } break;

                case UIS_TOKEN_OUT | 1:
                {
                    if (R8_USB_INT_FG & RB_U_TOG_OK)
                    {
                        R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                        R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xF3) | 0x08;
                    }
                    else
                    {
                        m_irq_flag[m_irq_w_idx] = 0;
                    }
                } break;

                case UIS_TOKEN_IN | 1:
                {
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xFC) | IN_NAK;
                } break;

                case UIS_TOKEN_OUT | 0:
                {
                    if (R8_USB_INT_FG & RB_U_TOG_OK)
                    {
                        R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xF3) | 0x08;
                    }
                    else
                    {
                        m_irq_flag[m_irq_w_idx] = 0;
                    }
                } break;

                case UIS_TOKEN_IN | 0:
                {
                    R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xFC) | IN_NAK;
                } break;
            }

            if (m_irq_flag[m_irq_w_idx])
            {
                m_irq_w_idx++;
                if (m_irq_w_idx >= USB_IRQ_FLAG_NUM) m_irq_w_idx = 0;
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            m_irq_flag[m_irq_w_idx] = 1;
            m_irq_pid[m_irq_w_idx] = UIS_TOKEN_SETUP | 0;
            m_irq_len[m_irq_w_idx] = 8;

            m_irq_w_idx++;
            if (m_irq_w_idx >= USB_IRQ_FLAG_NUM) m_irq_w_idx = 0;

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if (R8_USB_INT_FG & RB_UIF_BUS_RST)
    {
        R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

        R8_USB_DEV_AD = 0x00;
        m_dev_info.address = 0;

        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if (R8_USB_INT_FG & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}

void UsbUart::handle_setup(uint8_t len)
{
    usb_setup_req_t *setup = (usb_setup_req_t *)m_ep0_buffer;
    uint8_t data_dir;
    uint8_t buf[8];

    if (len != sizeof(usb_setup_req_t))
    {
        R8_UEP0_T_LEN = 0;
        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
        PFIC_EnableIRQ(USB_IRQn);
        return;
    }

    m_setup_len = setup->length_l;
    if (setup->length_h) m_setup_len = 0xFF;

    len = 0;
    m_setup_req_code = setup->request;

    data_dir = USB_REQ_TYP_OUT;
    if (setup->request_type & USB_REQ_TYP_IN) data_dir = USB_REQ_TYP_IN;

    if ((setup->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_VENDOR)
    {
        switch (m_setup_req_code)
        {
            case DEF_VEN_DEBUG_WRITE:
            {
                uint8_t reg_add1 = m_ep0_buffer[2];
                uint8_t reg_add2 = m_ep0_buffer[3];
                uint8_t reg_val1 = m_ep0_buffer[4];
                uint8_t reg_val2 = m_ep0_buffer[5];

                if ((reg_add1 == 0x12) && (reg_add2 == 0x13))
                {
                    uart0_para.baud_rate = ch341_calc_baud(setup->index_l, setup->index_h);
                    m_uart_status = 1;
                }
                else
                {
                    ch341_reg_write(reg_add1, reg_val1);
                    ch341_reg_write(reg_add2, reg_val2);
                }
            } break;

            case DEF_VEN_DEBUG_READ:
            {
                uint8_t reg_val1;
                uint8_t reg_val2;

                ch341_reg_read(setup->value_l, &reg_val1);
                ch341_reg_read(setup->value_h, &reg_val2);

                len = 2;
                buf[0] = reg_val1;
                buf[1] = reg_val2;
                m_setup_len = len;
                memcpy(m_ep0_buffer, buf, len);
            } break;

            case DEF_VEN_UART_INIT:
            {
                uint8_t uart_reg1_val;

                if (m_ep0_buffer[2] & 0x80)
                {
                    parse_line_coding(m_ep0_buffer[3]);

                    uart_reg1_val = setup->index_l;
                    if (!(uart_reg1_val & (1 << 6)))
                    {
                        uart_reg1_val &= 0xC7;
                    }

                    uart0_para.baud_rate = ch341_calc_baud(uart_reg1_val, setup->index_h);
                    m_uart_status = 1;
                }
            } break;

            case DEF_VEN_UART_M_OUT:
            {
                uart0_para.io_status = m_ep0_buffer[2];
                m_uart_status = 1;
            } break;

            case DEF_VEN_BUF_CLEAR:
            case DEF_VEN_I2C_CMD_X:
            case DEF_VEN_DELAY_MS:
                break;

            case DEF_VEN_GET_VER:
            {
                len = 2;
                buf[0] = 0x30;
                buf[1] = 0x00;
                m_setup_len = len;
                memcpy(m_ep0_buffer, buf, len);
            } break;

            default:
                break;
        }
    }
    else if ((setup->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
    {
        switch (m_setup_req_code)
        {
            case USB_GET_DESCRIPTOR:
            {
                switch (setup->value_h)
                {
                    case 1:
                        m_descr_ptr = usb_vendor_dev_desc;
                        len = sizeof(usb_vendor_dev_desc);
                        break;

                    case 2:
                        m_descr_ptr = usb_vendor_cfg_desc;
                        len = sizeof(usb_vendor_cfg_desc);
                        break;

                    case 3:
                        switch (setup->value_l)
                        {
                            case 0:
                                m_descr_ptr = usb_lang_str_desc;
                                len = sizeof(usb_lang_str_desc);
                                break;
                            case 1:
                                m_descr_ptr = usb_manufacturer_str;
                                len = sizeof(usb_manufacturer_str);
                                break;
                            case 2:
                                m_descr_ptr = usb_product_str;
                                len = sizeof(usb_product_str);
                                break;
                            case 3:
                                m_descr_ptr = usb_serial_str;
                                len = sizeof(usb_serial_str);
                                break;
                            default:
                                len = 0xFF;
                                break;
                        }
                        break;

                    case 6:
                        m_descr_ptr = usb_qualifier_desc;
                        len = sizeof(usb_qualifier_desc);
                        break;

                    default:
                        len = 0xFF;
                        break;
                }

                if (len != 0xFF)
                {
                    if (m_setup_len > len) m_setup_len = len;
                    len = (m_setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : m_setup_len;
                    memcpy(m_ep0_buffer, m_descr_ptr, len);
                    m_setup_len -= len;
                    m_descr_ptr += len;
                }
            } break;

            case USB_SET_ADDRESS:
            {
                m_dev_info.flag |= DEF_BIT_USB_ADDRESS;
                m_dev_info.address = setup->value_l;
            } break;

            case USB_GET_CONFIGURATION:
            {
                m_ep0_buffer[0] = m_dev_info.config;
                if (m_setup_len >= 1) len = 1;
            } break;

            case USB_SET_CONFIGURATION:
            {
                m_dev_info.config = setup->value_l;
            } break;

            case USB_CLEAR_FEATURE:
            {
                if ((setup->request_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                {
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                }
                else if ((setup->request_type & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                {
                    switch (setup->index_l)
                    {
                        case 0x82: R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK; break;
                        case 0x02: R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK; break;
                        case 0x81: R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK; break;
                        case 0x01: R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK; break;
                        default: len = 0xFF; break;
                    }
                }
                else
                {
                    len = 0xFF;
                }
            } break;

            case USB_GET_INTERFACE:
            {
                m_ep0_buffer[0] = 0x00;
                if (m_setup_len >= 1) len = 1;
            } break;

            case USB_GET_STATUS:
            {
                m_ep0_buffer[0] = 0x00;
                m_ep0_buffer[1] = 0x00;
                len = (m_setup_len >= 2) ? 2 : m_setup_len;
            } break;

            default:
                len = 0xFF;
                break;
        }
    }
    else if ((setup->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS)
    {
        if (data_dir == USB_REQ_TYP_OUT)
        {
            switch (m_setup_req_code)
            {
                case DEF_SET_LINE_CODING:
                    len = 0;
                    break;

                case DEF_SET_CONTROL_LINE_STATE:
                    len = 0;
                    break;

                default:
                    len = 0xFF;
                    break;
            }
        }
        else
        {
            switch (m_setup_req_code)
            {
                case DEF_GET_LINE_CODING:
                {
                    line_code_t *line_code = (line_code_t *)m_ep0_buffer;

                    m_descr_ptr = m_ep0_buffer;
                    len = sizeof(line_code_t);
                    line_code->baud_rate = uart0_para.baud_rate;
                    line_code->stop_bits = uart0_para.stop_bits;
                    line_code->parity_type = uart0_para.parity_type;
                    line_code->data_bits = uart0_para.data_bits;
                } break;

                case DEF_SERIAL_STATE:
                {
                    len = 2;
                    m_ep0_buffer[0] = 0;
                    m_ep0_buffer[1] = 0;
                } break;

                default:
                    len = 0xFF;
                    break;
            }
        }
    }
    else
    {
        len = 0xFF;
    }

    if (len == 0xFF)
    {
        m_setup_req_code = 0xFF;
        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
        PFIC_EnableIRQ(USB_IRQn);
    }
    else if (len <= THIS_ENDP0_SIZE)
    {
        if (m_setup_req_code == USB_GET_DESCRIPTOR)
        {
            R8_UEP0_T_LEN = len;
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
            PFIC_EnableIRQ(USB_IRQn);
        }
        else
        {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_T_LEN = len;
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
            PFIC_EnableIRQ(USB_IRQn);
        }
    }
    else
    {
        R8_UEP0_T_LEN = 0;
        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
        PFIC_EnableIRQ(USB_IRQn);
    }
}

void UsbUart::irq_process(void)
{
    uint8_t len;
    uint8_t i;

    i = m_irq_r_idx;
    m_irq_r_idx++;
    if (m_irq_r_idx >= USB_IRQ_FLAG_NUM) m_irq_r_idx = 0;

    switch (m_irq_pid[i] & 0x3F)
    {
        case UIS_TOKEN_OUT | 1:
        {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP1_CTRL = R8_UEP1_CTRL & 0xF3;
            PFIC_EnableIRQ(USB_IRQn);
        } break;

        case UIS_TOKEN_SETUP | 0:
        {
            len = m_irq_len[i];
            handle_setup(len);
        } break;

        case UIS_TOKEN_IN | 0:
        {
            switch (m_setup_req_code)
            {
                case USB_GET_DESCRIPTOR:
                {
                    len = (m_setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : m_setup_len;
                    memcpy(m_ep0_buffer, m_descr_ptr, len);
                    m_setup_len -= len;
                    m_descr_ptr += len;

                    if (len)
                    {
                        R8_UEP0_T_LEN = len;
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                        ep_set_in_status(ENDP0, ENDP_TYPE_IN, IN_ACK);
                        PFIC_EnableIRQ(USB_IRQn);
                    }
                    else
                    {
                        R8_UEP0_T_LEN = len;
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                        PFIC_EnableIRQ(USB_IRQn);
                    }
                } break;

                case USB_SET_ADDRESS:
                {
                    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | m_dev_info.address;
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } break;

                case DEF_VEN_DEBUG_READ:
                case DEF_VEN_GET_VER:
                case DEF_GET_LINE_CODING:
                {
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } break;

                case DEF_SET_LINE_CODING:
                {
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } break;

                default:
                {
                    R8_UEP0_T_LEN = 0;
                    PFIC_DisableIRQ(USB_IRQn);
                    R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                    PFIC_EnableIRQ(USB_IRQn);
                } break;
            }
        } break;

        case UIS_TOKEN_OUT | 0:
        {
            len = m_irq_len[i];
            if (len && (m_setup_req_code == DEF_SET_LINE_CODING))
            {
                uint32_t baud_rate;

                memcpy(&baud_rate, m_ep0_buffer, 4);
                uart0_para.baud_rate = baud_rate;
                uart0_para.stop_bits = m_ep0_buffer[4];
                uart0_para.parity_type = m_ep0_buffer[5];
                uart0_para.data_bits = m_ep0_buffer[6];

                PFIC_DisableIRQ(USB_IRQn);
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
                PFIC_EnableIRQ(USB_IRQn);
            }
            else
            {
                PFIC_DisableIRQ(USB_IRQn);
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_NAK;
                PFIC_EnableIRQ(USB_IRQn);
            }
        } break;

        default:
            break;
    }
}

void UsbUart::vendor_init(void)
{
    R8_USB_CTRL = 0x00;

    R8_UEP4_1_MOD = RB_UEP1_TX_EN | RB_UEP1_RX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN;

    R16_UEP0_DMA = reinterpret_cast<uint32_t>(&m_ep0_buffer[0]);
    R16_UEP1_DMA = reinterpret_cast<uint32_t>(&m_ep1_buffer[0]);
    R16_UEP2_DMA = reinterpret_cast<uint32_t>(&m_ep2_buffer[0]);

    R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

    R8_USB_DEV_AD = 0x00;
    R8_UDEV_CTRL = RB_UD_PD_DIS;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_USB_INT_FG = 0xFF;
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_TRANSFER | RB_UIE_BUS_RST;

    PFIC_EnableIRQ(USB_IRQn);

    R8_UDEV_CTRL |= RB_UD_PORT_EN;

    m_dev_info.config = 0;
    m_dev_info.address = 0;
    m_dev_info.flag = 0;
}

void UsbUart::dev_para_init(void)
{
    uint8_t i;

    uart0_para.baud_rate = 115200;
    uart0_para.data_bits = HAL_UART_8_BITS_PER_CHAR;
    uart0_para.parity_type = HAL_UART_NO_PARITY;
    uart0_para.stop_bits = HAL_UART_ONE_STOP_BIT;
    uart0_para.io_status = 0;
    m_uart_status = 1;

    for (i = 0; i < CH341_REG_NUM; i++)
    {
        m_ch341_reg_add[i] = 0xFF;
        m_ch341_reg_val[i] = 0x00;
    }

    for (i = 0; i < USB_IRQ_FLAG_NUM; i++)
    {
        m_irq_flag[i] = 0;
    }
}

void UsbUart::init(void)
{
    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;

    dev_para_init();
    vendor_init();

    PFIC_EnableIRQ(USB_IRQn);
}

void UsbUart::poll(void)
{
    buf_size_t len;
    uint8_t i;

    i = m_irq_r_idx;
    if (m_irq_flag[i])
    {
        irq_process();
        m_irq_flag[i] = 0;
    }

    if (m_dev_info.address)
    {
        if ((R8_UEP2_CTRL & MASK_UEP_T_RES) == UEP_T_RES_NAK)
        {
            len = USB_PACKET_SIZE / 2;
            if (RfUart::get().read(&m_ep2_buffer[USB_PACKET_SIZE], &len))
            {
                R8_UEP2_T_LEN = (uint8_t)len;
                PFIC_DisableIRQ(USB_IRQn);
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
                PFIC_EnableIRQ(USB_IRQn);
            }
        }
    }
}

__HIGH_CODE
uint8_t UsbUart::read(void *buf, buf_size_t *len)
{
    if (m_dev_info.address)
    {
        if (m_uart_status)
        {
            m_uart_status = 0;
            *len = 0;
            return 0x80;
        }
    }

    if (m_buf.size())
    {
        *len = m_buf.read(buf, DATA_LEN_MAX_TX);
        if (m_buf.size() < (USB_BUF_LEN - USB_PACKET_SIZE))
        {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
            PFIC_EnableIRQ(USB_IRQn);
        }
        return 0x00;
    }

    *len = 0;
    return 0xFF;
}
