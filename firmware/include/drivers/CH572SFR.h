#ifndef __BASE_TYPE__
#define __BASE_TYPE__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TRUE
#define TRUE                    1
#define FALSE                   0
#endif
#ifndef NULL
#define NULL                    0
#endif

#ifndef VOID
#define VOID                    void
#endif
#ifndef CONST
#define CONST                   const
#endif
#ifndef BOOL
typedef unsigned char           BOOL;
#endif
#ifndef BOOLEAN
typedef unsigned char           BOOLEAN;
#endif
#ifndef CHAR
typedef char                    CHAR;
#endif
#ifndef INT8
typedef char                    INT8;
#endif
#ifndef INT16
typedef short                   INT16;
#endif
#ifndef INT32
typedef long                    INT32;
#endif
#ifndef UINT8
typedef unsigned char           UINT8;
#endif
#ifndef UINT16
typedef unsigned short          UINT16;
#endif
#ifndef UINT32
typedef unsigned long           UINT32;
#endif
#ifndef UINT64
typedef unsigned long long      UINT64;
#endif
#ifndef UINT8V
typedef unsigned char volatile  UINT8V;
#endif
#ifndef UINT16V
typedef unsigned short volatile UINT16V;
#endif
#ifndef UINT32V
typedef unsigned long volatile  UINT32V;
#endif
#ifndef UINT64V
typedef unsigned long long volatile  UINT64V;
#endif

#ifndef PVOID
typedef void                    *PVOID;
#endif
#ifndef PCHAR
typedef char                    *PCHAR;
#endif
#ifndef PCHAR
typedef const char              *PCCHAR;
#endif
#ifndef PINT8
typedef char                    *PINT8;
#endif
#ifndef PINT16
typedef short                   *PINT16;
#endif
#ifndef PINT32
typedef long                    *PINT32;
#endif
#ifndef PUINT8
typedef unsigned char           *PUINT8;
#endif
#ifndef PUINT16
typedef unsigned short          *PUINT16;
#endif
#ifndef PUINT32
typedef unsigned long           *PUINT32;
#endif
#ifndef PUINT8V
typedef volatile unsigned char  *PUINT8V;
#endif
#ifndef PUINT16V
typedef volatile unsigned short *PUINT16V;
#endif
#ifndef PUINT32V
typedef volatile unsigned long  *PUINT32V;
#endif
#ifndef PUINT64V
typedef volatile unsigned long long  *PUINT64V;
#endif

#ifndef min
#define min(a,b)                (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)                (((a) > (b)) ? (a) : (b))
#endif

#define FIELD_OFFSET(Type, Field)    ((UINT16)&(((Type *)0)->Field))

#define FIELD_SIZE(Type, Field)      (sizeof(((Type *)0)->Field))

#define FIELD_TYPE(Type, Field)      (((Type *)0)->Field)

#define NUMBER_OF(Array)             (sizeof(Array)/sizeof((Array)[0]))
#define NUMBER_OF_FIELD(Type, Field) (NUMBER_OF(FIELD_TYPE(Type, Field)))

#ifdef __cplusplus
}
#endif

#endif

#ifndef __CH572SFR_H__
#define __CH572SFR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define R32_IWDG_KR         (*((PUINT32V)0x40001000))
#define R32_IWDG_CFG        (*((PUINT32V)0x40001004))

#define R32_SAFE_ACCESS     (*((PUINT32V)0x40001040))
#define R8_SAFE_ACCESS_SIG  (*((PUINT8V)0x40001040))
#define  RB_SAFE_ACC_MODE   0x03
#define  RB_SAFE_ACC_ACT    0x04
#define  RB_SAFE_ACC_TIMER  0xF8
#define SAFE_ACCESS_SIG1    0x57
#define SAFE_ACCESS_SIG2    0xA8
#define SAFE_ACCESS_SIG0    0x00
#define R8_CHIP_ID          (*((PUINT8V)0x40001041))
#define R8_SAFE_ACCESS_ID   (*((PUINT8V)0x40001042))
#define R8_WDOG_COUNT       (*((PUINT8V)0x40001043))

#define R32_GLOBAL_CONFIG   (*((PUINT32V)0x40001044))
#define R8_RESET_STATUS     (*((PUINT8V)0x40001044))
#define  RB_RESET_FLAG      0x07
#define  RST_FLAG_SW        0x00
#define  RST_FLAG_RPOR      0x01
#define  RST_FLAG_WTR       0x02
#define  RST_FLAG_MR        0x03

#define  RST_FLAG_GPWSM     0x05

#define R8_GLOB_ROM_CFG     R8_RESET_STATUS
#define  RB_ROM_CODE_WE     0xC0
#define  RB_ROM_CTRL_EN     0x20
#define  RB_ROM_CODE_OFS    0x10
#define R8_GLOB_CFG_INFO    (*((PUINT8V)0x40001045))
#define  RB_BOOT_LOADER     0x20
#define  RB_CFG_RST_PIN     0x10
#define  RB_CFG_BOOT_EN     0x08
#define  RB_CFG_RESET_EN    0x04
#define R8_RST_WDOG_CTRL    (*((PUINT8V)0x40001046))
#define  RB_WDOG_INT_FLAG   0x10
#define  RB_WDOG_INT_EN     0x04
#define  RB_WDOG_RST_EN     0x02
#define  RB_BOOT_LOAD_MAN   0x01
#define  RB_SOFTWARE_RESET  0x01
#define R8_GLOB_RESET_KEEP  (*((PUINT8V)0x40001047))

#define R32_CLOCK_CONFIG    (*((PUINT32V)0x40001008))
#define R8_CLK_SYS_CFG      (*((PUINT8V)0x40001008))
#define  RB_CLK_SYS_MOD     0xC0
#define  RB_CLK_PLL_DIV     0x1F
#define R8_HFCK_PWR_CTRL    (*((PUINT8V)0x4000100A))
#define  RB_CLK_PLL_PON     0x10
#define  RB_CLK_XT32M_KEEP  0x08
#define  RB_CLK_XT32M_PON   0x04

#define R32_SLEEP_CONTROL   (*((PUINT32V)0x4000100C))
#define R8_SLP_CLK_OFF0     (*((PUINT8V)0x4000100C))
#define  RB_SLP_KEYSCAN_WAKE  0x80
#define  RB_SLP_CLK_UART    0x10
#define  RB_SLP_CLK_CMP     0x02
#define  RB_SLP_CLK_TMR     0x01
#define R8_SLP_CLK_OFF1     (*((PUINT8V)0x4000100D))
#define  RB_SLP_CLK_BLE     0x80
#define  RB_SLP_CLK_USB     0x10
#define  RB_SLP_CLK_I2C     0x08
#define  RB_SLP_CLK_PWMX    0x04
#define  RB_CLK_OFF_AESCCM  0x02
#define  RB_SLP_CLK_SPI     0x01
#define R8_SLP_WAKE_CTRL    (*((PUINT8V)0x4000100E))
#define  RB_GPIO_WAKE_MODE  0x80
#define  RB_WAKE_EV_MODE    0x40
#define  RB_SLP_BAT_WAKE    0x20
#define  RB_SLP_GPIO_WAKE   0x10
#define  RB_SLP_RTC_WAKE    0x08
#define  RB_SLP_GPIO_EDGE_MODE    0x04
#define  RB_SLP_ENC_WAKE    0x02
#define  RB_SLP_USB_WAKE    0x01
#define R8_SLP_POWER_CTRL   (*((PUINT8V)0x4000100F))
#define  RB_RAM_RET_LV      0xC0
#define  RB_SLP_CLK_RAMX    0x10
#define  RB_WAKE_DLY_MOD    0x07

