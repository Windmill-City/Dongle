#ifndef __CH57X_TIMER_H__
#define __CH57X_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define TMR_IT_CYC_END     0x01

#define TMR_ITCfg(s, f)    ((s) ? (R8_TMR_INTER_EN |= (f)) : (R8_TMR_INTER_EN &= ~(f)))

#ifdef __cplusplus
}
#endif

#endif
