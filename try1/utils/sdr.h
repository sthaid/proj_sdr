#ifndef __SDR_H__
#define __SDR_H__

void sdr_list_devices(void);

void sdr_init(int dev_idx, int sample_rate);
void sdr_print_info(void);

void sdr_test(int dev_idx, int sample_rate);

void sdr_get_data(double ctr_freq, complex *buff, int n);

//void sdr_set_freq(double f);

#endif