#define R16_PIN_ALTERNATE   (*((PUINT16V)0x40001018))
#define  RB_PIN_DEBUG_EN     0x4000
#define  RB_PIN_USB_EN      0x2000
#define  RB_UDP_PU_EN         0x1000
#define  RB_PA_DI_DIS         0x0FFF
#define R16_PIN_ALTERNATE_H (*((PUINT16V)0x4000101A))
#define  RB_25M_EN            0x1000
#define  RB_SPI_CLK            0x0800
#define  RB_I2C_PIN            0x0600
#define  RB_SPI_CS            0x0100
#define  RB_TMR_PIN         0x00C0
#define  RB_UART_TXD        0x0038
#define  RB_UART_RXD        0x0007
#define R16_SLP_WAKE_CFG     (*((PUINT16V)0x4000101E))
#define  RB_ACAUTO_ENABLE   0x0100
#define  RB_PRECLK_CNT_SEL  0x0060
#define  RB_PRECLK_CNT_EN   0x0010
#define  RB_OSCCLK_RDY_KEEP 0x0001
#define R8_SLP_CLK_OFF2     (*((PUINT8V)0x4000101D))
#define  RB_CLK_OFF_HCLK    0x10
#define  RB_CLK_OFF_DEBUG   0x02
#define  RB_CLK_OFF_XROM    0x01
#define R8_LONG_RST_CFG     (*((PUINT8V)0x4000101C))
#define  RB_LONG_TIM_SEL    0x06
#define  RB_LONG_RST_EN     0x01

#define R32_POWER_MANAG     (*((PUINT32V)0x40001020))
#define R16_POWER_PLAN      (*((PUINT16V)0x40001020))
#define  RB_PWR_PLAN_EN     0x8000
#define  RB_XT_PRE_EN       0x4000
#define  RB_PWR_MUST_0      0x2000

#define  RB_PWR_LDO5V_EN    0x0100
#define  RB_PWR_SYS_EN      0x80
#define  RB_MAIN_ACT        0x40
#define  RB_PWR_EXTEND      0x08
#define  RB_PWR_CORE        0x04
#define  RB_PWR_RAM12K      0x02
#define  RB_PWR_XROM        0x01
#define R16_AUX_POWER_ADJ   (*((PUINT16V)0x40001022))
#define  RB_CFG_IVREF       0x0F00
#define  RB_ULPLDO_ADJ      0x0007

#define R32_BATTERY_CTRL    (*((PUINT32V)0x40001024))
#define R8_BAT_DET_CTRL     (*((PUINT8V)0x40001024))
#define  RB_BAT_LOW_IE      0x08
#define  RB_BAT_MON_EN      0x02
#define  RB_PWR_LDO_EN      0x01

#define R8_BAT_DET_CFG      (*((PUINT8V)0x40001025))
#define  RB_BAT_LOW_VTH     0x03
#define R8_BAT_STATUS       (*((PUINT8V)0x40001026))
#define  RB_BAT_STAT_LOW    0x02

#define R32_OSC_LSI_CTRL     (*((PUINT32V)0x4000102C))
#define R16_INT_LSI_TUNE     (*((PUINT16V)0x4000102C))
#define  RB_INT_LSI_TUNE     0x1FFF
#define R8_LSI_CONFIG       (*((PUINT8V)0x4000102F))
#define  RB_LSI_CLK_PIN     0x80
#define  RB_CLK_LSI_PON  0x01

#define R32_RTC_CTRL        (*((PUINT32V)0x40001030))
#define R8_RTC_FLAG_CTRL    (*((PUINT8V)0x40001030))
#define  RB_RTC_TRIG_FLAG   0x80
#define  RB_RTC_TMR_FLAG    0x40
#define  RB_RTC_TRIG_CLR    0x20
#define  RB_RTC_TMR_CLR     0x10
#define R8_RTC_MODE_CTRL    (*((PUINT8V)0x40001031))
#define  RB_RTC_LOAD_HI     0x80
#define  RB_RTC_LOAD_LO     0x40
#define  RB_RTC_TRIG_EN     0x20
#define  RB_RTC_TMR_EN      0x10
#define  RB_RTC_IGNORE_B0   0x08
#define  RB_RTC_TMR_MODE    0x07
#define R32_RTC_TRIG        (*((PUINT32V)0x40001034))
#define R32_RTC_CNT_LSI     (*((PUINT32V)0x40001038))
#define R16_RTC_CNT_LSI     (*((PUINT16V)0x40001038))
#define R16_RTC_CNT_DIV1    (*((PUINT16V)0x4000103A))
#define R32_RTC_CNT_DIV2    (*((PUINT32V)0x4000103C))

#define R32_MISC_CTRL       (*((PUINT32V)0x40001048))
#define R8_PLL_CONFIG       (*((PUINT8V)0x4000104B))
#define  RB_PLL_CFG_DAT     0x7F

#define R8_XT32M_TUNE       (*((PUINT8V)0x4000104E))
#define  RB_XT32M_C_LOAD    0x70
#define  RB_XT32M_I_BIAS    0x03

#define R32_OSC_CALIB       (*((PUINT32V)0x40001050))
#define R16_OSC_CAL_CNT     (*((PUINT16V)0x40001050))
#define  RB_OSC_CAL_IF      0x8000
#define  RB_OSC_CAL_OV_CLR  0x4000
#define  RB_OSC_CAL_CNT     0x3FFF
#define R8_OSC_CAL_OV_CNT   (*((PUINT8V)0x40001052))
#define R8_OSC_CAL_CTRL     (*((PUINT8V)0x40001053))
#define  RB_CNT_CLR         0x80
#define  RB_OSC_CNT_END     0x40
#define  RB_OSC_CNT_EN      0x20
#define  RB_OSC_CAL_IE      0x10
#define  RB_OSC_CNT_HALT    0x08
#define  RB_OSC_CNT_TOTAL   0x07

#define R32_CMP_CTRL       (*((PUINT32V)0x40001054))
#define R8_CMP_CTRL_0      (*((PUINT8V)0x40001054))
#define  RB_CMP_NREF_LEVEL 0xF0
#define  RB_CMP_SW         0x0C
#define  RB_CMP_CAP        0x02
#define  RB_CMP_EN         0x01
#define R8_CMP_CTRL_1      (*((PUINT8V)0x40001055))
#define  RB_CMP_OUT_SEL    0x0C
#define  RB_CMP_IE         0x01
#define R8_CMP_CTRL_2      (*((PUINT8V)0x40001056))
#define  RB_CMP_IF         0x01
#define R8_CMP_CTRL_3      (*((PUINT8V)0x40001057))
#define  RB_CMP_REAL_SIG   0x01
#define  RB_APR_OUT_CMP    0x02
#define R32_SAFE_ACCESS_SIG2  (*((PUINT32V)0x40001058))
#define  RB_FUN_MODE        0x07000000
#define  RB_FLASH_HALTED    0x00800000
#define  RB_MANU_CFG_LOCK   0x00400000
#define  RB_RD_PROTECT      0x00200000
#define  RB_SAFE_AC_DIS     0x00100000

#define R32_FLASH_DATA      (*((PUINT32V)0x40001800))
#define R32_FLASH_CONTROL   (*((PUINT32V)0x40001804))
#define R8_FLASH_DATA       (*((PUINT8V)0x40001804))
#define R8_FLASH_SCK        (*((PUINT8V)0x40001805))
#define R8_FLASH_CTRL       (*((PUINT8V)0x40001806))
#define R8_FLASH_CFG        (*((PUINT8V)0x40001807))

#define R32_GPIO_INT_EN     (*((PUINT32V)0x40001090))
#define R16_PA_INT_EN       (*((PUINT16V)0x40001090))
#define R32_GPIO_INT_MODE   (*((PUINT32V)0x40001094))
#define R16_PA_INT_MODE     (*((PUINT16V)0x40001094))
#define R16_PA_INT_EDGE_TYPE (*((PUINT16V)0x40001096))
#define R32_GPIO_INT_IF     (*((PUINT32V)0x4000109C))
#define R16_PA_INT_IF       (*((PUINT16V)0x4000109C))

#define R32_PA_DIR          (*((PUINT32V)0x400010A0))
#define R8_PA_DIR_0         (*((PUINT8V)0x400010A0))
#define R8_PA_DIR_1         (*((PUINT8V)0x400010A1))
#define R32_PA_PIN          (*((PUINT32V)0x400010A4))
#define R8_PA_PIN_0         (*((PUINT8V)0x400010A4))
#define R8_PA_PIN_1         (*((PUINT8V)0x400010A5))
#define R32_PA_OUT          (*((PUINT32V)0x400010A8))
#define R8_PA_OUT_0         (*((PUINT8V)0x400010A8))
#define R8_PA_OUT_1         (*((PUINT8V)0x400010A9))
#define R32_PA_CLR          (*((PUINT32V)0x400010AC))
#define R8_PA_CLR_0         (*((PUINT8V)0x400010AC))
#define R8_PA_CLR_1         (*((PUINT8V)0x400010AD))
#define R32_PA_PU           (*((PUINT32V)0x400010B0))
#define R8_PA_PU_0          (*((PUINT8V)0x400010B0))
#define R8_PA_PU_1          (*((PUINT8V)0x400010B1))
#define R32_PA_PD_DRV       (*((PUINT32V)0x400010B4))
#define R8_PA_PD_DRV_0      (*((PUINT8V)0x400010B4))
#define R8_PA_PD_DRV_1      (*((PUINT8V)0x400010B5))
#define R32_PA_SET          (*((PUINT32V)0x400010B8))

