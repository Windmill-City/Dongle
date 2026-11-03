#include "rf.h"
#include "rf_uart_rx.h"
#include "usb_uart.h"

#define LED_PIN             GPIO_Pin_7
#define USB_ADDR_MASK       0x7F
#define LED_BLINK_TICKS     (50UL * 1000UL * 1000UL)

static void LED_Init( void )
{
    GPIOA_ResetBits( LED_PIN );
    GPIOA_ModeCfg( LED_PIN, GPIO_ModeOut_PP_5mA );
}

__attribute__((used))
__HIGH_CODE
void process_main( void )
{
    uint32_t tick_now;
    uint32_t tick_last = SYS_GetSysTickCnt( );
    uint8_t  led_on    = 0;
    uint8_t  usb_ready = 0;

    while(1)
    {
        USB_StatusQuery( );

        tick_now = SYS_GetSysTickCnt( );

        if( R8_USB_DEV_AD & USB_ADDR_MASK )
        {

            if( !usb_ready )
            {
                usb_ready = 1;
                led_on    = 1;
                GPIOA_SetBits( LED_PIN );
            }
        }
        else
        {
            usb_ready = 0;
            if( (uint32_t)( tick_now - tick_last ) >= LED_BLINK_TICKS )
            {
                tick_last = tick_now;
                led_on   ^= 1;
                if( led_on )
                {
                    GPIOA_SetBits( LED_PIN );
                }
                else
                {
                    GPIOA_ResetBits( LED_PIN );
                }
            }
        }
    }
}

int main(void)
{

    LED_Init( );
    GPIOA_SetBits( LED_PIN );

    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;
    HSECFG_Capacitance( HSECap_18p );
    SetSysClock( CLK_SOURCE_HSE_PLL_100MHz );

    __SysTick_Config( SysTick_LOAD_RELOAD_Msk );

#ifdef DEBUG
    GPIOA_SetBits( bTXD_0 );
    GPIOA_ModeCfg( bTXD_0, GPIO_ModeOut_PP_5mA );
    UART_Remap( ENABLE, UART_TX_REMAP_PA3, UART_RX_REMAP_PA2 );
    UART_DefInit( );
#endif
    PRINT("start.\n");
    PRINT("%s\n", VER_RF_LIB);
    USB_Init( );
    RFRole_Init( );
    RF_UartRxInit( );
    process_main( );
}
