#ifndef __CH572_RF_HPP__
#define __CH572_RF_HPP__

#include <stdint.h>

using bStatus_t = uint8_t;
using rfRole_States_t = uint32_t;

using pfnRfRoleProcess = void (*)(rfRole_States_t status, uint8_t id);

#define  VER_RF_FILE            "CH57x_RF_BASIC_LIB_V1.1"

#define BREAK_UINT32( var, ByteNum ) (uint8_t)((uint32_t)(((var) >>((ByteNum) * 8)) & 0x00FF))
#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)
#define HI_UINT8(a)  (((a) >> 4) & 0x0F)
#define LO_UINT8(a)  ((a) & 0x0F)
#define BUILD_UINT32(Byte0, Byte1, Byte2, Byte3) \
          ((uint32_t)(((uint32_t)(Byte0) & 0x00FF) \
          + (((uint32_t)(Byte1) & 0x00FF) << 8) \
          + (((uint32_t)(Byte2) & 0x00FF) << 16) \
          + (((uint32_t)(Byte3) & 0x00FF) << 24)))
#define BUILD_UINT16(loByte, hiByte) ((uint16_t)(((loByte) & 0x00FF)|(((hiByte) & 0x00FF)<<8)))

#define ACTIVE_LOW        !
#define ACTIVE_HIGH       !!

#ifndef BV
#define BV(n)      (1 << (n))
#endif

#ifndef BF
#define BF(x,b,s)  (((x) & (b)) >> (s))
#endif

#ifndef MIN
#define MIN(n,m)   (((n) < (m)) ? (n) : (m))
#endif

#ifndef MAX
#define MAX(n,m)   (((n) < (m)) ? (m) : (n))
#endif

#ifndef ABS
#define ABS(n)     (((n) < 0) ? -(n) : (n))
#endif

#define LL_TX_PWR_MINUS_25_DBM       0x01
#define LL_TX_PWR_MINUS_20_DBM       0x02
#define LL_TX_PWR_MINUS_15_DBM       0x03
#define LL_TX_PWR_MINUS_10_DBM       0x05
#define LL_TX_PWR_MINUS_8_DBM        0x07
#define LL_TX_PWR_MINUS_5_DBM        0x0A
#define LL_TX_PWR_MINUS_3_DBM        0x0C
#define LL_TX_PWR_MINUS_1_DBM        0x10
#define LL_TX_PWR_0_DBM              0x12
#define LL_TX_PWR_1_DBM              0x15
#define LL_TX_PWR_2_DBM              0x18
#define LL_TX_PWR_3_DBM              0x1B
#define LL_TX_PWR_4_DBM              0x1F
#define LL_TX_PWR_5_DBM              0x25
#define LL_TX_PWR_6_DBM              0x2D

#define  RF_MAX_DATA_LEN             251

#define  BB_WHITENING_OFF            (1<<0)
#define  BB_WHITENING_CH             (1<<2)

#define PHY_MODE_MASK                (0x30)
#define  PHY_MODE_PHY_1M             (0<<4)
#define  PHY_MODE_PHY_2M             (1<<4)
#define  PHY_MODE_2G4                (3<<4)

struct rfRoleConfig_t
{
    pfnRfRoleProcess rfProcessCB;
    uint32_t processMask;
#define    RF_STATE_RX             (1<<0)
#define    RF_STATE_TIMEOUT        (1<<2)
#define    RF_STATE_RX_CRCERR      (1<<3)
#define    RF_STATE_TX_FINISH      (1<<4)
};

union TPROPERTIES_CFG
{
    struct
    {
        uint32_t whitOff               : 1;
        uint32_t resv0                 : 1;
        uint32_t whitChannel           : 1;
        uint32_t resv1                 : 1;
        uint32_t modePHY               : 2;
        uint32_t resv2                 :10;

        uint32_t lengthCrc             : 2;
        uint32_t ctlFiled              : 2;
        uint32_t lengthAA              : 2;
        uint32_t lengthPreamble        : 2;
        uint32_t dplEnable             : 1;
        uint32_t mode2G4               : 2;
#define  PHY_2G4_1M   1
#define  PHY_2G4_2M   0
        uint32_t bitOrderData          : 1;
        uint32_t resv3                 : 1;
        uint32_t resv4                 : 1;
        uint32_t crcXOREnable          : 1;
        uint32_t resv5                 : 1;
    };
    uint32_t cfgVal;
};

struct rfipTx_t
{
    uint32_t accessAddress;
    uint8_t accessAddressEx;
    uint32_t crcInit;
    uint32_t frequency;
    uint32_t properties;

    uint32_t txDMA;
    uint8_t whiteChannel;
    uint8_t txLen;
    uint8_t waitTime;
    int8_t txPowerVal;
    uint32_t crcPoly;
};

struct rfipRx_t
{
    uint32_t accessAddress;
    uint8_t accessAddressEx;
    uint32_t crcInit;
    uint32_t frequency;
    uint32_t properties;

    uint32_t rxDMA;
    uint8_t whiteChannel;
    uint8_t rxMaxLen;
    uint32_t timeOut;
    uint32_t crcPoly;
};

// Symbols provided by the prebuilt C library libch57x_rf.a.
extern "C"
{
extern const uint8_t VER_RF_LIB[];

void LLE_LibIRQHandler(void);

void BB_LibIRQHandler(void);

uint8_t RFIP_ReadCrc(void);

void RFIP_SetTxPower(uint8_t val);

bStatus_t RFRole_Stop(void);

bStatus_t RFRole_Shut(void);

void RFIP_Calibration(void);

void RFIP_WakeUpRegInit(void);

bStatus_t RFIP_SingleChannel(uint8_t ch);

void RFIP_TestEnd(void);

bStatus_t RFRole_BasicInit(rfRoleConfig_t *pConf);

bStatus_t RFIP_StartTx(rfipTx_t *pParm);

bStatus_t RFIP_SetRx(rfipRx_t *pParm);
}

#endif
