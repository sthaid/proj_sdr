#ifndef __SDR_H__
#define __SDR_H__

void sdr_list_devices(void);
void sdr_init(double f, void(*cb_arg)(unsigned char *iq, size_t len));
void sdr_set_freq(double f);

#endif