#define R16_KEY_SCAN_CTRL    (*((PUINT16V)0x40001064))
#define  RB_CLR_WAKEUP_EN   0x4000
#define  RB_SCAN_1END_WAKE_EN    0x2000
#define  RB_PIN_SCAN_EN     0x1F00
#define  RB_SCAN_CLK_DIV    0x00F0
#define  RB_SCAN_CNT_END    0x000E
#define  RB_SCAN_START_EN   0x0001
#define R8_KEY_SCAN_INT_EN (*((PUINT8V)0x40001066))
#define  RB_SCAN_1END_IE    0x02
#define  RB_KEY_PRESSED_IE  0x01
#define R8_KEY_SCAN_INT_FLAG (*((PUINT8V)0x40001067))
#define  RB_SCAN_1END_IF    0x02
#define  RB_KEY_PRESSED_IF  0x01
#define R32_KEY_SCAN_NUMB  (*((PUINT32V)0x40001068))
#define  RB_KEY_SCAN_CNT    0x700000
#define  RB_KEY_SCAN_NUMB   0x0FFFFF

#define BA_PA               ((PUINT8V)0x400010A0)
#define GPIO_DIR            0x00
#define GPIO_DIR_0          0x00
#define GPIO_DIR_1          0x01
#define GPIO_PIN            0x04
#define GPIO_PIN_0          0x04
#define GPIO_PIN_1          0x05
#define GPIO_OUT            0x08
#define GPIO_OUT_0          0x08
#define GPIO_OUT_1          0x09
#define GPIO_CLR            0x0C
#define GPIO_CLR_0          0x0C
#define GPIO_CLR_1          0x0D
#define GPIO_PU             0x10
#define GPIO_PU_0           0x10
#define GPIO_PU_1           0x11
#define GPIO_PD_DRV         0x14
#define GPIO_PD_DRV_0       0x14
#define GPIO_PD_DRV_1       0x15

#define bTIO            (1<<0)
#define bTXD_3          (1<<0)
#define bRXD_2          (1<<0)
#define bSCL_1          (1<<0)
#define bUDM            (1<<0)

#define bTCK            (1<<1)
#define bTXD_2          (1<<1)
#define bRXD_3          (1<<1)
#define bSDA_1          (1<<1)
#define bUDP            (1<<1)

#define bTXD_1          (1<<2)
#define bRXD_0          (1<<2)
#define bSDA_2          (1<<2)
#define bPWM2           (1<<2)
#define bPWM0_1         (1<<2)
#define bCAP1_1         (1<<2)
#define bCAP2_0         (1<<2)
#define bSCS_           (1<<2)
#define bKEY0           (1<<2)
#define bCMP0           (1<<2)

#define bTXD_0          (1<<3)
#define bRXD_1          (1<<3)
#define bSCL_2          (1<<3)
#define bPWM3           (1<<3)
#define bSCK_           (1<<3)
#define bKEY1           (1<<3)
#define bCMP1           (1<<3)

#define bPWM4           (1<<4)
#define bPWM0_2         (1<<4)
#define bSCS            (1<<4)
#define bCAP1_2         (1<<4)
#define bCAP2_3         (1<<4)
#define bXM25MO         (1<<4)

#define bSCL_3          (1<<5)
#define bSCK            (1<<5)

#define bSDA_6          (1<<6)
#define bMISO           (1<<6)
#define bRXD_4          (1<<6)

#define bTXD_4          (1<<7)
#define bMOSI           (1<<7)
#define bPWM1           (1<<7)
#define bPWM0_0         (1<<7)
#define bRST_           (1<<7)
#define bCAP1_0         (1<<7)
#define bCAP2_1         (1<<7)
#define bCMP2           (1<<7)

#define bRST            (1<<8)
#define bTXD_5          (1<<8)
#define bSCL_0          (1<<8)
#define bPWM5           (1<<8)
#define bKEY2           (1<<8)

#define bSDA_0          (1<<9)
#define bPWM0_3         (1<<9)
#define bRXD_5          (1<<9)
#define bCAP1_3         (1<<9)
#define bCAP2_2         (1<<9)

#define bTXD_7          (1<<10)
#define bRXD_6          (1<<10)
#define bKEY3           (1<<10)

#define bTXD_6          (1<<11)
#define bRXD_7          (1<<11)
#define bKEY4           (1<<11)

#define R32_TMR_CONTROL    (*((PUINT32V)0x40002400))
#define R8_TMR_CTRL_MOD    (*((PUINT8V)0x40002400))
#define R8_TMR_CTRL_DMA    (*((PUINT8V)0x40002401))
#define R8_TMR_INTER_EN    (*((PUINT8V)0x40002402))

#define R8_TMR_INT_FLAG    (*((PUINT8V)0x40002406))
#define R8_TMR_FIFO_COUNT  (*((PUINT8V)0x40002407))
#define R32_TMR_COUNT      (*((PUINT32V)0x40002408))
#define R16_TMR_COUNT      (*((PUINT16V)0x40002408))
#define R8_TMR_COUNT       (*((PUINT8V)0x40002408))
#define R32_TMR_CNT_END    (*((PUINT32V)0x4000240C))
#define R32_TMR_FIFO       (*((PUINT32V)0x40002410))
#define R16_TMR_FIFO       (*((PUINT16V)0x40002410))
#define R8_TMR_FIFO        (*((PUINT8V)0x40002410))
#define R32_TMR_DMA_NOW    (*((PUINT32V)0x40002414))
#define R16_TMR_DMA_NOW    (*((PUINT16V)0x40002414))
#define R32_TMR_DMA_BEG    (*((PUINT32V)0x40002418))
#define R16_TMR_DMA_BEG    (*((PUINT16V)0x40002418))
#define R32_TMR_DMA_END    (*((PUINT32V)0x4000241C))
#define R16_TMR_DMA_END    (*((PUINT16V)0x4000241C))

#define R32_ENC_REG_CTRL    (*((PUINT32V)0x40002420))
#define R8_ENC_REG_CTRL        (*((PUINT8V)0x40002420))

#define  RB_ENC_DIR         0x20
#define  RB_RD_CLR_EN       0x08
#define  RB_SMS_MODE        0x06
#define  RB_START_ENC_EN    0x01
#define R8_ENC_INTER_EN        (*((PUINT8V)0x40002421))
#define  RB_IE_DIR_DEC      0x02
#define  RB_IE_DIR_INC      0x01
#define R8_ENC_INT_FLAG        (*((PUINT8V)0x40002422))
#define  RB_IF_DIR_DEC      0x02
#define  RB_IF_DIR_INC      0x01
#define R32_ENC_REG_CEND    (*((PUINT32V)0x40002424))
#define R32_ENC_REG_CCNT    (*((PUINT32V)0x40002428))

