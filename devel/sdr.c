#include "common.h"

#include <rtl-sdr.h>

char *progname = "sdr";

void list_devices(void);

int main(int argc, char **argv)
{
    list_devices();
    return 0;
}





// list devices

void list_devices(void)
{
    int dev_cnt, rc;

    dev_cnt = rtlsdr_get_device_count();
    NOTICE("dev_cnt = %d\n", dev_cnt);
    if (dev_cnt == 0) {
        ERROR("dev_cnt = 0\n");
        exit(1);
    }

    for (int i = 0; i < dev_cnt; i++) {
        char manufact[256], product[256], serial[256];

        rc = rtlsdr_get_device_usb_strings(0, manufact, product, serial);
        if (rc != 0) {
            ERROR("rtlsdr_get_device_usb_strings(%d) rc=%d\n", i, rc);
            exit(1);
        }
        NOTICE("name='%s'  manufact='%s'  product='%s'  serial='%s'\n",
               rtlsdr_get_device_name(i), manufact, product, serial);
    }
}

