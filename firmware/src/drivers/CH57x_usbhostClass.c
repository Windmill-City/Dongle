#include "CH57x_common.h"
#if DISK_LIB_ENABLE
  #include "CHRV3UFI.H"
#endif

__attribute__((aligned(4))) const uint8_t SetupSetHIDIdle[] = {0x21, HID_SET_IDLE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

__attribute__((aligned(4))) const uint8_t SetupGetHIDDevReport[] = {0x81, USB_GET_DESCRIPTOR, 0x00, USB_DESCR_TYP_REPORT,
                                                                    0x00, 0x00, 0x41, 0x00};

__attribute__((aligned(4))) const uint8_t SetupGetHubDescr[] = {HUB_GET_HUB_DESCRIPTOR, HUB_GET_DESCRIPTOR, 0x00,
                                                                USB_DESCR_TYP_HUB, 0x00, 0x00, sizeof(USB_HUB_DESCR), 0x00};

__attribute__((aligned(4))) uint8_t Com_Buffer[128];

uint8_t AnalyzeHidIntEndp(uint8_t *buf, uint8_t HubPortIndex)
{
    uint8_t i, s, l;
    s = 0;

    if(HubPortIndex)
    {
        memset(DevOnHubPort[HubPortIndex - 1].GpVar, 0, sizeof(DevOnHubPort[HubPortIndex - 1].GpVar));
    }
    else
    {
        memset(ThisUsbDev.GpVar, 0, sizeof(ThisUsbDev.GpVar));
    }

    for(i = 0; i < ((PUSB_CFG_DESCR)buf)->wTotalLength; i += l)
    {
        if(((PUSB_ENDP_DESCR)(buf + i))->bDescriptorType == USB_DESCR_TYP_ENDP
           && (((PUSB_ENDP_DESCR)(buf + i))->bmAttributes & USB_ENDP_TYPE_MASK) == USB_ENDP_TYPE_INTER
           && (((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_DIR_MASK))
        {
            if(HubPortIndex)
            {
                DevOnHubPort[HubPortIndex - 1].GpVar[s] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
            }
            else
            {
                ThisUsbDev.GpVar[s] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
            }
            PRINT("%02x ", (uint16_t)ThisUsbDev.GpVar[s]);
            s++;
            if(s >= 4)
            {
                break;
            }
        }
        l = ((PUSB_ENDP_DESCR)(buf + i))->bLength;
        if(l > 16)
        {
            break;
        }
    }
    PRINT("\n");
    return (s);
}

uint8_t AnalyzeBulkEndp(uint8_t *buf, uint8_t HubPortIndex)
{
    uint8_t i, s1, s2, l;
    s1 = 0;
    s2 = 2;

    if(HubPortIndex)
    {
        memset(DevOnHubPort[HubPortIndex - 1].GpVar, 0, sizeof(DevOnHubPort[HubPortIndex - 1].GpVar));
    }
    else
    {
        memset(ThisUsbDev.GpVar, 0, sizeof(ThisUsbDev.GpVar));
    }

    for(i = 0; i < ((PUSB_CFG_DESCR)buf)->wTotalLength; i += l)
    {
        if((((PUSB_ENDP_DESCR)(buf + i))->bDescriptorType == USB_DESCR_TYP_ENDP)
           && ((((PUSB_ENDP_DESCR)(buf + i))->bmAttributes & USB_ENDP_TYPE_MASK) == USB_ENDP_TYPE_BULK))

        {
            if(HubPortIndex)
            {
                if(((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_DIR_MASK)
                {
                    DevOnHubPort[HubPortIndex - 1].GpVar[s1++] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
                }
                else
                {
                    DevOnHubPort[HubPortIndex - 1].GpVar[s2++] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
                }
            }
            else
            {
                if(((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_DIR_MASK)
                {
                    ThisUsbDev.GpVar[s1++] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
                }
                else
                {
                    ThisUsbDev.GpVar[s2++] = ((PUSB_ENDP_DESCR)(buf + i))->bEndpointAddress & USB_ENDP_ADDR_MASK;
                }
            }

            if(s1 == 2)
            {
                s1 = 1;
            }
            if(s2 == 4)
            {
                s2 = 3;
            }
        }
        l = ((PUSB_ENDP_DESCR)(buf + i))->bLength;
        if(l > 16)
        {
            break;
        }
    }
    return (0);
}

uint8_t InitRootDevice(void)
{
    uint8_t i, s;
    uint8_t cfg, dv_cls, if_cls;

    PRINT("Reset host port\n");
    ResetRootHubPort();
    for(i = 0, s = 0; i < 100; i++)
    {
        mDelaymS(1);
        if(EnableRootHubPort() == ERR_SUCCESS)
        {
            i = 0;
            s++;
            if(s > 100)
            {
                break;
            }
        }
    }
    if(i)
    {
        DisableRootHubPort();
        PRINT("Disable host port because of disconnect\n");
        return (ERR_USB_DISCON);
    }
    SetUsbSpeed(ThisUsbDev.DeviceSpeed);

    PRINT("GetDevDescr: ");
    s = CtrlGetDeviceDescr();
    if(s == ERR_SUCCESS)
    {
        for(i = 0; i < ((PUSB_SETUP_REQ)SetupGetDevDescr)->wLength; i++)
        {
            PRINT("x%02X ", (uint16_t)(Com_Buffer[i]));
        }
        PRINT("\n");

        ThisUsbDev.DeviceVID = ((PUSB_DEV_DESCR)Com_Buffer)->idVendor;
        ThisUsbDev.DevicePID = ((PUSB_DEV_DESCR)Com_Buffer)->idProduct;
        dv_cls = ((PUSB_DEV_DESCR)Com_Buffer)->bDeviceClass;

        s = CtrlSetUsbAddress(((PUSB_SETUP_REQ)SetupSetUsbAddr)->wValue);
        if(s == ERR_SUCCESS)
        {
            ThisUsbDev.DeviceAddress = ((PUSB_SETUP_REQ)SetupSetUsbAddr)->wValue;

            PRINT("GetCfgDescr: ");
            s = CtrlGetConfigDescr();
            if(s == ERR_SUCCESS)
            {
                for(i = 0; i < ((PUSB_CFG_DESCR)Com_Buffer)->wTotalLength; i++)
                {
                    PRINT("x%02X ", (uint16_t)(Com_Buffer[i]));
                }
                PRINT("\n");

                cfg = ((PUSB_CFG_DESCR)Com_Buffer)->bConfigurationValue;
                if_cls = ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceClass;

                if((dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_STORAGE))
                {
#ifdef FOR_ROOT_UDISK_ONLY
                    CHRV3DiskStatus = DISK_USB_ADDR;
                    return (ERR_SUCCESS);
                }
                else
                {
                    return (ERR_USB_UNSUPPORT);
                }
#else
                    s = CtrlSetUsbConfig(cfg);
                    if(s == ERR_SUCCESS)
                    {
                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsbDev.DeviceType = USB_DEV_CLASS_STORAGE;
                        PRINT("USB-Disk Ready\n");
                        SetUsbSpeed(1);
                        return (ERR_SUCCESS);
                    }
                }
                else if((dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_PRINTER) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass == 0x01)
                {
                    s = CtrlSetUsbConfig(cfg);
                    if(s == ERR_SUCCESS)
                    {

                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsbDev.DeviceType = USB_DEV_CLASS_PRINTER;
                        PRINT("USB-Print Ready\n");
                        SetUsbSpeed(1);
                        return (ERR_SUCCESS);
                    }
                }
                else if((dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01)
                {

                    s = AnalyzeHidIntEndp(Com_Buffer, 0);
                    PRINT("AnalyzeHidIntEndp %02x\n", (uint16_t)s);

                    if_cls = ((PUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceProtocol;
                    s = CtrlSetUsbConfig(cfg);
                    if(s == ERR_SUCCESS)
                    {
                        s = CtrlGetHIDDeviceReport(dv_cls);
                        if(s == ERR_SUCCESS)
                        {
                            for(i = 0; i < 64; i++)
                            {
                                PRINT("x%02X ", (uint16_t)(Com_Buffer[i]));
                            }
                            PRINT("\n");
                        }

                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        if(if_cls == 1)
                        {
                            ThisUsbDev.DeviceType = DEV_TYPE_KEYBOARD;

                            PRINT("USB-Keyboard Ready\n");
                            SetUsbSpeed(1);
                            return (ERR_SUCCESS);
                        }
                        else if(if_cls == 2)
                        {
                            ThisUsbDev.DeviceType = DEV_TYPE_MOUSE;

                            PRINT("USB-Mouse Ready\n");
                            SetUsbSpeed(1);
                            return (ERR_SUCCESS);
                        }
                        s = ERR_USB_UNSUPPORT;
                    }
                }
                else if(dv_cls == USB_DEV_CLASS_HUB)
                {
                    s = CtrlGetHubDescr();
                    if(s == ERR_SUCCESS)
                    {
                        PRINT("Max Port:%02X ", (((PXUSB_HUB_DESCR)Com_Buffer)->bNbrPorts));
                        ThisUsbDev.GpHUBPortNum = ((PXUSB_HUB_DESCR)Com_Buffer)->bNbrPorts;
                        if(ThisUsbDev.GpHUBPortNum > HUB_MAX_PORTS)
                        {
                            ThisUsbDev.GpHUBPortNum = HUB_MAX_PORTS;
                        }
                        s = CtrlSetUsbConfig(cfg);
                        if(s == ERR_SUCCESS)
                        {
                            ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                            ThisUsbDev.DeviceType = USB_DEV_CLASS_HUB;

                            for(i = 1; i <= ThisUsbDev.GpHUBPortNum; i++)
                            {
                                DevOnHubPort[i - 1].DeviceStatus = ROOT_DEV_DISCONNECT;
                                s = HubSetPortFeature(i, HUB_PORT_POWER);
                                if(s != ERR_SUCCESS)
                                {
                                    PRINT("Ext-HUB Port_%1d# power on error\n", (uint16_t)i);
                                }
                            }
                            PRINT("USB-HUB Ready\n");
                            SetUsbSpeed(1);
                            return (ERR_SUCCESS);
                        }
                    }
                }
                else
                {
                    s = CtrlSetUsbConfig(cfg);
                    if(s == ERR_SUCCESS)
                    {

                        ThisUsbDev.DeviceStatus = ROOT_DEV_SUCCESS;
                        ThisUsbDev.DeviceType = DEV_TYPE_UNKNOW;
                        SetUsbSpeed(1);
                        return (ERR_SUCCESS);
                    }
                }
#endif
            }
        }
    }

    PRINT("InitRootDev Err = %02X\n", (uint16_t)s);
#ifdef FOR_ROOT_UDISK_ONLY
    CHRV3DiskStatus = DISK_CONNECT;
#else
    ThisUsbDev.DeviceStatus = ROOT_DEV_FAILED;
#endif
    SetUsbSpeed(1);
    return (s);
}

uint8_t InitDevOnHub(uint8_t HubPortIndex)
{
    uint8_t i, s, cfg, dv_cls, if_cls;
    uint8_t ifc;
    PRINT("Init dev @ExtHub-port_%1d ", (uint16_t)HubPortIndex);
    if(HubPortIndex == 0)
    {
        return (ERR_USB_UNKNOWN);
    }
    SelectHubPort(HubPortIndex);
    PRINT("GetDevDescr: ");
    s = CtrlGetDeviceDescr();
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    DevOnHubPort[HubPortIndex - 1].DeviceVID = ((uint16_t)((PUSB_DEV_DESCR)Com_Buffer)->idVendor);
    DevOnHubPort[HubPortIndex - 1].DevicePID = ((uint16_t)((PUSB_DEV_DESCR)Com_Buffer)->idProduct);

    dv_cls = ((PUSB_DEV_DESCR)Com_Buffer)->bDeviceClass;
    cfg = (1 << 4) + HubPortIndex;
    s = CtrlSetUsbAddress(cfg);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    DevOnHubPort[HubPortIndex - 1].DeviceAddress = cfg;
    PRINT("GetCfgDescr: ");
    s = CtrlGetConfigDescr();
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    cfg = ((PUSB_CFG_DESCR)Com_Buffer)->bConfigurationValue;
    for(i = 0; i < ((PUSB_CFG_DESCR)Com_Buffer)->wTotalLength; i++)
    {
        PRINT("x%02X ", (uint16_t)(Com_Buffer[i]));
    }
    PRINT("\n");

    if_cls = ((PXUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceClass;
    if(dv_cls == 0x00 && if_cls == USB_DEV_CLASS_STORAGE)
    {
        AnalyzeBulkEndp(Com_Buffer, HubPortIndex);
        for(i = 0; i != 4; i++)
        {
            PRINT("%02x ", (uint16_t)DevOnHubPort[HubPortIndex - 1].GpVar[i]);
        }
        PRINT("\n");
        s = CtrlSetUsbConfig(cfg);
        if(s == ERR_SUCCESS)
        {
            DevOnHubPort[HubPortIndex - 1].DeviceStatus = ROOT_DEV_SUCCESS;
            DevOnHubPort[HubPortIndex - 1].DeviceType = USB_DEV_CLASS_STORAGE;
            PRINT("USB-Disk Ready\n");
            SetUsbSpeed(1);
            return (ERR_SUCCESS);
        }
    }
    else if((dv_cls == 0x00) && (if_cls == USB_DEV_CLASS_HID) && (((PXUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceSubClass <= 0x01))
    {
        ifc = ((PXUSB_CFG_DESCR_LONG)Com_Buffer)->cfg_descr.bNumInterfaces;
        s = AnalyzeHidIntEndp(Com_Buffer, HubPortIndex);
        PRINT("AnalyzeHidIntEndp %02x\n", (uint16_t)s);
        if_cls = ((PXUSB_CFG_DESCR_LONG)Com_Buffer)->itf_descr.bInterfaceProtocol;
        s = CtrlSetUsbConfig(cfg);
        if(s == ERR_SUCCESS)
        {
            for(dv_cls = 0; dv_cls < ifc; dv_cls++)
            {
                s = CtrlGetHIDDeviceReport(dv_cls);
                if(s == ERR_SUCCESS)
                {
                    for(i = 0; i < 64; i++)
                    {
                        PRINT("x%02X ", (uint16_t)(Com_Buffer[i]));
                    }
                    PRINT("\n");
                }
            }

            DevOnHubPort[HubPortIndex - 1].DeviceStatus = ROOT_DEV_SUCCESS;
            if(if_cls == 1)
            {
                DevOnHubPort[HubPortIndex - 1].DeviceType = DEV_TYPE_KEYBOARD;

                if(ifc > 1)
                {
                    PRINT("USB_DEV_CLASS_HID Ready\n");
                    DevOnHubPort[HubPortIndex - 1].DeviceType = USB_DEV_CLASS_HID;
                }
                PRINT("USB-Keyboard Ready\n");
                SetUsbSpeed(1);

                return (ERR_SUCCESS);
            }
            else if(if_cls == 2)
            {
                DevOnHubPort[HubPortIndex - 1].DeviceType = DEV_TYPE_MOUSE;

                if(ifc > 1)
                {
                    PRINT("USB_DEV_CLASS_HID Ready\n");
                    DevOnHubPort[HubPortIndex - 1].DeviceType = USB_DEV_CLASS_HID;
                }
                PRINT("USB-Mouse Ready\n");
                SetUsbSpeed(1);

                return (ERR_SUCCESS);
            }
            s = ERR_USB_UNSUPPORT;
        }
    }
    else if(dv_cls == USB_DEV_CLASS_HUB)
    {
        DevOnHubPort[HubPortIndex - 1].DeviceType = USB_DEV_CLASS_HUB;
        PRINT("This program don't support Level 2 HUB\n");
        s = HubClearPortFeature(i, HUB_PORT_ENABLE);
        if(s != ERR_SUCCESS)
        {
            return (s);
        }
        s = ERR_USB_UNSUPPORT;
    }
    else
    {
        AnalyzeBulkEndp(Com_Buffer, HubPortIndex);
        for(i = 0; i != 4; i++)
        {
            PRINT("%02x ", (uint16_t)DevOnHubPort[HubPortIndex - 1].GpVar[i]);
        }
        PRINT("\n");
        s = CtrlSetUsbConfig(cfg);
        if(s == ERR_SUCCESS)
        {

            DevOnHubPort[HubPortIndex - 1].DeviceStatus = ROOT_DEV_SUCCESS;
            DevOnHubPort[HubPortIndex - 1].DeviceType = dv_cls ? dv_cls : if_cls;
            SetUsbSpeed(1);
            return (ERR_SUCCESS);
        }
    }
    PRINT("InitDevOnHub Err = %02X\n", (uint16_t)s);
    DevOnHubPort[HubPortIndex - 1].DeviceStatus = ROOT_DEV_FAILED;
    SetUsbSpeed(1);
    return (s);
}

uint8_t EnumHubPort()
{
    uint8_t i, s;

    for(i = 1; i <= ThisUsbDev.GpHUBPortNum; i++)
    {
        SelectHubPort(0);
        s = HubGetPortStatus(i);
        if(s != ERR_SUCCESS)
        {
            return (s);
        }
        if(((Com_Buffer[0] & (1 << (HUB_PORT_CONNECTION & 0x07))) && (Com_Buffer[2] & (1 << (HUB_C_PORT_CONNECTION & 0x07)))) || (Com_Buffer[2] == 0x10))
        {
            DevOnHubPort[i - 1].DeviceStatus = ROOT_DEV_CONNECTED;
            DevOnHubPort[i - 1].DeviceAddress = 0x00;
            s = HubGetPortStatus(i);
            if(s != ERR_SUCCESS)
            {
                return (s);
            }
            DevOnHubPort[i - 1].DeviceSpeed = Com_Buffer[1] & (1 << (HUB_PORT_LOW_SPEED & 0x07)) ? 0 : 1;
            if(DevOnHubPort[i - 1].DeviceSpeed)
            {
                PRINT("Found full speed device on port %1d\n", (uint16_t)i);
            }
            else
            {
                PRINT("Found low speed device on port %1d\n", (uint16_t)i);
            }
            mDelaymS(200);
            s = HubSetPortFeature(i, HUB_PORT_RESET);
            if(s != ERR_SUCCESS)
            {
                return (s);
            }
            PRINT("Reset port and then wait in\n");
            do
            {
                mDelaymS(1);
                s = HubGetPortStatus(i);
                if(s != ERR_SUCCESS)
                {
                    return (s);
                }
            } while(Com_Buffer[0] & (1 << (HUB_PORT_RESET & 0x07)));
            mDelaymS(100);
            s = HubClearPortFeature(i, HUB_C_PORT_RESET);

            s = HubClearPortFeature(i, HUB_C_PORT_CONNECTION);
            if(s != ERR_SUCCESS)
            {
                return (s);
            }
            s = HubGetPortStatus(i);
            if(s != ERR_SUCCESS)
            {
                return (s);
            }
            if((Com_Buffer[0] & (1 << (HUB_PORT_CONNECTION & 0x07))) == 0)
            {
                DevOnHubPort[i - 1].DeviceStatus = ROOT_DEV_DISCONNECT;
            }
            s = InitDevOnHub(i);
            if(s != ERR_SUCCESS)
            {
                return (s);
            }
            SetUsbSpeed(1);
        }
        else if(Com_Buffer[2] & (1 << (HUB_C_PORT_ENABLE & 0x07)))
        {
            HubClearPortFeature(i, HUB_C_PORT_ENABLE);
            PRINT("Device on port error\n");
            s = HubSetPortFeature(i, HUB_PORT_RESET);
            if(s != ERR_SUCCESS)
                return (s);
            do
            {
                mDelaymS(1);
                s = HubGetPortStatus(i);
                if(s != ERR_SUCCESS)
                    return (s);
            } while(Com_Buffer[0] & (1 << (HUB_PORT_RESET & 0x07)));
        }
        else if((Com_Buffer[0] & (1 << (HUB_PORT_CONNECTION & 0x07))) == 0)
        {
            if(DevOnHubPort[i - 1].DeviceStatus >= ROOT_DEV_CONNECTED)
            {
                PRINT("Device on port %1d removed\n", (uint16_t)i);
            }
            DevOnHubPort[i - 1].DeviceStatus = ROOT_DEV_DISCONNECT;
            if(Com_Buffer[2] & (1 << (HUB_C_PORT_CONNECTION & 0x07)))
            {
                HubClearPortFeature(i, HUB_C_PORT_CONNECTION);
            }
        }
    }
    return (ERR_SUCCESS);
}

uint8_t EnumAllHubPort(void)
{
    uint8_t s;

    if((ThisUsbDev.DeviceStatus >= ROOT_DEV_SUCCESS) && (ThisUsbDev.DeviceType == USB_DEV_CLASS_HUB))
    {
        SelectHubPort(0);
        s = EnumHubPort();
        if(s != ERR_SUCCESS)
        {
            PRINT("EnumAllHubPort err = %02X\n", (uint16_t)s);
        }
        SetUsbSpeed(1);
    }
    return (ERR_SUCCESS);
}

uint16_t SearchTypeDevice(uint8_t type)
{
    uint8_t RootHubIndex;
    uint8_t HubPortIndex;

    RootHubIndex = 0;
    if((ThisUsbDev.DeviceType == USB_DEV_CLASS_HUB) && (ThisUsbDev.DeviceStatus >= ROOT_DEV_SUCCESS))
    {
        for(HubPortIndex = 1; HubPortIndex <= ThisUsbDev.GpHUBPortNum; HubPortIndex++)
        {
            if(DevOnHubPort[HubPortIndex - 1].DeviceType == type && DevOnHubPort[HubPortIndex - 1].DeviceStatus >= ROOT_DEV_SUCCESS)
            {
                return (((uint16_t)RootHubIndex << 8) | HubPortIndex);
            }
        }
    }
    if((ThisUsbDev.DeviceType == type) && (ThisUsbDev.DeviceStatus >= ROOT_DEV_SUCCESS))
    {
        return ((uint16_t)RootHubIndex << 8);
    }

    return (0xFFFF);
}

uint8_t SETorOFFNumLock(uint8_t *buf)
{
    uint8_t tmp[] = {0x21, 0x09, 0x00, 0x02, 0x00, 0x00, 0x01, 0x00};
    uint8_t len, s;
    if((buf[2] == 0x53) & ((buf[0] | buf[1] | buf[3] | buf[4] | buf[5] | buf[6] | buf[7]) == 0))
    {
        for(s = 0; s != sizeof(tmp); s++)
        {
            ((uint8_t *)pSetupReq)[s] = tmp[s];
        }
        s = HostCtrlTransfer(Com_Buffer, &len);
        if(s != ERR_SUCCESS)
        {
            return (s);
        }
    }
    return (ERR_SUCCESS);
}

uint8_t CtrlGetHIDDeviceReport(uint8_t infc)
{
    uint8_t s;
    uint8_t len;

    CopySetupReqPkg(SetupSetHIDIdle);
    pSetupReq->wIndex = infc;
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }

    CopySetupReqPkg(SetupGetHIDDevReport);
    pSetupReq->wIndex = infc;
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }

    return (ERR_SUCCESS);
}

uint8_t CtrlGetHubDescr(void)
{
    uint8_t s;
    uint8_t len;

    CopySetupReqPkg(SetupGetHubDescr);
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    if(len < ((PUSB_SETUP_REQ)SetupGetHubDescr)->wLength)
    {
        return (ERR_USB_BUF_OVER);
    }

    return (ERR_SUCCESS);
}

uint8_t HubGetPortStatus(uint8_t HubPortIndex)
{
    uint8_t s;
    uint8_t len;

    pSetupReq->bRequestType = HUB_GET_PORT_STATUS;
    pSetupReq->bRequest = HUB_GET_STATUS;
    pSetupReq->wValue = 0x0000;
    pSetupReq->wIndex = 0x0000 | HubPortIndex;
    pSetupReq->wLength = 0x0004;
    s = HostCtrlTransfer(Com_Buffer, &len);
    if(s != ERR_SUCCESS)
    {
        return (s);
    }
    if(len < 4)
    {
        return (ERR_USB_BUF_OVER);
    }
    return (ERR_SUCCESS);
}

uint8_t HubSetPortFeature(uint8_t HubPortIndex, uint8_t FeatureSelt)
{
    pSetupReq->bRequestType = HUB_SET_PORT_FEATURE;
    pSetupReq->bRequest = HUB_SET_FEATURE;
    pSetupReq->wValue = 0x0000 | FeatureSelt;
    pSetupReq->wIndex = 0x0000 | HubPortIndex;
    pSetupReq->wLength = 0x0000;
    return (HostCtrlTransfer(NULL, NULL));
}

uint8_t HubClearPortFeature(uint8_t HubPortIndex, uint8_t FeatureSelt)
{
    pSetupReq->bRequestType = HUB_CLEAR_PORT_FEATURE;
    pSetupReq->bRequest = HUB_CLEAR_FEATURE;
    pSetupReq->wValue = 0x0000 | FeatureSelt;
    pSetupReq->wIndex = 0x0000 | HubPortIndex;
    pSetupReq->wLength = 0x0000;
    return (HostCtrlTransfer(NULL, NULL));
}