#define TMR_FIFO_SIZE       8
#define BA_TMR             ((PUINT8V)0x40002400)
#define TMR_CTRL_MOD        0
#define  RB_TMR_PWM_REPEAT  0xC0
#define  RB_TMR_CAP_EDGE    0xC0
#define  RB_TMR_OUT_POLAR   0x10
#define  RB_TMR_CAP_COUNT   0x10
#define  RB_TMR_OUT_EN      0x08
#define  RB_TMR_COUNT_EN    0x04
#define  RB_TMR_ALL_CLEAR   0x02
#define  RB_TMR_MODE_IN     0x01
#define TMR_CTRL_DMA        1
#define  RB_TMR_DMA_LOOP    0x04
#define  RB_TMR_DMA_ENABLE  0x01
#define TMR_INTER_EN        2
#define  RB_TMR_IE_FIFO_OV  0x10
#define  RB_TMR_IE_DMA_END  0x08
#define  RB_TMR_IE_FIFO_HF  0x04
#define  RB_TMR_IE_DATA_ACT 0x02
#define  RB_TMR_IE_CYC_END  0x01
#define TMR_INT_FLAG        6
#define  RB_TMR_IF_FIFO_OV  0x10
#define  RB_TMR_IF_DMA_END  0x08
#define  RB_TMR_IF_FIFO_HF  0x04
#define  RB_TMR_IF_DATA_ACT 0x02
#define  RB_TMR_IF_CYC_END  0x01
#define TMR_FIFO_COUNT      7
#define TMR_DMA_END         0x1C
#define TMR_DMA_BEG         0x18
#define TMR_DMA_NOW         0x14
#define TMR_FIFO            0x10
#define TMR_CNT_END         0x0C
#define TMR_COUNT           0x08

#define R32_UART_CTRL      (*((PUINT32V)0x40003400))
#define R8_UART_MCR        (*((PUINT8V)0x40003400))
#define R8_UART_IER        (*((PUINT8V)0x40003401))
#define R8_UART_FCR        (*((PUINT8V)0x40003402))
#define R8_UART_LCR        (*((PUINT8V)0x40003403))
#define R8_UART_IIR        (*((PUINT8V)0x40003404))
#define R8_UART_LSR        (*((PUINT8V)0x40003405))
#define R8_UART_RBR        (*((PUINT8V)0x40003408))
#define R8_UART_THR        (*((PUINT8V)0x40003408))
#define R8_UART_RFC        (*((PUINT8V)0x4000340A))
#define R8_UART_TFC        (*((PUINT8V)0x4000340B))
#define R16_UART_DL        (*((PUINT16V)0x4000340C))
#define R8_UART_DLL        (*((PUINT8V)0x4000340C))
#define R8_UART_DLM        (*((PUINT8V)0x4000340D))
#define R8_UART_DIV        (*((PUINT8V)0x4000340E))

#define UART_FIFO_SIZE      8
#define UART_RECV_RDY_SZ    7
#define BA_UART            ((PUINT8V)0x40003400)
#define UART_MCR            0
#define  RB_MCR_OUT2        0x08
#define  RB_MCR_INT_OE      0x08
#define UART_IER            1
#define  RB_IER_TXD_EN      0x40
#define  RB_IER_LINE_STAT   0x04
#define  RB_IER_THR_EMPTY   0x02
#define  RB_IER_RECV_RDY    0x01
#define UART_FCR            2
#define  RB_FCR_FIFO_TRIG   0xC0
#define  RB_FCR_FIFO_EN     0x01
#define UART_LCR            3
#define  RB_LCR_DLAB        0x80
#define  RB_LCR_GP_BIT      0x80
#define  RB_LCR_BREAK_EN    0x40
#define  RB_LCR_PAR_MOD     0x30
#define  RB_LCR_PAR_EN      0x08
#define  RB_LCR_STOP_BIT    0x04
#define  RB_LCR_WORD_SZ     0x03
#define UART_IIR            4
#define  RB_IIR_FIFO_ID     0xC0
#define  RB_IIR_INT_MASK    0x0F
#define  RB_IIR_NO_INT      0x01
#define UART_LSR            5
#define  RB_LSR_ERR_RX_FIFO 0x80
#define  RB_LSR_TX_ALL_EMP  0x40
#define  RB_LSR_TX_FIFO_EMP 0x20
#define  RB_LSR_BREAK_ERR   0x10
#define  RB_LSR_FRAME_ERR   0x08
#define  RB_LSR_PAR_ERR     0x04
#define  RB_LSR_OVER_ERR    0x02
#define  RB_LSR_DATA_RDY    0x01
#define UART_MSR            6
#define UART_RBR            8
#define UART_THR            8
#define UART_RFC            0x0A
#define UART_TFC            0x0B
#define UART_DLL            0x0C

#define UART_DIV            0x0E
#define UART_ADR            0x0F

#define UART_II_RECV_TOUT   0x0C
#define UART_II_LINE_STAT   0x06
#define UART_II_RECV_RDY    0x04
#define UART_II_THR_EMPTY   0x02
#define UART_II_NO_INTER    0x01

#define R32_SPI_CONTROL    (*((PUINT32V)0x40004000))
#define R8_SPI_CTRL_MOD    (*((PUINT8V)0x40004000))
#define R8_SPI_CTRL_CFG    (*((PUINT8V)0x40004001))
#define R8_SPI_INTER_EN    (*((PUINT8V)0x40004002))
#define R8_SPI_CLOCK_DIV   (*((PUINT8V)0x40004003))
#define R8_SPI_SLAVE_PRE   (*((PUINT8V)0x40004003))
#define R32_SPI_STATUS     (*((PUINT32V)0x40004004))
#define R8_SPI_BUFFER      (*((PUINT8V)0x40004004))
#define R8_SPI_RUN_FLAG    (*((PUINT8V)0x40004005))
#define R8_SPI_INT_FLAG    (*((PUINT8V)0x40004006))
#define R8_SPI_FIFO_COUNT  (*((PUINT8V)0x40004007))
#define R32_SPI_INTER_CFG1 (*((PUINT32V)0x40004008))
#define R16_SPI_TOTAL_CNT  (*((PUINT16V)0x4000400C))
#define R32_SPI_FIFO       (*((PUINT32V)0x40004010))
#define R8_SPI_FIFO        (*((PUINT8V)0x40004010))
#define R8_SPI_FIFO_COUNT1 (*((PUINT8V)0x40004013))
#define R16_SPI_DMA_NOW    (*((PUINT16V)0x40004014))
#define R16_SPI_DMA_BEG    (*((PUINT16V)0x40004018))
#define R16_SPI_DMA_END    (*((PUINT16V)0x4000401C))

#define SPI_FIFO_SIZE       8
#define BA_SPI             ((PUINT8V)0x40004000)
#define SPI_CTRL_MOD        0
#define  RB_SPI_MISO_OE     0x80
#define  RB_SPI_MOSI_OE     0x40
#define  RB_SPI_SCK_OE      0x20
#define  RB_SPI_FIFO_DIR    0x10
#define  RB_SPI_SLV_CMD_MOD 0x08
#define  RB_SPI_MST_SCK_MOD 0x08
#define  RB_SPI_2WIRE_MOD   0x04
#define  RB_SPI_ALL_CLEAR   0x02
#define  RB_SPI_MODE_SLAVE  0x01
#define SPI_CTRL_CFG        1
#define  RB_SPI_MST_DLY_EN  0x40
#define  RB_SPI_BIT_ORDER   0x20
#define  RB_SPI_AUTO_IF     0x10
#define  RB_SPI_DMA_LOOP    0x04
#define  RB_MST_CLK_SEL     0x02
#define  RB_SPI_DMA_ENABLE  0x01
#define SPI_INTER_EN        2
#define  RB_SPI_IE_FST_BYTE 0x80
#define  RB_SPI_IE_FIFO_OV  0x10
#define  RB_SPI_IE_DMA_END  0x08
#define  RB_SPI_IE_FIFO_HF  0x04
#define  RB_SPI_IE_BYTE_END 0x02
#define  RB_SPI_IE_CNT_END  0x01
#define SPI_CLOCK_DIV       3
#define SPI_SLAVE_PRESET    3
#define SPI_BUFFER          4
#define SPI_RUN_FLAG        5
#define  RB_SPI_SLV_SELECT  0x80
#define  RB_SPI_SLV_CS_LOAD 0x40
#define  RB_SPI_FIFO_READY  0x20
#define  RB_SPI_SLV_CMD_ACT 0x10
#define SPI_INT_FLAG        6
#define  RB_SPI_IF_FST_BYTE 0x80
#define  RB_SPI_FREE        0x40
#define  RB_SPI_IF_FIFO_OV  0x10
#define  RB_SPI_IF_DMA_END  0x08
#define  RB_SPI_IF_FIFO_HF  0x04
#define  RB_SPI_IF_BYTE_END 0x02
#define  RB_SPI_IF_CNT_END  0x01
#define SPI_FIFO_COUNT      7
#define SPI_INTER_CFG1      8
#define  RB_SPI_IF_FIFO_FULL   0x020000
#define  RB_SPI_IF_FIFO_EMPTY  0x010000
#define  RB_SPI_IE_FIFO_FULL   0x0200
#define  RB_SPI_IE_FIFO_EMPTY  0x0100
#define  RB_SPI_INT_TYPE       0x001F
#define SPI_TOTAL_CNT       0x0C
#define SPI_FIFO            0x10
#define SPI_DMA_NOW         0x14
#define SPI_DMA_BEG         0x18

