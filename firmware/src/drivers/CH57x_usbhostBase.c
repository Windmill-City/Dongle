#include "CH57x_common.h"
#if DISK_LIB_ENABLE
  #include "CHRV3UFI.H"
#endif

uint8_t       UsbDevEndp0Size;
uint8_t       FoundNewDev;
_RootHubDev   ThisUsbDev;
_DevOnHubPort DevOnHubPort[HUB_MAX_PORTS];

uint8_t *pHOST_RX_RAM_Addr;
uint8_t *pHOST_TX_RAM_Addr;

__attribute__((aligned(4))) const uint8_t SetupGetDevDescr[] = {USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00,
                                                                USB_DESCR_TYP_DEVICE, 0x00, 0x00, sizeof(USB_DEV_DESCR), 0x00};

__attribute__((aligned(4))) const uint8_t SetupGetCfgDescr[] = {USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0x00,
                                                                USB_DESCR_TYP_CONFIG, 0x00, 0x00, 0x04, 0x00};

__attribute__((aligned(4))) const uint8_t SetupSetUsbAddr[] = {USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0x00,
                                                               0x00, 0x00, 0x00, 0x00};

__attribute__((aligned(4))) const uint8_t SetupSetUsbConfig[] = {USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00,
                                                                 0x00, 0x00, 0x00};

__attribute__((aligned(4))) const uint8_t SetupSetUsbInterface[] = {USB_REQ_RECIP_INTERF, USB_SET_INTERFACE, 0x00, 0x00,
                                                                    0x00, 0x00, 0x00, 0x00};

