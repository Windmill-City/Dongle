#include "ch57x_common.h"

uint32_t Freq_LSI = 0;

void LClk_Cfg(FunctionalState s)
{
    uint8_t cfg = R8_LSI_CONFIG;

    if(s == DISABLE)
    {
        cfg &= ~RB_CLK_LSI_PON;
    }
    else
    {
        cfg |= RB_CLK_LSI_PON;
    }

    sys_safe_access_enable();
    R8_LSI_CONFIG = cfg;
    sys_safe_access_disable();
}

void HSECFG_Current(HSECurrentTypeDef c)
{
    uint8_t x32M_c;

    x32M_c = R8_XT32M_TUNE;
    x32M_c = (x32M_c & 0xfc) | (c & 0x03);

    sys_safe_access_enable();
    R8_XT32M_TUNE = x32M_c;
    sys_safe_access_disable();
}

void HSECFG_Capacitance(HSECapTypeDef c)
{
    uint8_t x32M_c;

    x32M_c = R8_XT32M_TUNE;
    x32M_c = (x32M_c & 0x0f) | (c << 4);

    sys_safe_access_enable();
    R8_XT32M_TUNE = x32M_c;
    sys_safe_access_disable();
}

uint32_t RTC_InitClock(RTC_OSCCntTypeDef cnt)
{
    uint32_t count;
    uint32_t cyc;
    uint32_t last_ov_cnt = 0;
    uint32_t new_ov_cnt = 0;
    uint32_t ov_cnt_ov_cnt = 0;

    if(cnt<Count_32)
    {
        cyc = 1<<cnt;
    }
    else if(cnt<Count_1024)
    {
        cyc = 1<<(cnt+2);
    }
    else if(cnt<Count_2047)
    {
        cyc = 1024;
    }
    else
    {
        cyc = 2047;
    }
    R16_OSC_CAL_CNT |= RB_OSC_CAL_OV_CLR | RB_OSC_CAL_IF;
    sys_safe_access_enable();
    R8_OSC_CAL_CTRL |= RB_CNT_CLR;
    R8_OSC_CAL_CTRL = cnt;
    R8_OSC_CAL_CTRL |= RB_OSC_CNT_EN;
    sys_safe_access_disable();
    while(!(R16_OSC_CAL_CNT&RB_OSC_CAL_IF))
    {
        new_ov_cnt = R8_OSC_CAL_OV_CNT;
        if(new_ov_cnt<last_ov_cnt)
        {
            ov_cnt_ov_cnt++;
        }
        last_ov_cnt = new_ov_cnt;
    }
    count = ((uint32_t)R16_OSC_CAL_CNT&RB_OSC_CAL_CNT) + ((uint32_t)R8_OSC_CAL_OV_CNT+ov_cnt_ov_cnt*256)*16384;
    Freq_LSI = GetSysClock()/(count/cyc);
    return Freq_LSI;
}

void RTC_InitTime(uint16_t y, uint16_t mon, uint16_t d, uint16_t h, uint16_t m, uint16_t s)
{
    uint32_t         t;
    uint32_t         year, month, day, sec2;
    uint32_t         t32k;
    volatile uint8_t clk_pin;
    uint32_t temp,temp1,temp2,temp3;
    uint32_t tmp,tmp1,tmp2;

    year = y;
    month = mon;
    day = 0;
    while(year > BEGYEAR)
    {
        day += YearLength(year - 1);
        year--;
    }
    while(month > 1)
    {
        day += monthLength(IsLeapYear(y), month - 2);
        month--;
    }

    day += d - 1;
    sec2 = (h % 24) * 1800 + m * 30 + s / 2;
    t32k = (s & 1) ? (Freq_LSI) : (0);
    t = sec2;
    t = t << 16 | t32k;

    temp = day * Freq_LSI;
    temp1 = temp % 32768;
    temp = temp / 32768;

    temp2 = temp1 * 675;
    temp3 = temp2 % 512;
    temp2 = temp2 / 512;

    temp1 = temp3 * 128;

    tmp = sec2 * Freq_LSI;
    tmp1 = tmp % 32768;
    tmp = tmp / 32768;

    tmp2 = tmp1 * 2 ;

    t32k = (t32k * Freq_LSI + 16384) / 32768;

    t32k += tmp2 + temp1;
    tmp += (t32k/65536) + temp2;
    temp += (tmp/43200);

    t32k %= 65536;
    tmp %= 43200;

    t = tmp;
    t = t << 16 | t32k;

    do
    {
        clk_pin = (R8_LSI_CONFIG & RB_LSI_CLK_PIN);
    } while(clk_pin != (R8_LSI_CONFIG & RB_LSI_CLK_PIN));
    if(!clk_pin)
    {
        while(!clk_pin)
        {
            do
            {
                clk_pin = (R8_LSI_CONFIG & RB_LSI_CLK_PIN);
            } while(clk_pin != (R8_LSI_CONFIG & RB_LSI_CLK_PIN));
        }
    }

    sys_safe_access_enable();
    R32_RTC_TRIG = temp;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_HI;
    sys_safe_access_disable();
    while((R32_RTC_TRIG & 0x3FFF) != (R32_RTC_CNT_DIV2 & 0x3FFF));
    sys_safe_access_enable();
    R32_RTC_TRIG = t;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_LO;
    sys_safe_access_disable();
}