#define R16_I2C_CTRL1       (*((PUINT16V)0x40004800))
#define R16_I2C_CTRL2       (*((PUINT16V)0x40004804))
#define R16_I2C_OADDR1      (*((PUINT16V)0x40004808))
#define R16_I2C_OADDR2      (*((PUINT16V)0x4000480C))
#define R16_I2C_DATAR       (*((PUINT16V)0x40004810))
#define R16_I2C_STAR1       (*((PUINT16V)0x40004814))
#define R16_I2C_STAR2       (*((PUINT16V)0x40004818))

#define R16_I2C_CKCFGR      (*((PUINT16V)0x4000481C))
#define R16_I2C_RTR         (*((PUINT16V)0x40004820))

#define BA_I2C              ((PUINT8V)0x40004800)
#define I2C_CTRL1           0
#define  RB_I2C_SWRST       0x8000
#define  RB_I2C_ALERT       0x2000
#define  RB_I2C_PEC         0x1000
#define  RB_I2C_POS         0x0800
#define  RB_I2C_ACK         0x0400
#define  RB_I2C_STOP        0x0200
#define  RB_I2C_START       0x0100
#define  RB_I2C_NOSTRETCH   0x0080
#define  RB_I2C_ENGC        0x0040
#define  RB_I2C_ENPEC       0x0020
#define  RB_I2C_EBARP       0x0010
#define  RB_I2C_SMBTYPE     0x0008
#define  RB_I2C_SMBUS       0x0002
#define  RB_I2C_PE          0x0001
#define I2C_CTRL2           4
#define  RB_I2C_ITBUFEN     0x0400
#define  RB_I2C_ITEVTEN     0x0200
#define  RB_I2C_ITERREN     0x0100
#define  RB_I2C_FREQ        0x003F
#define I2C_OADDR1          8
#define  RB_I2C_ADDMODE     0x8000
#define  RB_I2C_ADD9_8      0x0300
#define  RB_I2C_ADD7_1      0x00FE
#define  RB_I2C_ADD0        0x0001
#define I2C_OADDR2          12
#define  RB_I2C_ADD2        0x00FE
#define  RB_I2C_ENDUAL      0x0001
#define I2C_DATAR           16
#define I2C_STAR1           20
#define  RB_I2C_SMBALERT    0x8000
#define  RB_I2C_TIMEOUT     0x4000
#define  RB_I2C_PECERR      0x1000
#define  RB_I2C_OVR         0x0800
#define  RB_I2C_AF          0x0400
#define  RB_I2C_ARLO        0x0200
#define  RB_I2C_BERR        0x0100
#define  RB_I2C_TxE         0x0080
#define  RB_I2C_RxNE        0x0040
#define  RB_I2C_STOPF       0x0010
#define  RB_I2C_ADD10       0x0008
#define  RB_I2C_BTF         0x0004
#define  RB_I2C_ADDR        0x0002
#define  RB_I2C_SB          0x0001
#define I2C_STAR2           24
#define  RB_I2C_PECX        0xFF00
#define  RB_I2C_DUALF       0x0080
#define  RB_I2C_SMBHOST     0x0040
#define  RB_I2C_SMBDEFAULT  0x0020
#define  RB_I2C_GENCALL     0x0010
#define  RB_I2C_TRA         0x0004
#define  RB_I2C_BUSY        0x0002
#define  RB_I2C_MSL         0x0001
#define I2C_CKCFGR          28
#define  RB_I2C_F_S         0x8000
#define  RB_I2C_DUTY        0x4000
#define  RB_I2C_CCR         0x0FFF
#define I2C_RTR             32
#define  RB_I2C_TRISE       0x003F

#define R32_PWM_CONTROL     (*((PUINT32V)0x40005000))
#define R8_PWM_OUT_EN       (*((PUINT8V)0x40005000))
#define R8_PWM_POLAR        (*((PUINT8V)0x40005001))
#define R8_PWM_CONFIG       (*((PUINT8V)0x40005002))
#define R8_PWM_DMA_CTRL    (*((PUINT8V)0x40005003))
#define R32_PWM1_3_DATA     (*((PUINT32V)0x40005004))
#define R16_PWM1_DATA       (*((PUINT16V)0x40005004))
#define R16_PWM2_DATA       (*((PUINT16V)0x40005006))
#define R8_PWM1_DATA        (*((PUINT8V)0x40005004))
#define R8_PWM2_DATA        (*((PUINT8V)0x40005005))
#define R8_PWM3_DATA        (*((PUINT8V)0x40005006))
#define R16_PWM3_DATA       (*((PUINT16V)0x40005008))
#define R32_PWM4_5_DATA     (*((PUINT32V)0x40005010))
#define R16_PWM4_DATA       (*((PUINT16V)0x40005010))
#define R16_PWM5_DATA       (*((PUINT16V)0x40005012))
#define R8_PWM4_DATA        (*((PUINT8V)0x40005010))
#define R8_PWM5_DATA        (*((PUINT8V)0x40005011))
#define R8_PWM_INT_EN       (*((PUINT8V)0x4000500C))
#define  RB_PWM_IE_OVER     0x40
#define  RB_PWM_IE_DMA      0x20
#define  RB_PWM_IE_FIFO     0x10
#define  RB_PWM1_IE_CYC     0x04
#define  RB_PWM_CYC_PRE     0x02
#define  RB_PWM_IE_CYC      0x01
#define R8_PWM_INT_FLAG     (*((PUINT8V)0x4000500D))
#define  RB_PWM_IF_OVER     0x10
#define  RB_PWM_IF_DMA      0x08
#define  RB_PWM_IF_FIFO     0x04
#define  RB_PWM1_IF_CYC     0x02
#define  RB_PWM_IF_CYC      0x01
#define R16_PWM_CYC_VALUE  (*((PUINT16V)0x40005014))
#define R16_PWM_CYC1_VALUE   (*((PUINT16V)0x40005016))
#define R16_PWM_CLOCK_DIV   (*((PUINT16V)0x40005018))
#define R32_PWM_DMA_NOW     (*((PUINT32V)0x4000501C))
#define R32_PWM_DMA_BEG     (*((PUINT32V)0x40005020))
#define R32_PWM_DMA_END     (*((PUINT32V)0x40005024))

#define BA_PWMX             ((PUINT8V)0x40005000)
#define PWM_OUT_EN          0
#define  RB_PWM5_OUT_EN     0x10
#define  RB_PWM4_OUT_EN     0x08
#define  RB_PWM3_OUT_EN     0x04
#define  RB_PWM2_OUT_EN     0x02
#define  RB_PWM1_OUT_EN     0x01
#define PWM_POLAR           1
#define  RB_PWM5_POLAR      0x10
#define  RB_PWM4_POLAR      0x08
#define  RB_PWM3_POLAR      0x04
#define  RB_PWM2_POLAR      0x02
#define  RB_PWM1_POLAR      0x01
#define PWM_CONFIG          2
#define  RB_PWM_SYNC_EN     0x80
#define  RB_PWM_SYNC_START  0x40
#define  RB_PWM4_5_CH       0x10
#define  RB_PWM4_5_STAG_EN  0x08
#define  RB_PWM_CYC_MOD     0x06
#define  RB_PWM_CYCLE_SEL   0x01
#define PWM_DMA_CTRL        3
#define  RB_DMA_SEL         0x04
#define  RB_DMA_ADDR_LOOP   0x02
#define  RB_DMA_ENABLE      0x01

#define PWM1_DATA_HOLD      4
#define PWM2_DATA_HOLD      5
#define PWM3_DATA_HOLD      6
#define PWM4_DATA_HOLD      7
#define PWM5_DATA_HOLD      8

