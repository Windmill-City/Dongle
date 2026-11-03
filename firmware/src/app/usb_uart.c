#include "ch57x_common.h"
#include "usb_uart.h"
#include "rf_uart_rx.h"
#include "rf.h"
#include "buf.h"

#define THIS_ENDP0_SIZE       64
#define MAX_PACKET_SIZE       64

#define DEF_IC_PRG_VER        0x31

#define CH341_REG_NUM         10

#define USB_BUF_LEN           512

#define USB_IRQ_FLAG_NUM      4

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

static const uint8_t usb_manufacturer_str[] = "wch.cn";
static const uint8_t usb_product_str[] = "USB2.0 To Serial Port";
static const uint8_t usb_serial_str[] = "WCH454545TS2";

typedef struct
{
    uint8_t config;
    uint8_t address;
    uint8_t flag;
} usb_dev_info_t;

typedef struct
{
    uint8_t request_type;
    uint8_t request;
    uint8_t value_l;
    uint8_t value_h;
    uint8_t index_l;
    uint8_t index_h;
    uint8_t length_l;
    uint8_t length_h;
} usb_setup_req_t;

#define USB_SETUP_BUF     ((usb_setup_req_t *)g_ep0_buffer)

line_code_t uart0_para;

static usb_dev_info_t g_dev_info;

static uint8_t g_setup_req_code;
static uint8_t g_setup_len;
static const uint8_t *g_descr_ptr;

static __aligned(4) uint8_t g_ep0_buffer[MAX_PACKET_SIZE];
static __aligned(4) uint8_t g_ep1_buffer[MAX_PACKET_SIZE];
static __aligned(4) uint8_t g_ep2_buffer[2 * MAX_PACKET_SIZE];
static __aligned(4) uint8_t g_ep1_out_buffer[MAX_PACKET_SIZE];
static uint8_t g_ep0_send_buf[256];

static uint8_t g_ch341_reg_add[CH341_REG_NUM];
static uint8_t g_ch341_reg_val[CH341_REG_NUM];

static uint8_t g_uart_status;
static uint32_t g_end2_data_len;

static uint8_t g_usb_irq_w_idx;
static uint8_t g_usb_irq_r_idx;

static volatile uint8_t g_usb_irq_len[USB_IRQ_FLAG_NUM];
static volatile uint8_t g_usb_irq_pid[USB_IRQ_FLAG_NUM];
static volatile uint8_t g_usb_irq_flag[USB_IRQ_FLAG_NUM];

static struct byte_buf *g_usb_buf_mgr;
static struct byte_buf g_usb_buf_struct;
static uint8_t g_usb_buf_pool[USB_BUF_LEN];

static void usb_buffer_create(struct byte_buf **buf)
{
    *buf = byte_buf_create(&g_usb_buf_struct, g_usb_buf_pool, sizeof(g_usb_buf_pool));
}

static void ch341_reg_write(uint8_t reg_add, uint8_t reg_val)
{
    uint8_t find_idx = 0;
    uint8_t find_flag = 0;
    uint8_t i;

    for (i = 0; i < CH341_REG_NUM; i++)
    {
        if (g_ch341_reg_add[i] == reg_add)
        {
            find_flag = 1;
            break;
        }
        if (g_ch341_reg_add[i] == 0xFF)
        {
            break;
        }
    }
    find_idx = i;

    if (find_flag)
    {
        g_ch341_reg_val[find_idx] = reg_val;
    }
    else
    {
        g_ch341_reg_add[find_idx] = reg_add;
        g_ch341_reg_val[find_idx] = reg_val;
    }

    if (reg_add == 0x18)
    {
        uint8_t reg_uart_ctrl = reg_val;
        uint8_t data_bit_val;
        uint8_t stop_bit_val;
        uint8_t parity_val;

        data_bit_val = reg_uart_ctrl & 0x03;
        if (data_bit_val == 0x00) data_bit_val = HAL_UART_5_BITS_PER_CHAR;
        else if (data_bit_val == 0x01) data_bit_val = HAL_UART_6_BITS_PER_CHAR;
        else if (data_bit_val == 0x02) data_bit_val = HAL_UART_7_BITS_PER_CHAR;
        else data_bit_val = HAL_UART_8_BITS_PER_CHAR;

        stop_bit_val = (reg_uart_ctrl & 0x04) ?
                       HAL_UART_TWO_STOP_BITS : HAL_UART_ONE_STOP_BIT;

        parity_val = reg_uart_ctrl & 0x38;
        if (parity_val == 0x00) parity_val = HAL_UART_NO_PARITY;
        else if (parity_val == 0x08) parity_val = HAL_UART_ODD_PARITY;
        else if (parity_val == 0x18) parity_val = HAL_UART_EVEN_PARITY;
        else if (parity_val == 0x28) parity_val = HAL_UART_MARK_PARITY;
        else parity_val = HAL_UART_SPACE_PARITY;

        uart0_para.stop_bits = stop_bit_val;
        uart0_para.parity_type = parity_val;
        uart0_para.data_bits = data_bit_val;
    }
}

