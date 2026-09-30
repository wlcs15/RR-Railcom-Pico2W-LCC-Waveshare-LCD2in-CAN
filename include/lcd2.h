#ifndef RR_LCD2_H
#define RR_LCD2_H
enum {
    RR_WIFI_ICON_OFF = 0,
    RR_WIFI_ICON_SEARCH = 1,
    RR_WIFI_ICON_OK = 2,
    RR_WIFI_ICON_FAIL = 3
};
enum {
    RR_SVC_ICON_DIM = 0,
    RR_SVC_ICON_OK = 1,
    RR_SVC_ICON_FAIL = 2
};
void rr_lcd2_init(void);
void rr_lcd2_status(int wifi, int lcc, int jmri);
#endif