#define BA_CODE             ((PUINT32)0x00000000)
#define SZ_CODE             0x00080000
#define BA_SFR              ((PUINT32)0x40000000)
#define SZ_SFR              0x00010000
#define BA_RAM              ((PUINT32)0x20000000)
#define SZ_RAM              0x00003000

#define CODE_FLASH_SIZE     0x00000
#define BOOT_LOAD_ADDR      0x3C000
#define BOOT_LOAD_SIZE      0x2000
#define BOOT_LOAD_CFG       0x3E000
#define ROM_CFG_ADDR        0x3F000

#define ID_CH572            0x72
#define ID_CH570            0x70

#define INT_ID_GPIO_A       17
#define INT_ID_SPI          19
#define INT_ID_BLEB         20
#define INT_ID_BLEL         21
#define INT_ID_USB          22
#define INT_ID_TMR          24
#define INT_ID_UART         27
#define INT_ID_RTC          28
#define INT_ID_CMP          29
#define INT_ID_I2C          30
#define INT_ID_PWMX         31
#define INT_ID_KEYSCAN      33
#define INT_ID_ENCODE       34
#define INT_ID_WDOG_BAT     35
#define INT_VEC_ENTRY_SZ    4

#define INT_ADDR_GPIO_A     (INT_ID_GPIO_A*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_SPI        (INT_ID_SPI*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_BLEB       (INT_ID_BLEB*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_BLEL       (INT_ID_BLEL*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_USB        (INT_ID_USB*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_TMR        (INT_ID_TMR*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_UART       (INT_ID_UART*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_RTC        (INT_ID_RTC*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_CMP        (INT_ID_CMP*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_I2C        (INT_ID_I2C*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_PWMX       (INT_ID_PWMX*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_KEYSCAN    (INT_ID_KEYSCAN*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_ENCODE     (INT_ID_ENCODE*INT_VEC_ENTRY_SZ+64)
#define INT_ADDR_WDOG_BAT   (INT_ID_WDOG_BAT*INT_VEC_ENTRY_SZ+64)

#ifndef TABLE_IRQN
#define __PFIC_PRIO_BITS          2
#define __Vendor_SysTickConfig    0
typedef enum IRQn
{
  Reset_IRQn                    =  0,
  NMI_IRQn                      =  2,
  EXC_IRQn                      =  3,
  ECALL_M_IRQn                  =  5,
  ECALL_U_IRQn                  =  8,
  BREAKPOINT_IRQn               =  9,
  SysTick_IRQn                  =  12,
  SWI_IRQn                      =  14,
  GPIO_A_IRQn                   =  17,
  SPI_IRQn                      =  19,
  BLEB_IRQn                     =  20,
  BLEL_IRQn                     =  21,
  USB_IRQn                      =  22,
  TMR_IRQn                      =  24,
  UART_IRQn                     =  27,
  RTC_IRQn                      =  28,
  CMP_IRQn                      =  29,
  I2C_IRQn                      =  30,
  PWMX_IRQn                     =  31,
  KEYSCAN_IRQn                  =  33,
  ENCODE_IRQn                   =  34,
  WDOG_BAT_IRQn                 =  35
} IRQn_Type;
#endif

#ifdef __cplusplus
}
#endif

#endif

#ifndef __CH572USBSFR_H__
#define __CH572USBSFR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define USB_BASE_ADDR              (0x40008000)
#define BA_USB              ((PUINT8V)0x40008000)

#define R32_USB_CONTROL     (*((PUINT32V)0x40008000))
#define R8_USB_CTRL         (*((PUINT8V)0x40008000))
#define  RB_UC_HOST_MODE    0x80
#define  RB_UC_LOW_SPEED    0x40
#define  RB_UC_DEV_PU_EN    0x20
#define  RB_UC_SYS_CTRL1    0x20
#define  RB_UC_SYS_CTRL0    0x10
#define  MASK_UC_SYS_CTRL   0x30

#define  RB_UC_INT_BUSY     0x08
#define  RB_UC_RESET_SIE    0x04
#define  RB_UC_CLR_ALL      0x02
#define  RB_UC_DMA_EN       0x01

#define R8_UDEV_CTRL        (*((PUINT8V)0x40008001))
#define  RB_UD_PD_DIS       0x80
#define  RB_UD_DP_PIN       0x20
#define  RB_UD_DM_PIN       0x10
#define  RB_UD_LOW_SPEED    0x04
#define  RB_UD_GP_BIT       0x02
#define  RB_UD_PORT_EN      0x01

#define R8_UHOST_CTRL       R8_UDEV_CTRL
#define  RB_UH_PD_DIS       0x80
#define  RB_UH_DP_PIN       0x20
#define  RB_UH_DM_PIN       0x10
#define  RB_UH_LOW_SPEED    0x04
#define  RB_UH_BUS_RESET    0x02
#define  RB_UH_PORT_EN      0x01

#define R8_USB_INT_EN       (*((PUINT8V)0x40008002))
#define  RB_UIE_DEV_SOF     0x80
#define  RB_UIE_DEV_NAK     0x40
#define  RB_MOD_1_WIRE      0x20
#define  RB_UIE_FIFO_OV     0x10
#define  RB_UIE_HST_SOF     0x08
#define  RB_UIE_SUSPEND     0x04
#define  RB_UIE_TRANSFER    0x02
#define  RB_UIE_DETECT      0x01
#define  RB_UIE_BUS_RST     0x01

#define R8_USB_DEV_AD       (*((PUINT8V)0x40008003))
#define  RB_UDA_GP_BIT      0x80
#define  MASK_USB_ADDR      0x7F

#define R32_USB_STATUS      (*((PUINT32V)0x40008004))
#define R8_USB_MIS_ST       (*((PUINT8V)0x40008005))
#define  RB_UMS_SOF_PRES    0x80
#define  RB_UMS_SOF_ACT     0x40
#define  RB_UMS_SIE_FREE    0x20
#define  RB_UMS_R_FIFO_RDY  0x10
#define  RB_UMS_BUS_RESET   0x08
#define  RB_UMS_SUSPEND     0x04
#define  RB_UMS_DM_LEVEL    0x02
#define  RB_UMS_DEV_ATTACH  0x01

#define R8_USB_INT_FG       (*((PUINT8V)0x40008006))
#define  RB_U_IS_NAK        0x80
#define  RB_U_TOG_OK        0x40
#define  RB_U_SIE_FREE      0x20
#define  RB_UIF_FIFO_OV     0x10
#define  RB_UIF_HST_SOF     0x08
#define  RB_UIF_SUSPEND     0x04
#define  RB_UIF_TRANSFER    0x02
#define  RB_UIF_DETECT      0x01
#define  RB_UIF_BUS_RST     0x01

#define R8_USB_INT_ST       (*((PUINT8V)0x40008007))
#define  RB_UIS_SETUP_ACT   0x80
#define  RB_UIS_TOG_OK      0x40
#define  RB_UIS_TOKEN1      0x20
#define  RB_UIS_TOKEN0      0x10
#define  MASK_UIS_TOKEN     0x30
#define  UIS_TOKEN_OUT      0x00
#define  UIS_TOKEN_SOF      0x10
#define  UIS_TOKEN_IN       0x20
#define  UIS_TOKEN_SETUP    0x30

#define  MASK_UIS_ENDP      0x0F
#define  MASK_UIS_H_RES     0x0F

#define R8_USB_RX_LEN       (*((PUINT8V)0x40008008))
#define R32_USB_BUF_MODE    (*((PUINT32V)0x4000800C))
#define R8_UEP4_1_MOD       (*((PUINT8V)0x4000800C))
#define  RB_UEP1_RX_EN      0x80
#define  RB_UEP1_TX_EN      0x40
#define  RB_UEP1_BUF_MOD    0x10

#define  RB_UEP4_RX_EN      0x08
#define  RB_UEP4_TX_EN      0x04

#define R8_UEP2_3_MOD       (*((PUINT8V)0x4000800D))
#define  RB_UEP3_RX_EN      0x80
#define  RB_UEP3_TX_EN      0x40
#define  RB_UEP3_BUF_MOD    0x10
#define  RB_UEP2_RX_EN      0x08
#define  RB_UEP2_TX_EN      0x04
#define  RB_UEP2_BUF_MOD    0x01