void RTC_GetTime(uint16_t *py, uint16_t *pmon, uint16_t *pd, uint16_t *ph, uint16_t *pm, uint16_t *ps)
{
    uint32_t t;
    uint32_t day, sec2, t32k;
    uint32_t temp,temp1,temp2,temp3;
    uint32_t tmp,tmp1,tmp2;

    day = R32_RTC_CNT_DIV2 & 0x3FFF;
    sec2 = R16_RTC_CNT_DIV1;
    t32k = R16_RTC_CNT_LSI;

    temp = day * 32768;
    temp1 = temp % Freq_LSI;
    temp = temp / Freq_LSI;

    temp2 = temp1 * 43200;
    temp3 = temp2 % Freq_LSI;
    temp2 = temp2 / Freq_LSI;

    temp1 = (temp3 * 65536 + Freq_LSI/2 ) / Freq_LSI;

    tmp = sec2 * 32768;
    tmp1 = tmp % Freq_LSI;
    tmp = tmp / Freq_LSI;

    tmp2 = (tmp1 * 65536 + Freq_LSI/2 ) / Freq_LSI;

    t32k = (t32k * 32768 + Freq_LSI/2 ) / Freq_LSI;

    t32k += tmp2 + temp1;
    tmp += (t32k/65536) + temp2;
    temp += (tmp/43200);

    t32k %= 65536;
    tmp %= 43200;

    t = tmp * 2 + ((t32k < 0x8000) ? 0 : 1);

    *py = BEGYEAR;
    while(temp >= YearLength(*py))
    {
        temp -= YearLength(*py);
        (*py)++;
    }

    *pmon = 0;
    while(temp >= monthLength(IsLeapYear(*py), *pmon))
    {
        temp -= monthLength(IsLeapYear(*py), *pmon);
        (*pmon)++;
    }
    (*pmon)++;
    *pd = temp + 1;
    *ph = t / 3600;
    *pm = t % 3600 / 60;
    *ps = t % 60;
}

void RTC_SetCycleLSI(uint32_t cyc)
{
    volatile uint8_t clk_pin;

    do
    {
        clk_pin = (R8_LSI_CONFIG & RB_LSI_CLK_PIN);
    } while((clk_pin != (R8_LSI_CONFIG & RB_LSI_CLK_PIN)) || (!clk_pin));

    sys_safe_access_enable();
    R32_RTC_TRIG = cyc;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_LO;
    sys_safe_access_disable();
}

uint32_t RTC_GetCycleLSI(void)
{
    volatile uint32_t i;

    do
    {
        i = R32_RTC_CNT_LSI;
    } while(i != R32_RTC_CNT_LSI);

    return (i);
}

void RTC_TMRFunCfg(RTC_TMRCycTypeDef t)
{
    sys_safe_access_enable();
    R8_RTC_MODE_CTRL &= ~(RB_RTC_TMR_EN | RB_RTC_TMR_MODE);
    sys_safe_access_disable();
    sys_safe_access_enable();
    R8_RTC_MODE_CTRL |= RB_RTC_TMR_EN | (t);
    sys_safe_access_disable();
}

void RTC_TRIGFunCfg(uint32_t cyc)
{
    uint32_t t;

    t = RTC_GetCycleLSI() + cyc;
    if(t > RTC_MAX_COUNT)
    {
        t -= RTC_MAX_COUNT;
    }

    sys_safe_access_enable();
    R32_RTC_TRIG = t;
    R8_RTC_MODE_CTRL |= RB_RTC_TRIG_EN;
    sys_safe_access_disable();
}

void RTC_ModeFunDisable(RTC_MODETypeDef m)
{
    uint8_t i = 0;

    if(m == RTC_TRIG_MODE)
    {
        i |= RB_RTC_TRIG_EN;
    }
    else if(m == RTC_TMR_MODE)
    {
        i |= RB_RTC_TMR_EN;
    }

    sys_safe_access_enable();
    R8_RTC_MODE_CTRL &= ~(i);
    sys_safe_access_disable();
}

uint8_t RTC_GetITFlag(RTC_EVENTTypeDef f)
{
    if(f == RTC_TRIG_EVENT)
    {
        return (R8_RTC_FLAG_CTRL & RB_RTC_TRIG_FLAG);
    }
    else
    {
        return (R8_RTC_FLAG_CTRL & RB_RTC_TMR_FLAG);
    }
}

void RTC_ClearITFlag(RTC_EVENTTypeDef f)
{
    switch(f)
    {
        case RTC_TRIG_EVENT:
            R8_RTC_FLAG_CTRL = RB_RTC_TRIG_CLR;
            break;
        case RTC_TMR_EVENT:
            R8_RTC_FLAG_CTRL = RB_RTC_TMR_CLR;
            break;
        default:
            break;
    }
}
