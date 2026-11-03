#ifndef __CH57x_USBDEV_H__
#define __CH57x_USBDEV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DEF_USB_GET_IDLE           0x02
#define DEF_USB_GET_PROTOCOL       0x03
#define DEF_USB_SET_REPORT         0x09
#define DEF_USB_SET_IDLE           0x0A
#define DEF_USB_SET_PROTOCOL       0x0B

extern uint8_t *pEP0_RAM_Addr;
extern uint8_t *pEP1_RAM_Addr;
extern uint8_t *pEP2_RAM_Addr;
extern uint8_t *pEP3_RAM_Addr;

#define pSetupReqPak          ((PUSB_SETUP_REQ)pEP0_RAM_Addr)
#define pEP0_DataBuf          (pEP0_RAM_Addr)
#define pEP1_OUT_DataBuf      (pEP1_RAM_Addr)
#define pEP1_IN_DataBuf       (pEP1_RAM_Addr + 64)
#define pEP2_OUT_DataBuf      (pEP2_RAM_Addr)
#define pEP2_IN_DataBuf       (pEP2_RAM_Addr + 64)
#define pEP3_OUT_DataBuf      (pEP3_RAM_Addr)
#define pEP3_IN_DataBuf       (pEP3_RAM_Addr + 64)
#define pEP4_OUT_DataBuf      (pEP0_RAM_Addr + 64)
#define pEP4_IN_DataBuf       (pEP0_RAM_Addr + 128)

void USB_DeviceInit(void);

void USB_DevTransProcess(void);

void DevEP1_OUT_Deal(uint8_t l);

void DevEP2_OUT_Deal(uint8_t l);

void DevEP3_OUT_Deal(uint8_t l);

void DevEP4_OUT_Deal(uint8_t l);

void DevEP1_IN_Deal(uint8_t l);

void DevEP2_IN_Deal(uint8_t l);

void DevEP3_IN_Deal(uint8_t l);

void DevEP4_IN_Deal(uint8_t l);

#define EP1_GetINSta()    (R8_UEP1_CTRL & UEP_T_RES_NAK)

#define EP2_GetINSta()    (R8_UEP2_CTRL & UEP_T_RES_NAK)

#define EP3_GetINSta()    (R8_UEP3_CTRL & UEP_T_RES_NAK)

#define EP4_GetINSta()    (R8_UEP4_CTRL & UEP_T_RES_NAK)

#define USB_DisablePin()  (R16_PIN_ANALOG_IE &= ~(RB_PIN_USB_IE | RB_PIN_USB_DP_PU))

#define USB_Disable()     (R32_USB_CONTROL = 0)

#ifdef __cplusplus
}
#endif

#endif