#define R8_UEP567_MOD       (*((PUINT8V)0x4000800E))
#define  RB_UEP7_RX_EN      0x20
#define  RB_UEP7_TX_EN      0x10
#define  RB_UEP6_RX_EN      0x08
#define  RB_UEP6_TX_EN      0x04
#define  RB_UEP5_RX_EN      0x02
#define  RB_UEP5_TX_EN      0x01

#define R8_UH_EP_MOD        R8_UEP2_3_MOD
#define  RB_UH_EP_TX_EN     0x40
#define  RB_UH_EP_TBUF_MOD  0x10

#define  RB_UH_EP_RX_EN     0x08
#define  RB_UH_EP_RBUF_MOD  0x01

#define R16_UEP0_DMA        (*((PUINT16V)0x40008010))
#define R16_UEP1_DMA        (*((PUINT16V)0x40008014))
#define R16_UEP2_DMA        (*((PUINT16V)0x40008018))
#define R16_UH_RX_DMA       R16_UEP2_DMA
#define R16_UEP3_DMA        (*((PUINT16V)0x4000801C))
#define R16_UH_TX_DMA       R16_UEP3_DMA
#define R16_UEP5_DMA        (*((PUINT16V)0x40008054))
#define R16_UEP6_DMA        (*((PUINT16V)0x40008058))
#define R16_UEP7_DMA        (*((PUINT16V)0x4000805C))
#define R32_USB_EP0_CTRL    (*((PUINT32V)0x40008020))
#define R8_UEP0_T_LEN       (*((PUINT8V)0x40008020))
#define R8_UEP0_CTRL        (*((PUINT8V)0x40008022))
#define R32_USB_EP1_CTRL    (*((PUINT32V)0x40008024))
#define R8_UEP1_T_LEN       (*((PUINT8V)0x40008024))
#define R8_UEP1_CTRL        (*((PUINT8V)0x40008026))
#define  RB_UEP_R_TOG       0x80
#define  RB_UEP_T_TOG       0x40
#define  RB_UEP_AUTO_TOG    0x10
#define  RB_UEP_R_RES1      0x08
#define  RB_UEP_R_RES0      0x04
#define  MASK_UEP_R_RES     0x0C
#define  UEP_R_RES_ACK      0x00
#define  UEP_R_RES_TOUT     0x04
#define  UEP_R_RES_NAK      0x08
#define  UEP_R_RES_STALL    0x0C

#define  RB_UEP_T_RES1      0x02
#define  RB_UEP_T_RES0      0x01
#define  MASK_UEP_T_RES     0x03
#define  UEP_T_RES_ACK      0x00
#define  UEP_T_RES_TOUT     0x01
#define  UEP_T_RES_NAK      0x02
#define  UEP_T_RES_STALL    0x03

#define R8_UH_SETUP         R8_UEP1_CTRL
#define  RB_UH_PRE_PID_EN   0x80
#define  RB_UH_SOF_EN       0x40

#define R32_USB_EP2_CTRL    (*((PUINT32V)0x40008028))
#define R8_UEP2_T_LEN       (*((PUINT8V)0x40008028))
#define R8_UEP2_CTRL        (*((PUINT8V)0x4000802A))

#define R8_UH_EP_PID        R8_UEP2_T_LEN
#define  MASK_UH_TOKEN      0xF0
#define  MASK_UH_ENDP       0x0F

#define R8_UH_RX_CTRL       R8_UEP2_CTRL
#define  RB_UH_R_TOG        0x80
#define  RB_UH_R_AUTO_TOG   0x10
#define  RB_UH_R_RES        0x04

#define R32_USB_EP3_CTRL    (*((PUINT32V)0x4000802c))
#define R8_UEP3_T_LEN       (*((PUINT8V)0x4000802c))
#define R8_UEP3_CTRL        (*((PUINT8V)0x4000802e))
#define R8_UH_TX_LEN        R8_UEP3_T_LEN

#define R8_UH_TX_CTRL       R8_UEP3_CTRL
#define  RB_UH_T_TOG        0x40
#define  RB_UH_T_AUTO_TOG   0x10
#define  RB_UH_T_RES        0x01

#define R32_USB_EP4_CTRL    (*((PUINT32V)0x40008030))
#define R8_UEP4_T_LEN       (*((PUINT8V)0x40008030))
#define R8_UEP4_CTRL        (*((PUINT8V)0x40008032))

#define R32_USB_EP5_CTRL    (*((PUINT32V)0x40008064))
#define R8_UEP5_T_LEN       (*((PUINT8V)0x40008064))
#define R8_UEP5_CTRL        (*((PUINT8V)0x40008066))

#define R32_USB_EP6_CTRL    (*((PUINT32V)0x40008068))
#define R8_UEP6_T_LEN       (*((PUINT8V)0x40008068))
#define R8_UEP6_CTRL        (*((PUINT8V)0x4000806A))

#define R32_USB_EP7_CTRL    (*((PUINT32V)0x4000806C))
#define R8_UEP7_T_LEN       (*((PUINT8V)0x4000806C))
#define R8_UEP7_CTRL        (*((PUINT8V)0x4000806E))

#ifdef __cplusplus
}
#endif

#endif

#ifndef __USB_TYPE__
#define __USB_TYPE__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USB_PID_SETUP
#define USB_PID_NULL            0x00
#define USB_PID_SOF             0x05
#define USB_PID_SETUP           0x0D
#define USB_PID_IN              0x09
#define USB_PID_OUT             0x01
#define USB_PID_ACK             0x02
#define USB_PID_NAK             0x0A
#define USB_PID_STALL           0x0E
#define USB_PID_DATA0           0x03
#define USB_PID_DATA1           0x0B
#define USB_PID_PRE             0x0C
#endif

#ifndef USB_GET_DESCRIPTOR
#define USB_GET_STATUS          0x00
#define USB_CLEAR_FEATURE       0x01
#define USB_SET_FEATURE         0x03
#define USB_SET_ADDRESS         0x05
#define USB_GET_DESCRIPTOR      0x06
#define USB_SET_DESCRIPTOR      0x07
#define USB_GET_CONFIGURATION   0x08
#define USB_SET_CONFIGURATION   0x09
#define USB_GET_INTERFACE       0x0A
#define USB_SET_INTERFACE       0x0B
#define USB_SYNCH_FRAME         0x0C
#endif

#ifndef HUB_GET_DESCRIPTOR
#define HUB_GET_STATUS          0x00
#define HUB_CLEAR_FEATURE       0x01
#define HUB_GET_STATE           0x02
#define HUB_SET_FEATURE         0x03
#define HUB_GET_DESCRIPTOR      0x06
#define HUB_SET_DESCRIPTOR      0x07
#endif

#ifndef HID_GET_REPORT
#define HID_GET_REPORT          0x01
#define HID_GET_IDLE            0x02
#define HID_GET_PROTOCOL        0x03
#define HID_SET_REPORT          0x09
#define HID_SET_IDLE            0x0A
#define HID_SET_PROTOCOL        0x0B
#endif

#ifndef USB_REQ_TYP_MASK
#define USB_REQ_TYP_IN          0x80
#define USB_REQ_TYP_OUT         0x00
#define USB_REQ_TYP_READ        0x80
#define USB_REQ_TYP_WRITE       0x00
#define USB_REQ_TYP_MASK        0x60
#define USB_REQ_TYP_STANDARD    0x00
#define USB_REQ_TYP_CLASS       0x20
#define USB_REQ_TYP_VENDOR      0x40
#define USB_REQ_TYP_RESERVED    0x60
#define USB_REQ_RECIP_MASK      0x1F
#define USB_REQ_RECIP_DEVICE    0x00
#define USB_REQ_RECIP_INTERF    0x01
#define USB_REQ_RECIP_ENDP      0x02
#define USB_REQ_RECIP_OTHER     0x03
#endif

#ifndef HUB_GET_HUB_DESCRIPTOR
#define HUB_CLEAR_HUB_FEATURE   0x20
#define HUB_CLEAR_PORT_FEATURE  0x23
#define HUB_GET_BUS_STATE       0xA3
#define HUB_GET_HUB_DESCRIPTOR  0xA0
#define HUB_GET_HUB_STATUS      0xA0
#define HUB_GET_PORT_STATUS     0xA3
#define HUB_SET_HUB_DESCRIPTOR  0x20
#define HUB_SET_HUB_FEATURE     0x20
#define HUB_SET_PORT_FEATURE    0x23
#endif