static uint8_t ch341_reg_read(uint8_t reg_add, uint8_t *reg_val)
{
    uint8_t find_flag = 0;
    uint8_t i;

    *reg_val = 0;
    for (i = 0; i < CH341_REG_NUM; i++)
    {
        if (g_ch341_reg_add[i] == reg_add)
        {
            find_flag = 1;
            *reg_val = g_ch341_reg_val[i];
            break;
        }
        if (g_ch341_reg_add[i] == 0xFF)
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

static uint32_t ch341_calc_baud(uint8_t reg1, uint8_t reg2)
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

static void usb_ep_set_in_status(uint8_t ep_num, uint8_t type, uint8_t sta)
{
    uint8_t *p_uepn_ctrl;

    p_uepn_ctrl = (uint8_t *)(USB_BASE_ADDR + 0x22 + ep_num * 4);
    if (type == ENDP_TYPE_IN)
    {
        *((PUINT8V)p_uepn_ctrl) = (*((PUINT8V)p_uepn_ctrl) & ~0x03) | sta;
    }
    else
    {
        *((PUINT8V)p_uepn_ctrl) = (*((PUINT8V)p_uepn_ctrl) & ~(0x03 << 2)) | (sta << 2);
    }
}

__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
    uint8_t i;

    if (R8_USB_INT_FG & RB_UIF_TRANSFER)
    {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            g_usb_irq_flag[g_usb_irq_w_idx] = 1;
            g_usb_irq_pid[g_usb_irq_w_idx] = R8_USB_INT_ST;
            g_usb_irq_len[g_usb_irq_w_idx] = R8_USB_RX_LEN;

            switch (g_usb_irq_pid[g_usb_irq_w_idx] & 0x3F)
            {
                case UIS_TOKEN_OUT | 2:
                {
                    if (R8_USB_INT_FG & RB_U_TOG_OK)
                    {
                        buf_size_t data_len;

                        R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        data_len = g_usb_irq_len[g_usb_irq_w_idx];
                        g_end2_data_len = byte_buf_write(g_usb_buf_mgr, g_ep2_buffer, &data_len);
                        if (g_end2_data_len > (USB_BUF_LEN - MAX_PACKET_SIZE))
                        {
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_NAK;
                        }
                    }
                    else
                    {
                        g_usb_irq_flag[g_usb_irq_w_idx] = 0;
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

                        for (i = 0; i < (MAX_PACKET_SIZE / 4); i++)
                        {
                            ((UINT32 *)g_ep1_out_buffer)[i] = ((UINT32 *)g_ep1_buffer)[i];
                        }
                    }
                    else
                    {
                        g_usb_irq_flag[g_usb_irq_w_idx] = 0;
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
                        g_usb_irq_flag[g_usb_irq_w_idx] = 0;
                    }
                } break;

                case UIS_TOKEN_IN | 0:
                {
                    R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xFC) | IN_NAK;
                } break;
            }

            if (g_usb_irq_flag[g_usb_irq_w_idx])
            {
                g_usb_irq_w_idx++;
                if (g_usb_irq_w_idx >= USB_IRQ_FLAG_NUM) g_usb_irq_w_idx = 0;
            }

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            g_usb_irq_flag[g_usb_irq_w_idx] = 1;
            g_usb_irq_pid[g_usb_irq_w_idx] = UIS_TOKEN_SETUP | 0;
            g_usb_irq_len[g_usb_irq_w_idx] = 8;

            g_usb_irq_w_idx++;
            if (g_usb_irq_w_idx >= USB_IRQ_FLAG_NUM) g_usb_irq_w_idx = 0;

            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if (R8_USB_INT_FG & RB_UIF_BUS_RST)
    {
        R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

        R8_USB_DEV_AD = 0x00;
        g_dev_info.address = 0;

        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if (R8_USB_INT_FG & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}

static void usb_build_string_desc(uint8_t index)
{
    const uint8_t *str;
    uint8_t str_len;
    uint8_t *p;
    uint8_t i;

    if (index == 1) str = usb_manufacturer_str;
    else if (index == 2) str = usb_product_str;
    else str = usb_serial_str;

    str_len = (uint8_t)strlen((char *)str);
    p = g_ep0_send_buf;
    *p++ = str_len * 2 + 2;
    *p++ = 0x03;
    for (i = 0; i < str_len; i++)
    {
        *p++ = str[i];
        *p++ = 0x00;
    }

    g_descr_ptr = g_ep0_send_buf;
}

static void usb_handle_setup(uint8_t len)
{
    usb_setup_req_t *setup = USB_SETUP_BUF;
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

    g_setup_len = setup->length_l;
    if (setup->length_h) g_setup_len = 0xFF;

    len = 0;
    g_setup_req_code = setup->request;

    data_dir = USB_REQ_TYP_OUT;
    if (setup->request_type & USB_REQ_TYP_IN) data_dir = USB_REQ_TYP_IN;

    if ((setup->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_VENDOR)
    {
        switch (g_setup_req_code)
        {
            case DEF_VEN_DEBUG_WRITE:
            {
                uint8_t reg_add1 = g_ep0_buffer[2];
                uint8_t reg_add2 = g_ep0_buffer[3];
                uint8_t reg_val1 = g_ep0_buffer[4];
                uint8_t reg_val2 = g_ep0_buffer[5];

                if ((reg_add1 == 0x12) && (reg_add2 == 0x13))
                {
                    uart0_para.baud_rate = ch341_calc_baud(setup->index_l, setup->index_h);
                    g_uart_status = 1;
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
                g_setup_len = len;
                memcpy(g_ep0_buffer, buf, len);
            } break;

            case DEF_VEN_UART_INIT:
            {
                uint8_t reg_uart_ctrl;
                uint8_t data_bit_val;
                uint8_t stop_bit_val;
                uint8_t parity_val;
                uint8_t uart_reg1_val;
                uint8_t uart_reg2_val;

                if (g_ep0_buffer[2] & 0x80)
                {
                    reg_uart_ctrl = g_ep0_buffer[3];

                    data_bit_val = reg_uart_ctrl & 0x03;
                    if (data_bit_val == 0x00) data_bit_val = HAL_UART_5_BITS_PER_CHAR;
                    else if (data_bit_val == 0x01) data_bit_val = HAL_UART_6_BITS_PER_CHAR;
                    else if (data_bit_val == 0x02) data_bit_val = HAL_UART_7_BITS_PER_CHAR;
                    else data_bit_val = HAL_UART_8_BITS_PER_CHAR;

                    stop_bit_val = (reg_uart_ctrl & 0x04) ?
                                   HAL_UART_TWO_STOP_BITS : HAL_UART_ONE_STOP_BIT;

                    parity_val = reg_uart_ctrl & 0x38;
                    if (parity_val == 0x00) parity_val = HAL_UART_NO_PARITY;
                    else if (parity_val == 0x08) parity_val = HAL_UART_ODD_PARITY;
                    else if (parity_val == 0x18) parity_val = HAL_UART_EVEN_PARITY;
                    else if (parity_val == 0x28) parity_val = HAL_UART_MARK_PARITY;
                    else parity_val = HAL_UART_SPACE_PARITY;

                    uart0_para.stop_bits = stop_bit_val;
                    uart0_para.parity_type = parity_val;
                    uart0_para.data_bits = data_bit_val;

                    uart_reg1_val = setup->index_l;
                    uart_reg2_val = setup->index_h;
                    if (!(uart_reg1_val & (1 << 6)))
                    {
                        uart_reg1_val &= 0xC7;
                    }

                    uart0_para.baud_rate = ch341_calc_baud(uart_reg1_val, uart_reg2_val);
                    g_uart_status = 1;
                }
            } break;

            case DEF_VEN_UART_M_OUT:
            {
                uart0_para.io_status = g_ep0_buffer[2];
                g_uart_status = 1;
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
                g_setup_len = len;
                memcpy(g_ep0_buffer, buf, len);
            } break;

            default:
                break;
        }
    }
    else if ((setup->request_type & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
    {
        switch (g_setup_req_code)
        {
            case USB_GET_DESCRIPTOR:
            {
                switch (setup->value_h)
                {
                    case 1:
                        memcpy(g_ep0_send_buf, usb_vendor_dev_desc, sizeof(usb_vendor_dev_desc));
                        g_descr_ptr = g_ep0_send_buf;
                        len = sizeof(usb_vendor_dev_desc);
                        break;

                    case 2:
                        memcpy(g_ep0_send_buf, usb_vendor_cfg_desc, sizeof(usb_vendor_cfg_desc));
                        g_descr_ptr = g_ep0_send_buf;
                        len = sizeof(usb_vendor_cfg_desc);
                        break;

                    case 3:
                        switch (setup->value_l)
                        {
                            case 0:
                                g_descr_ptr = usb_lang_str_desc;
                                len = sizeof(usb_lang_str_desc);
                                break;
                            case 1:
                            case 2:
                            case 3:
                                usb_build_string_desc(setup->value_l);
                                len = g_ep0_send_buf[0];
                                break;
                            default:
                                len = 0xFF;
                                break;
                        }
                        break;

                    case 6:
                        g_descr_ptr = usb_qualifier_desc;
                        len = sizeof(usb_qualifier_desc);
                        break;

                    default:
                        len = 0xFF;
                        break;
                }

                if (len != 0xFF)
                {
                    if (g_setup_len > len) g_setup_len = len;
                    len = (g_setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : g_setup_len;
                    memcpy(g_ep0_buffer, g_descr_ptr, len);
                    g_setup_len -= len;
                    g_descr_ptr += len;
                }
            } break;

            case USB_SET_ADDRESS:
            {
                g_dev_info.flag |= DEF_BIT_USB_ADDRESS;
                g_dev_info.address = setup->value_l;
            } break;

            case USB_GET_CONFIGURATION:
            {
                g_ep0_buffer[0] = g_dev_info.config;
                if (g_setup_len >= 1) len = 1;
            } break;

            case USB_SET_CONFIGURATION:
            {
                g_dev_info.config = setup->value_l;
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
                g_ep0_buffer[0] = 0x00;
                if (g_setup_len >= 1) len = 1;
            } break;

            case USB_GET_STATUS:
            {
                g_ep0_buffer[0] = 0x00;
                g_ep0_buffer[1] = 0x00;
                len = (g_setup_len >= 2) ? 2 : g_setup_len;
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
            switch (g_setup_req_code)
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
            switch (g_setup_req_code)
            {
                case DEF_GET_LINE_CODING:
                {
                    line_code_t *line_code = (line_code_t *)g_ep0_buffer;

                    g_descr_ptr = g_ep0_buffer;
                    len = sizeof(line_code_t);
                    line_code->baud_rate = uart0_para.baud_rate;
                    line_code->stop_bits = uart0_para.stop_bits;
                    line_code->parity_type = uart0_para.parity_type;
                    line_code->data_bits = uart0_para.data_bits;
                } break;

                case DEF_SERIAL_STATE:
                {
                    len = 2;
                    g_ep0_buffer[0] = 0;
                    g_ep0_buffer[1] = 0;
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
        g_setup_req_code = 0xFF;
        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
        PFIC_EnableIRQ(USB_IRQn);
    }
    else if (len <= THIS_ENDP0_SIZE)
    {
        if (g_setup_req_code == USB_GET_DESCRIPTOR)
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

static void usb_irq_process(void)
{
    uint8_t len;
    uint8_t i;

    i = g_usb_irq_r_idx;
    g_usb_irq_r_idx++;
    if (g_usb_irq_r_idx >= USB_IRQ_FLAG_NUM) g_usb_irq_r_idx = 0;

    switch (g_usb_irq_pid[i] & 0x3F)
    {
        case UIS_TOKEN_OUT | 1:
        {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP1_CTRL = R8_UEP1_CTRL & 0xF3;
            PFIC_EnableIRQ(USB_IRQn);
        } break;

        case UIS_TOKEN_SETUP | 0:
        {
            len = g_usb_irq_len[i];
            usb_handle_setup(len);
        } break;

        case UIS_TOKEN_IN | 0:
        {
            switch (g_setup_req_code)
            {
                case USB_GET_DESCRIPTOR:
                {
                    len = (g_setup_len >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : g_setup_len;
                    memcpy(g_ep0_buffer, g_descr_ptr, len);
                    g_setup_len -= len;
                    g_descr_ptr += len;

                    if (len)
                    {
                        R8_UEP0_T_LEN = len;
                        PFIC_DisableIRQ(USB_IRQn);
                        R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                        usb_ep_set_in_status(ENDP0, ENDP_TYPE_IN, IN_ACK);
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
                    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | g_dev_info.address;
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
            len = g_usb_irq_len[i];
            if (len && (g_setup_req_code == DEF_SET_LINE_CODING))
            {
                uint32_t baud_rate;

                memcpy(&baud_rate, g_ep0_buffer, 4);
                uart0_para.baud_rate = baud_rate;
                uart0_para.stop_bits = g_ep0_buffer[4];
                uart0_para.parity_type = g_ep0_buffer[5];
                uart0_para.data_bits = g_ep0_buffer[6];

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

static void usb_vendor_init(void)
{
    R8_USB_CTRL = 0x00;

    R8_UEP4_1_MOD = RB_UEP1_TX_EN | RB_UEP1_RX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN;

    R16_UEP0_DMA = (UINT32)&g_ep0_buffer[0];
    R16_UEP1_DMA = (UINT32)&g_ep1_buffer[0];
    R16_UEP2_DMA = (UINT32)&g_ep2_buffer[0];

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

    g_dev_info.config = 0;
    g_dev_info.address = 0;
    g_dev_info.flag = 0;
}

static void usb_dev_para_init(void)
{
    uint8_t i;

    uart0_para.baud_rate = 115200;
    uart0_para.data_bits = HAL_UART_8_BITS_PER_CHAR;
    uart0_para.parity_type = HAL_UART_NO_PARITY;
    uart0_para.stop_bits = HAL_UART_ONE_STOP_BIT;
    uart0_para.io_status = 0;
    g_uart_status = 1;

    for (i = 0; i < CH341_REG_NUM; i++)
    {
        g_ch341_reg_add[i] = 0xFF;
        g_ch341_reg_val[i] = 0x00;
    }

    for (i = 0; i < USB_IRQ_FLAG_NUM; i++)
    {
        g_usb_irq_flag[i] = 0;
    }
}

void usb_uart_init(void)
{
    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;

    usb_dev_para_init();
    usb_buffer_create(&g_usb_buf_mgr);
    usb_vendor_init();

    PFIC_EnableIRQ(USB_IRQn);
}

__HIGH_CODE
void usb_uart_poll(void)
{
    buf_size_t len;
    uint8_t i;

    i = g_usb_irq_r_idx;
    if (g_usb_irq_flag[i])
    {
        usb_irq_process();
        g_usb_irq_flag[i] = 0;
    }

    if (g_dev_info.address)
    {
        if ((R8_UEP2_CTRL & MASK_UEP_T_RES) == UEP_T_RES_NAK)
        {
            len = MAX_PACKET_SIZE / 2;
            if (rf_uart_rx_read(&g_ep2_buffer[MAX_PACKET_SIZE], &len))
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
uint8_t usb_uart_read(void *buf, buf_size_t *len)
{
    if (g_dev_info.address)
    {
        if (g_uart_status)
        {
            g_uart_status = 0;
            *len = 0;
            return 0x80;
        }
    }

    if (g_end2_data_len)
    {
        *len = DATA_LEN_MAX_TX;
        g_end2_data_len = byte_buf_read(g_usb_buf_mgr, buf, len);
        if (g_end2_data_len < (USB_BUF_LEN - MAX_PACKET_SIZE))
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