__attribute__((aligned(4))) const uint8_t SetupClrEndpStall[] = {USB_REQ_TYP_OUT | USB_REQ_RECIP_ENDP, USB_CLEAR_FEATURE,
                                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void DisableRootHubPort(void)
{
#ifdef FOR_ROOT_UDISK_ONLY
    CHRV3DiskStatus = DISK_DISCONNECT;
#endif
#ifndef DISK_BASE_BUF_LEN
    ThisUsbDev.DeviceStatus = ROOT_DEV_DISCONNECT;
    ThisUsbDev.DeviceAddress = 0x00;
#endif
}

uint8_t AnalyzeRootHub(void)
{
    uint8_t s;

    s = ERR_SUCCESS;

    if(R8_USB_MIS_ST & RB_UMS_DEV_ATTACH)
    {
#ifdef DISK_BASE_BUF_LEN
        if(CHRV3DiskStatus == DISK_DISCONNECT
#else
        if(ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT
#endif
           || (R8_UHOST_CTRL & RB_UH_PORT_EN) == 0x00)
        {
            DisableRootHubPort();
#ifdef DISK_BASE_BUF_LEN
            CHRV3DiskStatus = DISK_CONNECT;
#else
            ThisUsbDev.DeviceSpeed = R8_USB_MIS_ST & RB_UMS_DM_LEVEL ? 0 : 1;
            ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;
#endif
            PRINT("USB dev in\n");
            s = ERR_USB_CONNECT;
        }
    }

#ifdef DISK_BASE_BUF_LEN
    else if(CHRV3DiskStatus >= DISK_CONNECT)
    {
#else
    else if(ThisUsbDev.DeviceStatus >= ROOT_DEV_CONNECTED)
    {
#endif
        DisableRootHubPort();
        PRINT("USB dev out\n");
        if(s == ERR_SUCCESS)
        {
            s = ERR_USB_DISCON;
        }
    }

    return (s);
}

void SetHostUsbAddr(uint8_t addr)
{
    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | (addr & MASK_USB_ADDR);
}

void SetUsbSpeed(uint8_t FullSpeed)
{
#ifndef DISK_BASE_BUF_LEN
    if(FullSpeed)
    {
        R8_USB_CTRL &= ~RB_UC_LOW_SPEED;
        R8_UH_SETUP &= ~RB_UH_PRE_PID_EN;
    }
    else
    {
        R8_USB_CTRL |= RB_UC_LOW_SPEED;
    }
#endif
    (void)FullSpeed;
}

void ResetRootHubPort(void)
{
    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;
    SetHostUsbAddr(0x00);
    R8_UHOST_CTRL &= ~RB_UH_PORT_EN;
    SetUsbSpeed(1);
    R8_UHOST_CTRL = (R8_UHOST_CTRL & ~RB_UH_LOW_SPEED) | RB_UH_BUS_RESET;
    mDelaymS(15);
    R8_UHOST_CTRL = R8_UHOST_CTRL & ~RB_UH_BUS_RESET;
    mDelayuS(250);
    R8_USB_INT_FG = RB_UIF_DETECT;
}

uint8_t EnableRootHubPort(void)
{
#ifdef DISK_BASE_BUF_LEN
    if(CHRV3DiskStatus < DISK_CONNECT)
        CHRV3DiskStatus = DISK_CONNECT;
#else
    if(ThisUsbDev.DeviceStatus < ROOT_DEV_CONNECTED)
        ThisUsbDev.DeviceStatus = ROOT_DEV_CONNECTED;
#endif
    if(R8_USB_MIS_ST & RB_UMS_DEV_ATTACH)
    {
#ifndef DISK_BASE_BUF_LEN
        if((R8_UHOST_CTRL & RB_UH_PORT_EN) == 0x00)
        {
            ThisUsbDev.DeviceSpeed = (R8_USB_MIS_ST & RB_UMS_DM_LEVEL) ? 0 : 1;
            if(ThisUsbDev.DeviceSpeed == 0)
            {
                R8_UHOST_CTRL |= RB_UH_LOW_SPEED;
            }
        }
#endif
        R8_UHOST_CTRL |= RB_UH_PORT_EN;
        return (ERR_SUCCESS);
    }
    return (ERR_USB_DISCON);
}

#ifndef DISK_BASE_BUF_LEN

void SelectHubPort(uint8_t HubPortIndex)
{
    if(HubPortIndex)
    {
        SetHostUsbAddr(DevOnHubPort[HubPortIndex - 1].DeviceAddress);
        SetUsbSpeed(DevOnHubPort[HubPortIndex - 1].DeviceSpeed);
        if(DevOnHubPort[HubPortIndex - 1].DeviceSpeed == 0)
        {
            R8_UEP1_CTRL |= RB_UH_PRE_PID_EN;
            mDelayuS(100);
        }
    }
    else
    {
        SetHostUsbAddr(ThisUsbDev.DeviceAddress);
        SetUsbSpeed(ThisUsbDev.DeviceSpeed);
    }
}
#endif

uint8_t WaitUSB_Interrupt(void)
{
    uint16_t i;
    for(i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB_INT_FG & RB_UIF_TRANSFER) == 0; i--)
    {
        ;
    }
    return ((R8_USB_INT_FG & RB_UIF_TRANSFER) ? ERR_SUCCESS : ERR_USB_UNKNOWN);
}

uint8_t USBHostTransact(uint8_t endp_pid, uint8_t tog, uint32_t timeout)
{
    uint8_t TransRetry;

    uint8_t  s, r;
    uint16_t i;

    R8_UH_RX_CTRL = R8_UH_TX_CTRL = tog;
    TransRetry = 0;

    do
    {
        R8_UH_EP_PID = endp_pid;
        R8_USB_INT_FG = RB_UIF_TRANSFER;
        for(i = WAIT_USB_TOUT_200US; i != 0 && (R8_USB_INT_FG & RB_UIF_TRANSFER) == 0; i--)
        {
            DelayUs(1);
        }
        R8_UH_EP_PID = 0x00;
        if((R8_USB_INT_FG & RB_UIF_TRANSFER) == 0)
        {
            return (ERR_USB_UNKNOWN);
        }

        if(R8_USB_INT_FG & RB_UIF_DETECT)
        {

            R8_USB_INT_FG = RB_UIF_DETECT;
            s = AnalyzeRootHub();

            if(s == ERR_USB_CONNECT)
                FoundNewDev = 1;
#ifdef DISK_BASE_BUF_LEN
            if(CHRV3DiskStatus == DISK_DISCONNECT)
            {
                return (ERR_USB_DISCON);
            }
            if(CHRV3DiskStatus == DISK_CONNECT)
            {
                return (ERR_USB_CONNECT);
            }
#else
            if(ThisUsbDev.DeviceStatus == ROOT_DEV_DISCONNECT)
            {
                return (ERR_USB_DISCON);
            }
            if(ThisUsbDev.DeviceStatus == ROOT_DEV_CONNECTED)
            {
                return (ERR_USB_CONNECT);
            }
#endif
            mDelayuS(200);
        }

        if(R8_USB_INT_FG & RB_UIF_TRANSFER)
        {
            if(R8_USB_INT_ST & RB_UIS_TOG_OK)
            {
                return (ERR_SUCCESS);
            }
            r = R8_USB_INT_ST & MASK_UIS_H_RES;
            if(r == USB_PID_STALL)
            {
                return (r | ERR_USB_TRANSFER);
            }
            if(r == USB_PID_NAK)
            {
                if(timeout == 0)
                {
                    return (r | ERR_USB_TRANSFER);
                }
                if(timeout < 0xFFFFFFFF)
                {
                    timeout--;
                }
                --TransRetry;
            }
            else
                switch(endp_pid >> 4)
                {
                    case USB_PID_SETUP:
                    case USB_PID_OUT:
                        if(r)
                        {
                            return (r | ERR_USB_TRANSFER);
                        }
                        break;
                    case USB_PID_IN:
                        if(r == USB_PID_DATA0 || r == USB_PID_DATA1)
                        {
                        }
                        else if(r)
                        {
                            return (r | ERR_USB_TRANSFER);
                        }
                        break;
                    default:
                        return (ERR_USB_UNKNOWN);
                        break;
                }
        }
        else
        {
            R8_USB_INT_FG = 0xFF;
        }
        mDelayuS(15);
    } while(++TransRetry < 3);
    return (ERR_USB_TRANSFER);
}

uint8_t HostCtrlTransfer(uint8_t *DataBuf, uint8_t *RetLen)
{
    uint16_t RemLen = 0;
    uint8_t  s, RxLen, RxCnt, TxCnt;
    uint8_t *pBuf;
    uint8_t *pLen;

    pBuf = DataBuf;
    pLen = RetLen;
    mDelayuS(200);
    if(pLen)
    {
        *pLen = 0;
    }

    R8_UH_TX_LEN = sizeof(USB_SETUP_REQ);
    s = USBHostTransact(USB_PID_SETUP << 4 | 0x00, 0x00, 200000 / 20);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    R8_UH_RX_CTRL = R8_UH_TX_CTRL = RB_UH_R_TOG | RB_UH_R_AUTO_TOG | RB_UH_T_TOG | RB_UH_T_AUTO_TOG;
    R8_UH_TX_LEN = 0x01;
    RemLen = pSetupReq->wLength;
    PRINT("wLength: %x\n", RemLen);
    if(RemLen && pBuf)
    {
        PRINT("bRequestType: %x\n", pSetupReq->bRequestType);
        if(pSetupReq->bRequestType & USB_REQ_TYP_IN)
        {
            while(RemLen)
            {
                mDelayuS(200);
                s = USBHostTransact(USB_PID_IN << 4 | 0x00, R8_UH_RX_CTRL, 200000 / 20);
                if(s != ERR_SUCCESS)
                {
                    return (s);
                }
                RxLen = R8_USB_RX_LEN < RemLen ? R8_USB_RX_LEN : RemLen;
                RemLen -= RxLen;
                if(pLen)
                {
                    *pLen += RxLen;
                }
                for(RxCnt = 0; RxCnt != RxLen; RxCnt++)
                {
                    *pBuf = pHOST_RX_RAM_Addr[RxCnt];
                    pBuf++;
                }
                if(R8_USB_RX_LEN == 0 || (R8_USB_RX_LEN & (UsbDevEndp0Size - 1)))
                {
                    break;
                }
            }
            R8_UH_TX_LEN = 0x00;
        }
        else
        {
            while(RemLen)
            {
                mDelayuS(200);
                R8_UH_TX_LEN = RemLen >= UsbDevEndp0Size ? UsbDevEndp0Size : RemLen;
                for(TxCnt = 0; TxCnt != R8_UH_TX_LEN; TxCnt++)
                {
                    pHOST_TX_RAM_Addr[TxCnt] = *pBuf;
                    pBuf++;
                }
                s = USBHostTransact(USB_PID_OUT << 4 | 0x00, R8_UH_TX_CTRL, 200000 / 20);
                if(s != ERR_SUCCESS)
                {
                    return (s);
                }
                RemLen -= R8_UH_TX_LEN;
                if(pLen)
                {
                    *pLen += R8_UH_TX_LEN;
                }
            }
            PRINT("Send: %d\n", *pLen);

        }
    }
    mDelayuS(200);
    s = USBHostTransact((R8_UH_TX_LEN ? USB_PID_IN << 4 | 0x00 : USB_PID_OUT << 4 | 0x00), RB_UH_R_TOG | RB_UH_T_TOG, 200000 / 20);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    if(R8_UH_TX_LEN == 0)
    {
        return (ERR_SUCCESS);
    }
    if(R8_USB_RX_LEN == 0)
    {
        return (ERR_SUCCESS);
    }
    return (ERR_USB_BUF_OVER);
}

void CopySetupReqPkg(const uint8_t *pReqPkt)
{
    uint8_t i;
    for(i = 0; i != sizeof(USB_SETUP_REQ); i++)
    {
        ((uint8_t *)pSetupReq)[i] = *pReqPkt;
        pReqPkt++;
    }
}

uint8_t CtrlGetDeviceDescr(void)
{
    uint8_t s;
    uint8_t len;

    UsbDevEndp0Size = DEFAULT_ENDP0_SIZE;
    CopySetupReqPkg(SetupGetDevDescr);
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    UsbDevEndp0Size = ((PUSB_DEV_DESCR)Com_Buffer)->bMaxPacketSize0;
    if(len < ((PUSB_SETUP_REQ)SetupGetDevDescr)->wLength)
    {
        return (ERR_USB_BUF_OVER);
    }
    return (ERR_SUCCESS);
}

uint8_t CtrlGetConfigDescr(void)
{
    uint8_t s;
    uint8_t len;

    CopySetupReqPkg(SetupGetCfgDescr);
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    if(len < ((PUSB_SETUP_REQ)SetupGetCfgDescr)->wLength)
    {
        return (ERR_USB_BUF_OVER);
    }

    len = ((PUSB_CFG_DESCR)Com_Buffer)->wTotalLength;
    CopySetupReqPkg(SetupGetCfgDescr);
    pSetupReq->wLength = len;
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }

#ifdef DISK_BASE_BUF_LEN
    if(len > 64)
        len = 64;
    memcpy(TxBuffer, Com_Buffer, len);
#endif

    return (ERR_SUCCESS);
}

uint8_t CtrlSetUsbAddress(uint8_t addr)
{
    uint8_t s;

    CopySetupReqPkg(SetupSetUsbAddr);
    pSetupReq->wValue = addr;
    s = HostCtrlTransfer(NULL, NULL);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    SetHostUsbAddr(addr);
    mDelaymS(10);
    return (ERR_SUCCESS);
}

uint8_t CtrlSetUsbConfig(uint8_t cfg)
{
    CopySetupReqPkg(SetupSetUsbConfig);
    pSetupReq->wValue = cfg;
    return (HostCtrlTransfer(NULL, NULL));
}

uint8_t CtrlClearEndpStall(uint8_t endp)
{
    CopySetupReqPkg(SetupClrEndpStall);
    pSetupReq->wIndex = endp;
    return (HostCtrlTransfer(NULL, NULL));
}

uint8_t CtrlSetUsbIntercace(uint8_t cfg)
{
    CopySetupReqPkg(SetupSetUsbInterface);
    pSetupReq->wValue = cfg;
    return (HostCtrlTransfer(NULL, NULL));
}

void USB_HostInit(void)
{
    R8_USB_CTRL = RB_UC_HOST_MODE;
    R8_UHOST_CTRL = 0;
    R8_USB_DEV_AD = 0x00;

    R8_UH_EP_MOD = RB_UH_EP_TX_EN | RB_UH_EP_RX_EN;
    R16_UH_RX_DMA = (uint32_t)pHOST_RX_RAM_Addr;
    R16_UH_TX_DMA = (uint32_t)pHOST_TX_RAM_Addr;

    R8_UH_RX_CTRL = 0x00;
    R8_UH_TX_CTRL = 0x00;
    R8_USB_CTRL = RB_UC_HOST_MODE | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_UH_SETUP = RB_UH_SOF_EN;
    R8_USB_INT_FG = 0xFF;
    DisableRootHubPort();
    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_DETECT;

    FoundNewDev = 0;
}
