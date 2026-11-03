#include "rf.h"
#include "rf_uart_rx.h"
#include "usb_uart.h"

#define LED_PIN             GPIO_Pin_7
#define USB_ADDR_MASK       0x7F
#define LED_BLINK_TICKS     (50UL * 1000UL * 1000UL)

static void led_init(void)
{
    GPIOA_ResetBits(LED_PIN);
    GPIOA_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);
}

static void led_set(uint8_t on)
{
    if (on)
    {
        GPIOA_SetBits(LED_PIN);
    }
    else
    {
        GPIOA_ResetBits(LED_PIN);
    }
}

static void main_loop(void)
{
    uint32_t tick_now;
    uint32_t tick_last = SYS_GetSysTickCnt();
    uint8_t led_on = 0;
    uint8_t usb_ready = 0;

    while (1)
    {
        usb_uart_poll();

        tick_now = SYS_GetSysTickCnt();

        if (R8_USB_DEV_AD & USB_ADDR_MASK)
        {
            if (!usb_ready)
            {
                usb_ready = 1;
                led_on = 1;
                led_set(led_on);
            }
        }
        else
        {
            usb_ready = 0;
            if ((uint32_t)(tick_now - tick_last) >= LED_BLINK_TICKS)
            {
                tick_last = tick_now;
                led_on ^= 1;
                led_set(led_on);
            }
        }
    }
}

int main(void)
{
    led_init();
    led_set(1);

    R16_PIN_ALTERNATE &= ~RB_PIN_DEBUG_EN;
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(CLK_SOURCE_HSE_PLL_100MHz);

    __SysTick_Config(SysTick_LOAD_RELOAD_Msk);

    usb_uart_init();
    rf_role_init();
    rf_uart_rx_init();

    main_loop();
}