#ifndef HUB_PORT_RESET
#define HUB_C_HUB_LOCAL_POWER   0
#define HUB_C_HUB_OVER_CURRENT  1
#define HUB_PORT_CONNECTION     0
#define HUB_PORT_ENABLE         1
#define HUB_PORT_SUSPEND        2
#define HUB_PORT_OVER_CURRENT   3
#define HUB_PORT_RESET          4
#define HUB_PORT_POWER          8
#define HUB_PORT_LOW_SPEED      9
#define HUB_C_PORT_CONNECTION   16
#define HUB_C_PORT_ENABLE       17
#define HUB_C_PORT_SUSPEND      18
#define HUB_C_PORT_OVER_CURRENT 19
#define HUB_C_PORT_RESET        20
#endif

#ifndef USB_DESCR_TYP_DEVICE
#define USB_DESCR_TYP_DEVICE    0x01
#define USB_DESCR_TYP_CONFIG    0x02
#define USB_DESCR_TYP_STRING    0x03
#define USB_DESCR_TYP_INTERF    0x04
#define USB_DESCR_TYP_ENDP      0x05
#define USB_DESCR_TYP_QUALIF    0x06
#define USB_DESCR_TYP_SPEED     0x07
#define USB_DESCR_TYP_OTG       0x09
#define USB_DESCR_TYP_HID       0x21
#define USB_DESCR_TYP_REPORT    0x22
#define USB_DESCR_TYP_PHYSIC    0x23
#define USB_DESCR_TYP_CS_INTF   0x24
#define USB_DESCR_TYP_CS_ENDP   0x25
#define USB_DESCR_TYP_HUB       0x29
#endif

#ifndef USB_DEV_CLASS_HUB
#define USB_DEV_CLASS_RESERVED  0x00
#define USB_DEV_CLASS_AUDIO     0x01
#define USB_DEV_CLASS_COMMUNIC  0x02
#define USB_DEV_CLASS_HID       0x03
#define USB_DEV_CLASS_MONITOR   0x04
#define USB_DEV_CLASS_PHYSIC_IF 0x05
#define USB_DEV_CLASS_POWER     0x06
#define USB_DEV_CLASS_PRINTER   0x07
#define USB_DEV_CLASS_STORAGE   0x08
#define USB_DEV_CLASS_HUB       0x09
#define USB_DEV_CLASS_VEN_SPEC  0xFF
#endif

#ifndef USB_ENDP_TYPE_MASK
#define USB_ENDP_DIR_MASK       0x80
#define USB_ENDP_ADDR_MASK      0x0F
#define USB_ENDP_TYPE_MASK      0x03
#define USB_ENDP_TYPE_CTRL      0x00
#define USB_ENDP_TYPE_ISOCH     0x01
#define USB_ENDP_TYPE_BULK      0x02
#define USB_ENDP_TYPE_INTER     0x03
#endif

#ifndef USB_DEVICE_ADDR
#define USB_DEVICE_ADDR         0x02
#endif
#ifndef DEFAULT_ENDP0_SIZE
#define DEFAULT_ENDP0_SIZE      8
#endif
#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE         64
#endif
#ifndef USB_BO_CBW_SIZE
#define USB_BO_CBW_SIZE         0x1F
#define USB_BO_CSW_SIZE         0x0D
#endif
#ifndef USB_BO_CBW_SIG
#define USB_BO_CBW_SIG          0x43425355
#define USB_BO_CSW_SIG          0x53425355
#endif

#ifndef __PACKED
#define __PACKED               __attribute__((packed))
#endif

typedef struct __PACKED _USB_SETUP_REQ {
    UINT8 bRequestType;
    UINT8 bRequest;
    UINT16 wValue;
    UINT16 wIndex;
    UINT16 wLength;
} USB_SETUP_REQ, *PUSB_SETUP_REQ;

typedef struct __PACKED _USB_DEVICE_DESCR {
    UINT8 bLength;
    UINT8 bDescriptorType;
    UINT16 bcdUSB;
    UINT8 bDeviceClass;
    UINT8 bDeviceSubClass;
    UINT8 bDeviceProtocol;
    UINT8 bMaxPacketSize0;
    UINT16 idVendor;
    UINT16 idProduct;
    UINT16 bcdDevice;
    UINT8 iManufacturer;
    UINT8 iProduct;
    UINT8 iSerialNumber;
    UINT8 bNumConfigurations;
} USB_DEV_DESCR, *PUSB_DEV_DESCR;

typedef struct __PACKED _USB_CONFIG_DESCR {
    UINT8 bLength;
    UINT8 bDescriptorType;
    UINT16 wTotalLength;
    UINT8 bNumInterfaces;
    UINT8 bConfigurationValue;
    UINT8 iConfiguration;
    UINT8 bmAttributes;
    UINT8 MaxPower;
} USB_CFG_DESCR, *PUSB_CFG_DESCR;

typedef struct __PACKED _USB_INTERF_DESCR {
    UINT8 bLength;
    UINT8 bDescriptorType;
    UINT8 bInterfaceNumber;
    UINT8 bAlternateSetting;
    UINT8 bNumEndpoints;
    UINT8 bInterfaceClass;
    UINT8 bInterfaceSubClass;
    UINT8 bInterfaceProtocol;
    UINT8 iInterface;
} USB_ITF_DESCR, *PUSB_ITF_DESCR;

typedef struct __PACKED _USB_ENDPOINT_DESCR {
    UINT8 bLength;
    UINT8 bDescriptorType;
    UINT8 bEndpointAddress;
    UINT8 bmAttributes;
    UINT16 wMaxPacketSize;
    UINT8 bInterval;
} USB_ENDP_DESCR, *PUSB_ENDP_DESCR;

typedef struct __PACKED _USB_CONFIG_DESCR_LONG {
    USB_CFG_DESCR   cfg_descr;
    USB_ITF_DESCR   itf_descr;
    USB_ENDP_DESCR  endp_descr[1];
} USB_CFG_DESCR_LONG, *PUSB_CFG_DESCR_LONG;

typedef USB_CFG_DESCR_LONG *PXUSB_CFG_DESCR_LONG;

typedef struct __PACKED  _USB_HUB_DESCR {
    UINT8 bDescLength;
    UINT8 bDescriptorType;
    UINT8 bNbrPorts;
    UINT8 wHubCharacteristicsL;
    UINT8 wHubCharacteristicsH;
    UINT8 bPwrOn2PwrGood;
    UINT8 bHubContrCurrent;
    UINT8 DeviceRemovable;
    UINT8 PortPwrCtrlMask;
} USB_HUB_DESCR, *PUSB_HUB_DESCR;

typedef USB_HUB_DESCR  *PXUSB_HUB_DESCR;

typedef struct __PACKED  _USB_HID_DESCR {
    UINT8 bLength;
    UINT8 bDescriptorType;
    UINT16 bcdHID;
    UINT8 bCountryCode;
    UINT8 bNumDescriptors;
    UINT8 bDescriptorTypeX;
    UINT8 wDescriptorLengthL;
    UINT8 wDescriptorLengthH;
} USB_HID_DESCR, *PUSB_HID_DESCR;

typedef USB_HID_DESCR *PXUSB_HID_DESCR;

typedef struct __PACKED  _UDISK_BOC_CBW {
    UINT32 mCBW_Sig;
    UINT32 mCBW_Tag;
    UINT32 mCBW_DataLen;
    UINT8 mCBW_Flag;
    UINT8 mCBW_LUN;
    UINT8 mCBW_CB_Len;
    UINT8 mCBW_CB_Buf[16];
} UDISK_BOC_CBW, *PUDISK_BOC_CBW;

typedef UDISK_BOC_CBW  *PXUDISK_BOC_CBW;

typedef struct __PACKED  _UDISK_BOC_CSW {
    UINT32 mCSW_Sig;
    UINT32 mCSW_Tag;
    UINT32 mCSW_Residue;
    UINT8 mCSW_Status;
} UDISK_BOC_CSW, *PUDISK_BOC_CSW;

typedef UDISK_BOC_CSW  *PXUDISK_BOC_CSW;

#ifdef __cplusplus
}
#endif

#endif
