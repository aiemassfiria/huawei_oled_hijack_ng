#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

#include "oled.h"
#include "oled_font.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

extern void lcd_refresh_screen(struct lcd_screen*);
extern int lcd_control_operate(int);
extern int notify_handler_async(int subsystemid, int action, int subaction);

extern void put_pixel(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue);
extern void put_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t red, uint8_t green, uint8_t blue);
extern void put_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue);
extern void put_small_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue, char *text);
extern void put_large_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue, char *text);
extern void put_raw_buffer(uint8_t* from, uint32_t len);
extern int get_bytes_num_fit_by_width(uint8_t x, uint8_t w, uint8_t *text, uint8_t* font_widths);

extern uint32_t (*timer_create_ex)(uint32_t, uint32_t, void (*)(), uint32_t);
extern uint32_t (*timer_delete_ex)(uint32_t);

int process_is_alive();
int create_process(char* command, void (*finish_callback)(int, char *));
void destroy_process();
void destroy_process_pooler();

extern struct lcd_screen secret_screen;

uint32_t active_widget = 0;

// these are decared in the end
struct led_widget widgets[];
const uint32_t WIDGETS_SIZE;
const uint32_t USER_CUSTOM_SCRIPT_IDX;

uint32_t lcd_timer = 0;
uint32_t lcd_state = LED_ON;

void lcd_turn_on() {
    if (lcd_state != LED_ON) {
        lcd_state = LED_ON;
        lcd_control_operate(lcd_state);
    }
}

void lcd_turn_off() {
    if (lcd_state != LED_SLEEP) {
        lcd_state = LED_SLEEP;
        lcd_control_operate(lcd_state);
    }
}

void reschedule_lcd_timer() {
    if (lcd_timer) {
        timer_delete_ex(lcd_timer);
        lcd_timer = 0;
    }

    lcd_timer = timer_create_ex(widgets[active_widget].lcd_sleep_ms, 0, lcd_turn_off, 0);
}

void clear_screen() {
    put_rect(0, 0, lcd_width, lcd_height, 0, 0, 0);
}

void repaint() {
    clear_screen();
    if(widgets[active_widget].paint) {
        widgets[active_widget].paint();
    }
    lcd_refresh_screen(&secret_screen);
}

void enter_widget(uint32_t num) {
    if (num >= WIDGETS_SIZE) {
        fprintf(stderr, "Attempted to switch to non-existing widget %d\n", num);
        return;
    }

    // do not deinit active widget for better user experience
    active_widget = num;
    widgets[active_widget].init();
    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void leave_widget() {
    if (widgets[active_widget].deinit) {
        widgets[active_widget].deinit();
    }

    destroy_process_pooler();
    destroy_process();

    active_widget = widgets[active_widget].parent_idx;
    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void reset_widgets() {
    for(uint32_t widget = 0; widget < WIDGETS_SIZE; widget += 1) {
        if(widgets[widget].deinit) {
            widgets[widget].deinit();
        }
    }
    active_widget = 0;
    enter_widget(active_widget);
}

void dispatch_power_key() {
    if(widgets[active_widget].power_key_handler) {
        widgets[active_widget].power_key_handler();
    }

    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void dispatch_menu_key() {
    if(widgets[active_widget].menu_key_handler) {
        widgets[active_widget].menu_key_handler();
    }

    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}


// ---------------------------------- THE MAIN WIDGET ---------------------

uint8_t main_current_item;

char *main_lines[] = {
    "<- Back",
    "Signal info",
    "Radio mode",
    "SMS & USSD",
    "Wi-Fi",
    "Speedtest",
    "TTL & IMEI",
    "Disable battery",
    "Add SSH key",
    "ADB daemon",
    "Matrix",
    "Video",
    "Snake",
    "User scripts",
};

char main_lines_num = 14;

void main_init() {
    main_current_item = 0;
}

void main_paint() {
    const int lines_per_page = is_small_screen ? 4 : 7;

    const int page_first_item = main_current_item / lines_per_page * lines_per_page;

    for (int i = 0; i < lines_per_page && page_first_item + i < main_lines_num; i += 1) {
        char* cur_line = main_lines[page_first_item + i];
        uint8_t y = 3 + i * 15;

        if (page_first_item + i != 0) {
            y += 3;
        }

        if (page_first_item + i == main_current_item) {
            put_small_text(4, y, lcd_width, lcd_height, 255,0,255, "#");
        }

        put_small_text(20, y, lcd_width, lcd_height, 255,255,255, cur_line);
    }

    if (page_first_item + lines_per_page < main_lines_num) {
        if (is_small_screen) {
            put_small_text(118, 54, lcd_width, lcd_height, 255, 255, 255, SMALL_FONT_TRIANGLE);
        } else {
            put_small_text(20, 112, lcd_width, lcd_height, 255, 255, 255, SMALL_FONT_TRIANGLE);
        }
    }

}

void main_power_key_pressed() {
    switch(main_current_item) {
        case 0:
            notify_handler_async(SUBSYSTEM_GPIO, BUTTON_LONGMENU, 0);
            break;
        case 1:
            enter_widget(1);
            break;
        case 2:
            enter_widget(2);
            break;
        case 3:
            enter_widget(3);
            break;
        case 4:
            enter_widget(4);
            break;
        case 5:
            enter_widget(5);
            break;
        case 6:
            enter_widget(6);
            break;
        case 7:
            enter_widget(7);
            break;
        case 8:
            enter_widget(8);
            break;
        case 9:
            enter_widget(9);
            break;
        case 10:
            enter_widget(10);
            break;
        case 11:
            enter_widget(11);
            break;
        case 12:
            enter_widget(12);
            break;
        case 13:
            enter_widget(13);
            break;
    }
}

void main_menu_key_pressed() {
    main_current_item += 1;
    if (main_current_item >= main_lines_num) {
        main_current_item = 0;
    }
}

// ---------------------------------- MOBILE SIGNAL --------------------------

uint32_t mobile_timer = 0;

uint8_t mobile_tab_num = 0;

int32_t mobile_rssi = 0;
int32_t mobile_rsrq = 0;
int32_t mobile_rsrp = 0;
int32_t mobile_sinr = 0;
int32_t mobile_rscp = 0;
int32_t mobile_ecio = 0;
int32_t mobile_ul_bw = 0;
int32_t mobile_dl_bw = 0;
int32_t mobile_band = 0;
int32_t mobile_ca = -1;

const int32_t MAX_LAST_RSSI = 128;
int32_t last_rssi[MAX_LAST_RSSI] = {};

int mobile_parse_ca(char *buf) {
    char* saveptr = 0;

    char *field = strtok_r(buf, ",", &saveptr);
    while(field) {
        int idx, ul_ca_on, dl_ca_on, ca_active;

        if (sscanf(field, "\"%d %d %d %d\"", &idx, &ul_ca_on, &dl_ca_on, &ca_active) != 4) {
            break;
        }
        if (ul_ca_on || dl_ca_on) {
            return idx;
        }
        field = strtok_r(NULL, ",", &saveptr);
    }
    return -1;
}

void mobile_process_callback(int good, char *buf) {
    if (!good) {
        return;
    }

    mobile_rssi = mobile_rsrq = mobile_rsrp = mobile_sinr = mobile_rscp = mobile_ecio = 0;
    mobile_ul_bw = mobile_dl_bw = mobile_band = 0;
    mobile_ca = -1;

    char ca_buf[128] = {0};

    int offset = 0;

    while (buf[offset]) {
        int32_t val;

        if (sscanf(&buf[offset], "<rssi>&gt;=%ddBm</rssi>", &val) == 1) {
            mobile_rssi = val;
        } else if (sscanf(&buf[offset], "<rssi>&lt;=%ddBm</rssi>", &val) == 1) {
            mobile_rssi = val;
        } else if (sscanf(&buf[offset], "<rssi>%ddBm</rssi>", &val) == 1) {
            mobile_rssi = val;
        } else if (sscanf(&buf[offset], "<rsrq>%ddB</rsrq>", &val) == 1) {
            mobile_rsrq = val;
        } else if (sscanf(&buf[offset], "<rsrp>%ddBm</rsrp>", &val) == 1) {
            mobile_rsrp = val;
        } else if (sscanf(&buf[offset], "<sinr>%ddB</sinr>", &val) == 1) {
            mobile_sinr = val;
        } else if (sscanf(&buf[offset], "<rscp>%ddBm</rscp>", &val) == 1) {
            mobile_rscp = val;
        } else if (sscanf(&buf[offset], "<ecio>%ddB</ecio>", &val) == 1) {
            mobile_ecio = val;
        } else if (sscanf(&buf[offset], "<ulbandwidth>%dMHz</ulbandwidth>", &val) == 1) {
            mobile_ul_bw = val;
        } else if (sscanf(&buf[offset], "<dlbandwidth>%dMHz</dlbandwidth>", &val) == 1) {
            mobile_dl_bw = val;
        } else if (sscanf(&buf[offset], "<band>%d</band>", &val) == 1) {
            mobile_band = val;
        } else if (strncmp(&buf[offset], "^LCACELL: ", strlen("^LCACELL: ")) == 0) {
            strncpy(ca_buf, &buf[offset + strlen("^LCACELL: ")], 128);
            mobile_ca = mobile_parse_ca(ca_buf);
        }

        while(buf[offset] != 0 && buf[offset] != '\n') {
            offset += 1;
        }

        if(buf[offset] == '\n') {
            offset += 1;
        }
    }

    if (mobile_rssi) {
        for(int i = MAX_LAST_RSSI - 1; i > 0; i -= 1) {
            last_rssi[i] = last_rssi[i - 1];
        }
        last_rssi[0] = mobile_rssi;
    }

    repaint();
}


void update_measurements() {
    char* cmd = "/app/hijack/bin/device_webhook_client device signal 1 1;atc 'AT^LCACELL?'";
    create_process(cmd, mobile_process_callback);
}

void init_measurements_callback(int isgood, char *buf) {
    UNUSED(isgood);
    UNUSED(buf);

    update_measurements();
    mobile_timer = timer_create_ex(1000, 1, update_measurements, 0);
}

void mobile_signal_init() {
    mobile_rssi = mobile_rsrq = mobile_rsrp = mobile_sinr = mobile_rscp = mobile_ecio = 0;
    mobile_ul_bw = mobile_dl_bw = mobile_band = 0;
    mobile_ca = -1;

    mobile_tab_num = 0;
    mobile_timer = 0;

    for (int i = 0; i < MAX_LAST_RSSI; i += 1) {
        last_rssi[i] = 0;
    }

    create_process("/system/xbin/atc AT^RSSI=1", init_measurements_callback);
}

void mobile_signal_deinit() {
    if (mobile_timer) {
        timer_delete_ex(mobile_timer);
        mobile_timer = 0;
    }
}

void mobile_print_val_colorized(int x, int y, int thresh1, int thresh2, int thresh3, int val, char* addition) {
    char buf[256];
    uint8_t r, g, b;

    snprintf(buf, 256, "%d", val);
    strcat(buf, addition);

    if (val > thresh1) {
        r = 0; g = 255; b = 0;
    } else if (val > thresh2) {
        r = 188; g = 255; b = 0;
    } else if (val > thresh3) {
        r = 255; g = 188; b = 0;
    } else {
        r = 255; g = 0; b = 0;
    }

    put_large_text(x, y, lcd_width, lcd_height, r, g, b, buf);
}

void mobile_put_pixel_colorized(int x, int y, int thresh1, int thresh2, int thresh3, int val) {
    uint8_t r, g, b;

    if (val > thresh1) {
        r = 0; g = 255; b = 0;
    } else if (val > thresh2) {
        r = 188; g = 255; b = 0;
    } else if (val > thresh3) {
        r = 255; g = 188; b = 0;
    } else {
        r = 255; g = 0; b = 0;
    }

    put_pixel(x, y, r, g, b);
}

void mobile_signal_text_paint() {
    int dy = 0;

    if (is_small_screen) {
        if (mobile_tab_num == 2) {
            dy = 63;
        }
    }

    const int H = 21;

    put_small_text(8, (H*0)+6-dy, lcd_width, lcd_height, 255, 255, 255, "RSSI");
    mobile_print_val_colorized(48, (H*0)+2-dy, -65, -75, -85, mobile_rssi, "dBm");

    if (mobile_rsrp != 0) {
        put_small_text(8, (H*1)+6-dy, lcd_width, lcd_height, 255, 255, 255, "RSRP");
        mobile_print_val_colorized(48, (H*1)+2-dy, -84, -102, -111, mobile_rsrp, "dBm");
    } else if (mobile_rscp != 0) {
        put_small_text(8, (H*1)+6-dy, lcd_width, lcd_height, 255, 255, 255, "RSCP");
        mobile_print_val_colorized(48, (H*1)+2-dy, -65, -75, -85, mobile_rscp, "dBm");
    }

    if (mobile_rsrq != 0) {
        put_small_text(8, (H*2)+6-dy, lcd_width, lcd_height, 255, 255, 255, "RSRQ");
        mobile_print_val_colorized(48, (H*2)+2-dy, -5, -9, -12, mobile_rsrq, "dB");
    } else if (mobile_ecio != 0) {
        put_small_text(8, (H*2)+6-dy, lcd_width, lcd_height, 255, 255, 255, "EC/IO");
        mobile_print_val_colorized(48, (H*2)+2-dy, -6, -9, -12, mobile_ecio, "dB");
    }

    if (is_small_screen && !mobile_sinr && !mobile_ul_bw && !mobile_dl_bw && !mobile_ul_bw) {
        put_small_text(8, (H*3)+6-dy, lcd_width, lcd_height, 255, 255, 255, "No 4G Info");
        return;
    }

    if (mobile_sinr != 0) {
        put_small_text(8, (H*3)+6-dy, lcd_width, lcd_height, 255, 255, 255, "SINR");
        mobile_print_val_colorized(48, (H*3)+2-dy, 12, 10, 7, mobile_sinr, "dB");
    }

    if (mobile_ul_bw != 0 && mobile_dl_bw != 0) {
        put_small_text(8, (H*4)+6-dy, lcd_width, lcd_height, 255, 255, 255, "BW");
        mobile_print_val_colorized(48, (H*4)+2-dy, 12, 10, 7, (mobile_ul_bw+mobile_dl_bw) / 2, "Mhz");
    }

    if (mobile_band) {
        char buf[256];
        if (mobile_ca == -1) {
            snprintf(buf, 256, "B%d", mobile_band);
        } else {
            snprintf(buf, 256, "B%d+CA%d", mobile_band, mobile_ca);
        }

        put_small_text(8, (H*5)+6-dy, lcd_width, lcd_height, 255, 255, 255, "Band");
        put_large_text(48, (H*5)+2-dy, lcd_width, lcd_height, 255, 255, 255, buf);
    }
}

uint8_t mobile_val_to_y(int32_t val) {
        if (val > -51) {
            val = -51;
        }

        if (val < -105) {
            val = -105;
        }

        uint8_t y;
        if (is_small_screen) {
            y = -(val + 51);
        } else {
            y = -(val + 51) * 2;
        }
        return y;
}

uint32_t mobile_y_to_val(uint8_t y) {
    if (is_small_screen) {
        return -y - 51;
    } else {
        return -y/2 - 51;
    }
}


void mobile_signal_graph_paint() {
    if (is_small_screen) {
        put_small_text(8, 51, lcd_width, lcd_height, 255, 255, 255, "RSSI");
        mobile_print_val_colorized(48, 47, -65, -75, -85, mobile_rssi, "dBm");
    } else {
        put_small_text(8, 113, lcd_width, lcd_height, 255, 255, 255, "RSSI");
        mobile_print_val_colorized(48, 109, -65, -75, -85, mobile_rssi, "dBm");
    }

    uint32_t prev_val = 0;

    for (int i = 0; i < MAX_LAST_RSSI-1; i += 1) {
        uint8_t x = lcd_width - i - 1;

        if (last_rssi[i] == 0 || last_rssi[i + 1] == 0) {
            continue;
        }

        uint8_t y_from = MIN(mobile_val_to_y(last_rssi[i]), mobile_val_to_y(last_rssi[i+1]));
        uint8_t y_to = MAX(mobile_val_to_y(last_rssi[i]), mobile_val_to_y(last_rssi[i+1]));

        if ((y_to - y_from) > 1) {
            // more smooth lines
            y_from += 1;
        }
        if ((y_to - y_from) > 2) {
            // more smooth lines
            y_to -= 1;
        }

        for (int y = y_from; y <= y_to; y += 1) {
            mobile_put_pixel_colorized(x, y, -65, -75, -85, mobile_y_to_val(y));
        }
    }
}


void mobile_signal_paint() {
    if (mobile_tab_num == 0) {
        mobile_signal_graph_paint();
    } else {
        mobile_signal_text_paint();
    }
}

void mobile_switch_mode() {
    if (is_small_screen) {
        mobile_tab_num = (mobile_tab_num + 1) % 3;
    } else {
        mobile_tab_num = (mobile_tab_num + 1) % 2;
    }
    repaint();
}


// ---------------------------- COMMON EXTERNAL MENU FUNCTIONS -------------------------
const uint8_t MAXMENUITEMS = 128;
const int MAXITEMLEN = 64;
int lines_per_page = 7;

void make_items_from_buf(char* buf, char items[][MAXITEMLEN]) {
    char *saveptr;
    strcpy(items[0], "item:<- Back");
    int item = 1;

    char *line = strtok_r(buf, "\n", &saveptr);
    while(line) {
        if (strncmp(line, "pagebreak:", 10) == 0) {
            int items_to_insert = lines_per_page - (item % lines_per_page);
            if (items_to_insert == lines_per_page) {
                items_to_insert = 0;
            }
            for (int i = 0; i < items_to_insert; i += 1) {
                strncpy(items[item], "text:", MAXITEMLEN);
                item += 1;
                if(item >= MAXMENUITEMS - 1) {
                    goto clear_rest_items;
                }
            }
        } else if (strncmp(line, "item:", 5) == 0) {
            if(strlen(line) >= MAXITEMLEN-1) {
                fprintf(stderr, "line is too long: %s, aborting parse\n", line);
                items[item][0] = 0;
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }
            strncpy(items[item], line, MAXITEMLEN);
            item += 1;
            if(item >= MAXMENUITEMS-1) {
                goto clear_rest_items;
            }
        } else if (strncmp(line, "text:", 5) == 0) {
            for (uint32_t pos = strlen("text:"); pos < strlen(line); ) {
                int w = get_bytes_num_fit_by_width(5, lcd_width, (uint8_t*)line+pos, SMALL_FONT_WIDTHS);
                if(line[pos + w] != 0) {
                    char *last_space_ptr = memrchr(line+pos, ' ', w);
                    if (last_space_ptr) {
                        w = last_space_ptr - (line+pos) + 1;
                        if(line[pos + w] == ' ') {
                            w += 1;
                        }
                    }
                }
                if(w == 0 || w + 5 >= MAXITEMLEN - 1) {
                    break;
                }

                strncpy(items[item], "text:", MAXITEMLEN);
                strncat(items[item], line+pos, MIN(w, MAXITEMLEN));
                pos += w;
                item += 1;
                if(item >= MAXMENUITEMS-1) {
                    goto clear_rest_items;
                }
            }

        }

        line = strtok_r(NULL, "\n", &saveptr);
    }
clear_rest_items:
    for (int i = item; i < MAXMENUITEMS; i += 1) {
        items[i][0] = 0;
    }
}

void init_menu(uint8_t* curr_item, char items[][MAXITEMLEN]) {
    lines_per_page = is_small_screen ? 4 : 7;
    *curr_item = 0;
    strcpy(items[0], "item:<- Back:");
    for (int i = 1; i < MAXMENUITEMS; i += 1) {
        items[i][0] = 0;
    }
}

void next_menu_item(uint8_t* curr_item, char items[][MAXITEMLEN]) {
    int first_item_on_page = *curr_item / lines_per_page * lines_per_page;
    int last_item_to_consider = first_item_on_page + 2*lines_per_page - 1;

    for(*curr_item += 1; *curr_item < last_item_to_consider; *curr_item += 1) {
        if (items[*curr_item][0] == 0 || *curr_item >= MAXMENUITEMS) {
            // this should be always the back button
            break;
        }

        // before the wrap, check if we saw the last page
        if (*curr_item+1 < MAXMENUITEMS && items[*curr_item+1][0] == 0 &&
            (*curr_item - first_item_on_page ) >= lines_per_page) {
            break;
        }
        if(strncmp(items[*curr_item], "item:", 5) == 0) {
            return;
        }
    }
    if (items[*curr_item][0] == 0 || *curr_item >= MAXMENUITEMS) {
        // this should be always the back button
        *curr_item = 0;
    }
}

void paint_menu(uint8_t curr_item, char items[][MAXITEMLEN]) {
    const int page_first_item = curr_item / lines_per_page * lines_per_page;

    int i;
    for (i = 0;
         i < lines_per_page && (page_first_item + i) < MAXMENUITEMS && items[page_first_item + i][0];
         i += 1)
    {
        char cur_line[MAXITEMLEN];
        strncpy(cur_line, items[page_first_item + i], MAXITEMLEN);

        int8_t y = 3 + i * 15;
        if (page_first_item == 0 && i > 0) {
            y += 3;
        }


        char *saveptr;
        char *item_type = strtok_r(cur_line, ":", &saveptr);
        if (!item_type) {
            continue;
        }

        int is_item = (strcmp(item_type, "item") == 0);

        if (page_first_item + i == curr_item && is_item) {
            put_small_text(5, y, lcd_width, lcd_height, 255,0,255, "#");
        }

        int8_t x;
        char *item_text;
        if (is_item) {
            x = 20;
            item_text = strtok_r(NULL, ":", &saveptr);
        } else {
            x = 5;
            item_text = strtok_r(NULL, "\n", &saveptr);
        }

        if(!item_text) {
            continue;
        }

        put_small_text(x, y, lcd_width, lcd_height, 255,255,255, item_text);
    }

    if (i == lines_per_page && (i+page_first_item) < MAXMENUITEMS && items[page_first_item + i][0]) {
        if (is_small_screen) {
            put_small_text(118, 54, lcd_width, lcd_height, 255, 255, 255, SMALL_FONT_TRIANGLE);
        } else {
            put_small_text(20, 112, lcd_width, lcd_height, 255, 255, 255, SMALL_FONT_TRIANGLE);
        }
    }
}

void menu_process_callback(int isgood, char* buf, uint8_t* curr_item, char items[][MAXITEMLEN]) {
    if(!isgood) {
        strcpy(items[0], "item:<- Back:");
        strcpy(items[1], "text:Call error");
        for(int i = 2; i < MAXMENUITEMS; i += 1) {
            items[i][0] = 0;
        }
    } else {
        make_items_from_buf(buf, items);
    }
    *curr_item = 0;
    repaint();
}

void execute_menu_item(uint8_t curr_item, char items[][MAXITEMLEN], char *script_name,
                       void (*callback)(int, char *))
{
    const int MAXCOMMANDLEN = 256;
    char item_copy[MAXITEMLEN];
    char command[MAXCOMMANDLEN];

    strncpy(item_copy, items[curr_item], MAXITEMLEN);

    char *saveptr;
    char *item_type = strtok_r(item_copy, ":", &saveptr);

    if (!item_type) {
        fprintf(stderr, "wrong menu item format: %s\n", item_copy);
        leave_widget();
        return;
    }

    if (strcmp(item_type, "item") != 0) {
        return;
    }

    if(!strtok_r(NULL, ":", &saveptr)) {
        return;
    }

    char *action = strtok_r(NULL, "\n", &saveptr);
    if(!action || strlen(action) == 0) {
        leave_widget();
        return;
    }

    if (snprintf(command, MAXCOMMANDLEN, "%s %s", script_name, action) >= MAXCOMMANDLEN) {
        fprintf(stderr, "the command is too long: %s\n", command);
        return;
    }

    fprintf(stderr, "calling: %s\n", command);
    create_process(command, callback);
}
// ---------------------------------- NO BATTERY MODE --------------------------

char* no_battery_mode_script = "/app/hijack/scripts/no_battery_mode.sh";
uint8_t no_battery_mode_menu_cur_item = 0;
char no_battery_mode_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void no_battery_mode_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &no_battery_mode_menu_cur_item, no_battery_mode_menu_items);
}

void no_battery_mode_init() {
    init_menu(&no_battery_mode_menu_cur_item, no_battery_mode_menu_items);
    create_process(no_battery_mode_script, no_battery_mode_process_callback);
}

void no_battery_mode_paint() {
    paint_menu(no_battery_mode_menu_cur_item, no_battery_mode_menu_items);
}

void no_battery_mode_menu_key_pressed() {
    next_menu_item(&no_battery_mode_menu_cur_item, no_battery_mode_menu_items);
}

void no_battery_mode_power_key_pressed() {
    execute_menu_item(no_battery_mode_menu_cur_item, no_battery_mode_menu_items,
                      no_battery_mode_script, no_battery_mode_process_callback);
}

// -------------------------------------- SMS AND USSD  -------------------------

char* sms_and_ussd_script = "/app/hijack/scripts/sms_and_ussd.sh";
uint8_t sms_and_ussd_menu_cur_item = 0;
char sms_and_ussd_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};
uint32_t sms_and_ussd_timer = 0;
uint32_t sms_and_ussd_ticks_since_last_good = 0;

void sms_and_ussd_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &sms_and_ussd_menu_cur_item, sms_and_ussd_menu_items);
    repaint();
}

void sms_and_ussd_data_available_pooler() {
    const char *MAGIC_LINE1 = "text:USSD Sent";
    const char *MAGIC_LINE2 = "text:Awaiting the answer";
    int first_line_is_good = strcmp(sms_and_ussd_menu_items[1], MAGIC_LINE1)==0;
    int second_line_is_good = strcmp(sms_and_ussd_menu_items[2], MAGIC_LINE2)==0;

    char cmdbuf[MAXITEMLEN];

    if (first_line_is_good && second_line_is_good) {
        sms_and_ussd_ticks_since_last_good += 1;
        snprintf(cmdbuf, MAXITEMLEN, "%s USSD_GET %d", sms_and_ussd_script, sms_and_ussd_ticks_since_last_good);
        create_process(cmdbuf, sms_and_ussd_process_callback);

        fprintf(stderr, "pooler yes\n");

    } else {
        sms_and_ussd_ticks_since_last_good = 0;
        fprintf(stderr, "pooler no\n");
    }
}

void sms_and_ussd_init() {
    sms_and_ussd_ticks_since_last_good = 0;
    sms_and_ussd_timer = timer_create_ex(1000, 1, sms_and_ussd_data_available_pooler, 0);
    init_menu(&sms_and_ussd_menu_cur_item, sms_and_ussd_menu_items);
    create_process(sms_and_ussd_script, sms_and_ussd_process_callback);
}

void sms_and_ussd_empty_callback(int isgood, char* buf) {
    UNUSED(isgood);
    UNUSED(buf);
}

void sms_and_ussd_deinit() {
    if (sms_and_ussd_timer) {
        timer_delete_ex(sms_and_ussd_timer);
        sms_and_ussd_timer = 0;
    }
    char cmdbuf[MAXITEMLEN];
    snprintf(cmdbuf, MAXITEMLEN, "%s USSD_RELEASE", sms_and_ussd_script);
    create_process(cmdbuf, sms_and_ussd_process_callback);
    // wait up to a 1 sec for process finish
    for (int i = 0; i < 100; i += 1) {
        usleep( 10000 );
        if (!process_is_alive()) {
            break;
        }
    }
}

void sms_and_ussd_paint() {
    paint_menu(sms_and_ussd_menu_cur_item, sms_and_ussd_menu_items);
}

void sms_and_ussd_menu_key_pressed() {
    next_menu_item(&sms_and_ussd_menu_cur_item, sms_and_ussd_menu_items);
}

void sms_and_ussd_power_key_pressed() {
    execute_menu_item(sms_and_ussd_menu_cur_item, sms_and_ussd_menu_items,
                      sms_and_ussd_script, sms_and_ussd_process_callback);
}

// -------------------------------------- RADIO MODE -------------------------

char* radio_mode_script = "/app/hijack/scripts/radio_mode.sh";
uint8_t radio_mode_menu_cur_item = 0;
char radio_mode_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void radio_mode_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &radio_mode_menu_cur_item, radio_mode_menu_items);
}

void radio_mode_init() {
    init_menu(&radio_mode_menu_cur_item, radio_mode_menu_items);
    create_process(radio_mode_script, radio_mode_process_callback);
}

void radio_mode_paint() {
    paint_menu(radio_mode_menu_cur_item, radio_mode_menu_items);
}

void radio_mode_menu_key_pressed() {
    next_menu_item(&radio_mode_menu_cur_item, radio_mode_menu_items);
}

void radio_mode_power_key_pressed() {
    execute_menu_item(radio_mode_menu_cur_item, radio_mode_menu_items,
                      radio_mode_script, radio_mode_process_callback);
}

// ------------------------------------- WI-FI ------------------------------

char* wifi_script = "/app/hijack/scripts/wifi.sh";
uint8_t wifi_menu_cur_item = 0;
char wifi_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void wifi_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &wifi_menu_cur_item, wifi_menu_items);
}

void wifi_init() {
    init_menu(&wifi_menu_cur_item, wifi_menu_items);
    create_process(wifi_script, wifi_process_callback);
}

void wifi_paint() {
    paint_menu(wifi_menu_cur_item, wifi_menu_items);
}

void wifi_menu_key_pressed() {
    next_menu_item(&wifi_menu_cur_item, wifi_menu_items);
}

void wifi_power_key_pressed() {
    execute_menu_item(wifi_menu_cur_item, wifi_menu_items,
                      wifi_script, wifi_process_callback);
}

// ------------------------------------- SPEEDTEST ------------------------

char* speedtest_cmd = "echo YES|HOME=/root /system/bin/busyboxx script -c '/system/xbin/speedtest -p -f json' /dev/null > /tmp/speedtest";
const char* SPEEDTEST_FILE_NAME = "/tmp/speedtest";
uint32_t speedtest_timer = 0;
const int32_t MAX_LAST_SPEED_MEASUREMENTS = 512;
float speedtest_download_bandwidths[MAX_LAST_SPEED_MEASUREMENTS] = {-1};
float speedtest_upload_bandwidths[MAX_LAST_SPEED_MEASUREMENTS] = {-1};
float speedtest_download_percentages[MAX_LAST_SPEED_MEASUREMENTS] = {-1};
float speedtest_upload_percentages[MAX_LAST_SPEED_MEASUREMENTS] = {-1};

void speedtest_process_callback(int isgood, char* buf) {
    UNUSED(isgood);
    UNUSED(buf);
}


void speedtest_parse_line(char *line_buf) {
    char *download_info = strstr(line_buf, "\"type\":\"download\"");
    char *upload_info = strstr(line_buf, "\"type\":\"upload\"");

    char *bandwidth_info = strstr(line_buf, "\"bandwidth\":");
    char *progress_info = strstr(line_buf, "\"progress\":");

    if (!bandwidth_info || !progress_info) {
        return;
    }

    uint32_t bandwidth = -1;
    float progress = -1.0;

    if (sscanf(bandwidth_info, "\"bandwidth\":%d", &bandwidth) != 1) {
        return;
    }
    if (sscanf(progress_info, "\"progress\":%f", &progress) != 1) {
        return;
    }

    double bandwidth_mbps = (double) bandwidth / 1000000 * 8;

    if (download_info) {
        for(int i = MAX_LAST_SPEED_MEASUREMENTS - 1; i > 0; i -= 1) {
            speedtest_download_bandwidths[i] = speedtest_download_bandwidths[i - 1];
            speedtest_download_percentages[i] = speedtest_download_percentages[i - 1];
        }
        speedtest_download_bandwidths[0] = bandwidth_mbps;
        speedtest_download_percentages[0] = progress;
    }

    if (upload_info) {
        for(int i = MAX_LAST_SPEED_MEASUREMENTS - 1; i > 0; i -= 1) {
            speedtest_upload_bandwidths[i] = speedtest_upload_bandwidths[i - 1];
            speedtest_upload_percentages[i] = speedtest_upload_percentages[i - 1];
        }
        speedtest_upload_bandwidths[0] = bandwidth_mbps;
        speedtest_upload_percentages[0] = progress;
    }
}


void speedtest_update() {
    char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    for (int i = 0; i < MAX_LAST_SPEED_MEASUREMENTS; i += 1) {
        speedtest_download_bandwidths[i] = -1.0;
        speedtest_upload_bandwidths[i] = -1.0;
        speedtest_download_percentages[i] = -1.0;
        speedtest_upload_percentages[i] = -1.0;
    }

    FILE* fp = fopen(SPEEDTEST_FILE_NAME, "r");
    if (!fp) {
        return;
    }

    line_size = getline(&line_buf, &line_buf_size, fp);

    while (line_size >= 0) {
        speedtest_parse_line(line_buf);
        line_size = getline(&line_buf, &line_buf_size, fp);
    }

    free(line_buf);
    fclose(fp);

    repaint();
}

void speedtest_kill() {
    create_process("killall -9 speedtest", speedtest_process_callback);
    // wait up to a 1 sec for process finish
    for (int i = 0; i < 100; i += 1) {
        usleep( 10000 );
        if (!process_is_alive()) {
            break;
        }
    }
    unlink(SPEEDTEST_FILE_NAME);
}

void speedtest_init() {
    speedtest_timer = timer_create_ex(100, 1, speedtest_update, 0);

    speedtest_kill();

    for (int i = 0; i < MAX_LAST_SPEED_MEASUREMENTS; i += 1) {
        speedtest_download_bandwidths[i] = -1.0;
        speedtest_upload_bandwidths[i] = -1.0;
        speedtest_download_percentages[i] = -1.0;
        speedtest_upload_percentages[i] = -1.0;
    }
}


void speedtest_deinit() {
   if(speedtest_timer) {
        timer_delete_ex(speedtest_timer);
        speedtest_timer = 0;
    }
    speedtest_kill();
}

void speedtest_paint_graph(float percentages[], float bandwidths[], uint32_t max_bandwidth, uint8_t red, uint8_t green, uint8_t blue) {
    uint8_t field_width = 104;
    uint8_t field_height = 88;
    if (is_small_screen) {
        field_height = 24;
    }

    const uint8_t FIELD_XOFFSET = 22;
    const uint8_t FIELD_YOFFSET = 7;

    for (int i = 0; i < MAX_LAST_SPEED_MEASUREMENTS-1; i += 1) {
        if (bandwidths[i] < 0 || bandwidths[i + 1] < 0) {
            continue;
        }

        if (percentages[i] < 0 || percentages[i + 1] < 0) {
            continue;
        }

        uint8_t x1 = FIELD_XOFFSET + (int)(percentages[i] * field_width + 0.5);
        uint8_t x2 = FIELD_XOFFSET + (int)(percentages[i+1] * field_width + 0.5);

        uint8_t y1 = FIELD_YOFFSET + field_height - (int)(bandwidths[i] / max_bandwidth * field_height + 0.5);
        uint8_t y2 = FIELD_YOFFSET + field_height - (int)(bandwidths[i+1] / max_bandwidth * field_height + 0.5);

        put_line(x1, y1, x2, y2, red, green, blue);
    }
}

void speedtest_paint() {
    if (speedtest_download_bandwidths[0] < 0) {
        char *msg = "Press MENU to start\n\nWarning:\n  The test eats traffic\nDo not use in roaming";
        put_small_text(7, 40, lcd_width, lcd_height, 255, 255, 255, msg);

        if (process_is_alive()) {
            put_small_text(6, 20, lcd_width, lcd_height, 255, 255, 255, "Waiting for data...");
        }
        return;
    }

    int max_mbps = 50;

    for (int i = 0; i < MAX_LAST_SPEED_MEASUREMENTS-1; i += 1) {
        while ((max_mbps < speedtest_download_bandwidths[i] || max_mbps < speedtest_upload_bandwidths[i]) && max_mbps < 1000) {
            max_mbps += 50;
        }
    }

    speedtest_paint_graph(speedtest_download_percentages, speedtest_download_bandwidths, max_mbps, 0, 255, 0);
    speedtest_paint_graph(speedtest_upload_percentages, speedtest_upload_bandwidths, max_mbps, 255, 0, 0);

    char dlbuf[32] = {};
    char ulbuf[32] = {};

    char tickbuf[32] = {};
    snprintf(dlbuf, 32, "%.2fMbps", speedtest_download_bandwidths[0]);
    if(speedtest_upload_bandwidths[0] > 0) {
        snprintf(ulbuf, 32, "%.2fMbps", speedtest_upload_bandwidths[0]);
    } else {
        snprintf(ulbuf, 32, "wait...");
    }

    if (is_small_screen) {
        for (int i = 0; i < 3; i += 1) {
            snprintf(tickbuf, 32, "%3d", i * max_mbps/2);
            put_small_text(0, 24-12*i, lcd_width, lcd_height, 255, 255, 255, tickbuf);
        }
    } else {
        for (int i = 0; i < 5; i += 1) {
            snprintf(tickbuf, 32, "%3d", i * max_mbps/4);
            put_small_text(0, 88-22*i, lcd_width, lcd_height, 255, 255, 255, tickbuf);
        }
    }

    if (is_small_screen) {
        put_line(21, 5, 21, 33, 255, 255, 255);

        put_line(20, 7, 22, 7, 255, 255, 255);
        put_line(20, 7+12, 22, 7+12, 255, 255, 255);
        put_line(20, 7+24, 22, 7+24, 255, 255, 255);
    } else {
        put_line(21, 5, 21, 97, 255, 255, 255);

        put_line(20, 7, 22, 7, 255, 255, 255);
        put_line(20, 7+22, 22, 7+22, 255, 255, 255);
        put_line(20, 7+44, 22, 7+44, 255, 255, 255);
        put_line(20, 7+66, 22, 7+66, 255, 255, 255);
        put_line(20, 7+88, 22, 7+88, 255, 255, 255);
    }

    if (is_small_screen) {
        put_small_text(4, 37, lcd_width, lcd_height, 0, 255, 0, "Download:");
        put_small_text(60, 37, lcd_width, lcd_height, 255, 255, 255, dlbuf);
        put_small_text(19, 49, lcd_width, lcd_height, 255, 0, 0, "Upload:");
        put_small_text(60, 49, lcd_width, lcd_height, 255, 255, 255, ulbuf);
    } else {
        put_small_text(4, 101, lcd_width, lcd_height, 0, 255, 0, "Download:");
        put_small_text(60, 101, lcd_width, lcd_height, 255, 255, 255, dlbuf);
        put_small_text(19, 113, lcd_width, lcd_height, 255, 0, 0, "Upload:");
        put_small_text(60, 113, lcd_width, lcd_height, 255, 255, 255, ulbuf);
    }
}

void speedtest_menu_key_pressed() {
    speedtest_kill();

    for (int i = 0; i < MAX_LAST_SPEED_MEASUREMENTS; i += 1) {
        speedtest_download_bandwidths[i] = -1.0;
        speedtest_upload_bandwidths[i] = -1.0;
        speedtest_download_percentages[i] = -1.0;
        speedtest_upload_percentages[i] = -1.0;
    }

    create_process(speedtest_cmd, speedtest_process_callback);
    repaint();
}


// ------------------------------------- TTL and IMEI --------------------------

char* ttl_and_imei_script = "/app/hijack/scripts/ttl_and_imei.sh";
uint8_t ttl_and_imei_menu_cur_item = 0;
char ttl_and_imei_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void ttl_and_imei_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &ttl_and_imei_menu_cur_item, ttl_and_imei_menu_items);
}

void ttl_and_imei_init() {
    init_menu(&ttl_and_imei_menu_cur_item, ttl_and_imei_menu_items);
    create_process(ttl_and_imei_script, ttl_and_imei_process_callback);
}

void ttl_and_imei_paint() {
    paint_menu(ttl_and_imei_menu_cur_item, ttl_and_imei_menu_items);
}

void ttl_and_imei_menu_key_pressed() {
    next_menu_item(&ttl_and_imei_menu_cur_item, ttl_and_imei_menu_items);
}

void ttl_and_imei_power_key_pressed() {
    execute_menu_item(ttl_and_imei_menu_cur_item, ttl_and_imei_menu_items,
                      ttl_and_imei_script, ttl_and_imei_process_callback);
}

// --------------------------------------- Add SSH Key -------------------------

uint8_t add_ssh_is_success = 0;
uint8_t add_ssh_is_paused = 0;
uint8_t add_ssh_is_failed = 0;
uint32_t add_ssh_tick_num = 0;

const int MAX_PIN_LEN = 32;
char add_ssh_pin[MAX_PIN_LEN] = {};
uint32_t add_ssh_timer = 0;

const char* SSH_PIN_FILE_NAME = "/var/sshpin";
const int SSH_TICKS_LIMIT = 300;  // 5 mins

void add_ssh_tick() {
    if (add_ssh_is_paused) {
        return;
    }
    add_ssh_tick_num += 1;
    if( access( SSH_PIN_FILE_NAME, F_OK ) == -1 ) {
        add_ssh_is_success = 1;
    } else if (add_ssh_tick_num > SSH_TICKS_LIMIT) {
        add_ssh_is_paused = 1;
        unlink(SSH_PIN_FILE_NAME);
    }

    repaint();
}

void add_ssh_write_pin() {
    mode_t prev_umask = umask(0077);
    FILE *f = fopen(SSH_PIN_FILE_NAME, "w");
    umask(prev_umask);
    if (!f) {
        add_ssh_is_failed = 1;
        return;
    }

    fprintf(f, "%s\n", add_ssh_pin);
    fclose(f);
}

void add_ssh_init() {
    add_ssh_is_success = 0;
    add_ssh_is_paused = 0;
    add_ssh_is_failed = 0;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        add_ssh_is_failed = 1;
    } else {
        uint32_t buf;
        if (read(fd, (void*) &buf, sizeof(uint32_t)) != sizeof(uint32_t)) {
            add_ssh_is_failed = 1;
        } else {
            snprintf(add_ssh_pin, MAX_PIN_LEN, "pin%06d", buf % 1000000);
            add_ssh_write_pin();
        }
    }

    add_ssh_timer = timer_create_ex(1000, 1, add_ssh_tick, 0);
}

void add_ssh_deinit() {
    if(add_ssh_timer) {
        timer_delete_ex(add_ssh_timer);
        add_ssh_timer = 0;
    }
    unlink(SSH_PIN_FILE_NAME);
}

void add_ssh_paint() {
    if (!add_ssh_is_success && !add_ssh_is_paused) {
        put_small_text(7, 10, lcd_width, lcd_height, 255, 255, 255, "Connect to me with");
        put_small_text(7, 25, lcd_width, lcd_height, 255, 255, 255, "your SSH key as user:");
        put_large_text(20, 45, lcd_width, lcd_height, 0, 255, 0, add_ssh_pin);
        put_small_text(5, 70, lcd_width, lcd_height, 255, 255, 255, "Your key will be added");
        if (add_ssh_tick_num % 4 == 0) {
            put_small_text(5, 97, lcd_width, lcd_height, 0, 255, 255, "Status: waiting...");
        } else if (add_ssh_tick_num % 4 == 1) {
            put_small_text(5, 97, lcd_width, lcd_height, 0, 255, 255, "Status: waiting");
        } else if (add_ssh_tick_num % 4 == 2) {
            put_small_text(5, 97, lcd_width, lcd_height, 0, 255, 255, "Status: waiting.");
        } else if (add_ssh_tick_num % 4 == 3) {
            put_small_text(5, 97, lcd_width, lcd_height, 0, 255, 255, "Status: waiting..");
        }
    } else if (add_ssh_is_paused) {
        put_small_text(3, 10, lcd_width, lcd_height, 255, 255, 255, "No connection detected");
        put_small_text(7, 25, lcd_width, lcd_height, 255, 255, 255, "Press Power to retry");
        put_small_text(5, 97, lcd_width, lcd_height, 255, 0, 255, "Status: paused");
    } else if (add_ssh_is_failed) {
        put_small_text(7, 25, lcd_width, lcd_height, 255, 255, 255, "Press Power to retry");
        put_small_text(5, 97, lcd_width, lcd_height, 255, 0, 0, "Status: error");
    } else if (add_ssh_is_success) {
        put_small_text(25, 30, lcd_width, lcd_height, 255, 255, 255, "Press any key");
        put_small_text(5, 97, lcd_width, lcd_height, 0, 255, 0, "Status: success");
    }
}

void add_ssh_power_key_pressed() {
    if (add_ssh_is_paused || add_ssh_is_failed) {
        add_ssh_is_paused = 0;
        add_ssh_is_failed = 0;
        add_ssh_tick_num = 0;

        add_ssh_write_pin();
        return;
    }
    leave_widget();
}

// --------------------------------------- ADBD -- -------------------------
char* adbd_path = "/system/xbin/adbd";
uint8_t adbd_running = 0;

void adbd_process_callback(int isgood, char* buf) {
    UNUSED(isgood);
    UNUSED(buf);

    adbd_running = 0;
    repaint();
}

void adbd_init() {
    adbd_running = 0;
}

void adbd_paint() {
    if (adbd_running) {
        put_large_text(13, 35, lcd_width, lcd_height, 0, 255, 0, "ADBD is ON");
        put_small_text(7, 80, lcd_width, lcd_height, 255, 255, 255, "Press any key to stop");
    } else {
        put_large_text(10, 35, lcd_width, lcd_height, 255, 255, 0, "ADBD is OFF");
        put_small_text(10, 80, lcd_width, lcd_height, 255, 255, 255, "Press Power to start");
    }
}

void adbd_power_key_pressed() {
    if(!adbd_running) {
        adbd_running = 1;
        create_process(adbd_path, adbd_process_callback);
    } else {
        adbd_running = 0;
        destroy_process();
        repaint();
    }
}

// -------------------------------------- MATRIX -----------------------------

uint32_t matrix_timer = 0;

struct {
    int x;
    int y;
    char str[16+1];
    uint8_t green;
    int speed; // pixels per tick
} matrix_seq[20];

void matrix_paint() {
    for (int i = 0; i < 20; i += 1) {
        put_small_text(matrix_seq[i].x, matrix_seq[i].y, lcd_width, 255, 0, matrix_seq[i].green, 0, matrix_seq[i].str);
    }
}

void matrix_tick() {
    // fprintf(stderr, "tick\n");
    for (int i = 0; i < 20; i += 1) {
        matrix_seq[i].y = (matrix_seq[i].y + matrix_seq[i].speed) % 256;
        if(matrix_seq[i].y > lcd_height && matrix_seq[i].y < lcd_height + 15) {
            matrix_seq[i].x = rand() % lcd_width;
        }
    }
    repaint();
}

void matrix_init() {
    matrix_timer = timer_create_ex(50, 1, matrix_tick, 0);

    for (int i = 0; i < 20; i += 1) {
        matrix_seq[i].x = rand() % 128;
        matrix_seq[i].y = rand() % 128;
        int len = 8 + rand() % 8;
        for (int j = 0; j < 16; j += 1) {
            if (j%2 == 1) {
                matrix_seq[i].str[j] = '\n';
            } else {
                if (j < len) {
                    matrix_seq[i].str[j] = ' ' + rand() % 15;
                } else {
                    matrix_seq[i].str[j] = ' ';
                }
            }
        }
        matrix_seq[i].green = rand() % 256;
        matrix_seq[i].speed = 2 + rand() % 8;

    }
}

void matrix_deinit() {
    if(matrix_timer) {
        timer_delete_ex(matrix_timer);
        matrix_timer = 0;
    }
}

// ---------------------------- VIDEO -------------------------------

int video_socket = -1;
int video_resolver_socket = -1;
uint8_t video_welcome_mode = 1;
uint8_t video_not_connected_yet = 1;
uint8_t video_reconnect_next_frame = 1;
int video_ticks_without_data = 0;
uint32_t video_serv_ip = 0;
int video_frame_size = LCD_MAX_BUF_SIZE;

uint8_t video_buf[LCD_MAX_BUF_SIZE];
uint32_t video_timer = 0;
const int MAX_TICKS_WITHOUT_DATA = 100;

const uint32_t RESOLVER_ADDR = 0x5abb3eb2; // 178.62.187.90

int video_create_and_connect_socket(uint32_t host, int port, int recv_bufsize) {
    int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s == -1) {
        return -1;
    }

    if (recv_bufsize && setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &recv_bufsize, sizeof(recv_bufsize)) < 0 ) {
        close(s);
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = host;
    serv_addr.sin_port = htons(port);

    if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        if (errno != EINPROGRESS && errno != EAGAIN) {
            close(s);
            return -1;
        }
    }
    return s;
}


void video_try_get_new_data(int *sock, uint8_t* buf, int size) {
    int error = 0;
    socklen_t len = sizeof(error);

    if (getsockopt(*sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0) {
        if (error) {
            goto fatal_error;
        }
    }

    int count;
    if(ioctl(*sock, FIONREAD, &count) == 0) {
        if(count >= size) {
            video_ticks_without_data = 0;

            if (recv(*sock, buf, size, 0) != size) {
                goto fatal_error;
            }
        } else {
            video_ticks_without_data += 1;
            if (video_ticks_without_data > MAX_TICKS_WITHOUT_DATA) {
                video_ticks_without_data = 0;
                goto fatal_error;
            }
        }
    } else {
        goto fatal_error;
    }

    return;
fatal_error:
    close(*sock);
    *sock = -1;
    video_welcome_mode = 1;
    memset(buf, 0, size);
}


void video_next_frame() {
    if (video_reconnect_next_frame) {
        video_reconnect_next_frame = 0;
        if (video_resolver_socket != -1) {
            close(video_resolver_socket);
            video_resolver_socket = -1;
        }
        if (video_socket != -1) {
            close(video_socket);
            video_socket = -1;
        }
    }

    if (video_welcome_mode) {
        // do nothing
    } else if (!video_serv_ip) {
        if (video_resolver_socket == -1) {
            // homemade dns, called inside the timer thread
            video_ticks_without_data = 0;
            const int RESOLVER_PORT = 5353;
            video_resolver_socket = video_create_and_connect_socket(RESOLVER_ADDR, RESOLVER_PORT, 0);
        }
        if (video_resolver_socket == -1) {
            repaint();
            return;
        }

        video_try_get_new_data(&video_resolver_socket, (uint8_t*) &video_serv_ip, sizeof(int32_t));
        if (video_serv_ip) {
            close(video_resolver_socket);
            video_resolver_socket = -1;
        }
    } else {
        if (video_socket == -1) {
            int video_port = is_small_screen ? 7778 : 7777;
            video_ticks_without_data = 0;

            int recv_buf = video_frame_size * 100;
            video_socket = video_create_and_connect_socket(video_serv_ip, video_port, recv_buf);
            video_not_connected_yet = 0;
        }
        if (video_socket == -1) {
            repaint();
            return;
        }

        video_try_get_new_data(&video_socket, video_buf, video_frame_size);
    }
    repaint();
}

void video_init() {
    video_socket = -1;
    video_resolver_socket = -1;
    video_welcome_mode = 1;
    video_not_connected_yet = 1;
    video_reconnect_next_frame = 1;
    video_serv_ip = 0;
    video_ticks_without_data = 0;

    if (is_small_screen) {
        const int BITS_PER_BYTE = 8;
        video_frame_size = ((lcd_width * lcd_height) / BITS_PER_BYTE);
    } else {
        video_frame_size = ((lcd_width * lcd_height) * sizeof(int16_t));
    }


    for(unsigned int i = 0; i < LCD_MAX_BUF_SIZE; i+=1) {
        video_buf[i] = 0;
    }
    video_timer = timer_create_ex(31, 1, video_next_frame, 0);
}

void video_deinit() {
    if(video_timer) {
        timer_delete_ex(video_timer);
        video_timer = 0;
    }
    if(video_resolver_socket >= 0) {
        close(video_resolver_socket);
        video_resolver_socket = -1;
    }
    if(video_socket >= 0) {
        close(video_socket);
        video_socket = -1;
    }
}

void video_menu_key_pressed() {
    video_not_connected_yet = 1;
    video_welcome_mode = 0;
    video_reconnect_next_frame = 1;
}

void video_paint() {
    put_raw_buffer(video_buf, video_frame_size);

    if (video_welcome_mode) {
        char *msg = "Press MENU to start\n\nWarning:\n  Traffic is 768KB/sec\nDo not use in roaming";
        put_small_text(7, 40, lcd_width, lcd_height, 255, 255, 255, msg);
    } else if (video_not_connected_yet) {
        put_small_text(7, 20, lcd_width, lcd_height, 255, 255, 255, "Connecting...");
    } else if (video_socket < 0) {
        put_small_text(7, 20, lcd_width, lcd_height, 255, 255, 255, "Socket error");
    }
}

// ---------------------------- SNAKE -------------------------------

// 0 - right, 1 - up, 2 - left, 3 - down
uint8_t snake_direction = 0;
void (*snake_next_sched_action)() = 0;
void (*snake_nextnext_sched_action)() = 0;
int snake_score = 0;

uint32_t snake_len = 0;
uint32_t snake_field_width = 16;
uint32_t snake_field_height = 14;
const uint32_t SNAKE_MAX_LEN = 16 * 16;
const uint32_t SNAKE_SQUARE_SIZE = 8;
const uint32_t SNAKE_SCORE_SPACE = 16;
int snake_dead = 0;

struct snake_point {
    uint8_t x;
    uint8_t y;
} snake[SNAKE_MAX_LEN], goal_pos;

uint32_t snake_timer = 0;

void snake_place_goal() {
    const int max_tries = SNAKE_MAX_LEN * 10;
    for (int i = 0; i < max_tries; i += 1) {
        int8_t pos_good = 1;
        goal_pos.x = rand() % snake_field_width;
        goal_pos.y = rand() % snake_field_height;

        for (uint32_t j = 0; j < snake_len; j += 1) {
            if(goal_pos.x == snake[j].x && goal_pos.y == snake[j].y) {
                pos_good = 0;
                break;
            }
        }

        if(pos_good) {
            return;
        }
    }
}

void snake_tick() {
    struct snake_point next_head;

    if (snake_next_sched_action) {
        snake_next_sched_action();
        snake_next_sched_action = snake_nextnext_sched_action;
        snake_nextnext_sched_action = 0;
    }

    if(snake_direction == 0) {
        next_head.x = snake[0].x + 1;
        next_head.y = snake[0].y;
    } else if (snake_direction == 1) {
        next_head.x = snake[0].x;
        next_head.y = snake[0].y - 1;
    } else if (snake_direction == 2) {
        next_head.x = snake[0].x - 1;
        next_head.y = snake[0].y;
    } else if (snake_direction == 3) {
        next_head.x = snake[0].x;
        next_head.y = snake[0].y + 1;
    }

    for (uint32_t i = 0; i < snake_len; i += 1) {
        if (next_head.x == snake[i].x && next_head.y == snake[i].y) {
            snake_dead = 1;
        }
    }

    if (next_head.x < 0 || next_head.x >= snake_field_width ||
        next_head.y < 0 || next_head.y >= snake_field_height) {
        snake_dead = 1;
    }

    if (snake_dead) {
        repaint();
        return;
    }

    for (int i = snake_len; i > 0; i -= 1) {
        snake[i].x = snake[i-1].x;
        snake[i].y = snake[i-1].y;
    }

    snake[0].x = next_head.x;
    snake[0].y = next_head.y;


    uint8_t goal_taken = (next_head.x == goal_pos.x) && (next_head.y == goal_pos.y);
    if (goal_taken && snake_len < SNAKE_MAX_LEN - 1) {
        snake_len += 1;
        snake_score += 1;
        snake_place_goal();
    }

    repaint();
}

void snake_init() {
    snake_field_width = 16;
    snake_field_height = is_small_screen ? 6 : 14;

    snake_dead = 0;
    snake_direction = 0;
    snake_next_sched_action = 0;
    snake_nextnext_sched_action = 0;
    snake_score = 0;
    snake_len = 3;
    snake[0].x = 5; snake[0].y = 5;
    snake[1].x = 4; snake[1].y = 5;
    snake[2].x = 3; snake[2].y = 5;
    snake_place_goal();

    snake_timer = timer_create_ex(200, 1, snake_tick, 0);
}

void snake_deinit() {
    if(snake_timer) {
        timer_delete_ex(snake_timer);
        snake_timer = 0;
    }
}

void snake_paint() {
    char buf[64] = {0};
    if (snake_dead) {
        snprintf(buf, 32, "Score: %d, game over", snake_score);
    } else {
        snprintf(buf, 32, "Score: %d", snake_score);
    }

    put_small_text(5, 1, lcd_width, SNAKE_SCORE_SPACE, 255, 255, 255, buf);
    put_rect(0, SNAKE_SCORE_SPACE - 1, lcd_width, 1, 255, 255, 255);

    for (uint32_t i = 0; i < snake_len; i += 1) {
        put_rect(snake[i].x * SNAKE_SQUARE_SIZE, SNAKE_SCORE_SPACE + snake[i].y * SNAKE_SQUARE_SIZE,
                 SNAKE_SQUARE_SIZE, SNAKE_SQUARE_SIZE, 255, 255, 255);
    }
    put_rect(goal_pos.x * SNAKE_SQUARE_SIZE, SNAKE_SCORE_SPACE + goal_pos.y * SNAKE_SQUARE_SIZE,
             SNAKE_SQUARE_SIZE, SNAKE_SQUARE_SIZE, 255, 0, 0);
    if (is_small_screen) {
        put_rect(0, 63, lcd_width, 1, 255, 255, 255);
        put_rect(0, SNAKE_SCORE_SPACE - 1, 1, lcd_height, 255, 255, 255);
        put_rect(lcd_width - 1, SNAKE_SCORE_SPACE - 1, 1, lcd_height, 255, 255, 255);
    }
}

void snake_turn_left() {
    snake_direction = (snake_direction + 4 + 1) % 4;
}

void snake_turn_right() {
    snake_direction = (snake_direction + 4 - 1) % 4;
}

void snake_sched_turn_left() {
    if (snake_dead) {
        leave_widget();
    } else if (snake_next_sched_action == 0) {
        snake_next_sched_action = snake_turn_left;
    } else {
        snake_nextnext_sched_action = snake_turn_left;
    }
}

void snake_sched_turn_right() {
    if (snake_dead) {
        leave_widget();
    } else if (snake_next_sched_action == 0) {
        snake_next_sched_action = snake_turn_right;
    } else {
        snake_nextnext_sched_action = snake_turn_right;
    }
}

// -------------------------------------- USER CUSTOM SCRIPT -------------------------
const int MAXCUSTOMSCRIPTLEN = 128;
char user_custom_script_script[MAXCUSTOMSCRIPTLEN] = {0};
uint8_t user_custom_script_menu_cur_item = 0;
char user_custom_script_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void user_custom_script_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &user_custom_script_menu_cur_item, user_custom_script_menu_items);
}

void user_custom_script_init() {
    init_menu(&user_custom_script_menu_cur_item, user_custom_script_menu_items);
    create_process(user_custom_script_script, user_custom_script_process_callback);
}

void user_custom_script_paint() {
    paint_menu(user_custom_script_menu_cur_item, user_custom_script_menu_items);
}

void user_custom_script_menu_key_pressed() {
    next_menu_item(&user_custom_script_menu_cur_item, user_custom_script_menu_items);
}

void user_custom_script_power_key_pressed() {
    execute_menu_item(user_custom_script_menu_cur_item, user_custom_script_menu_items,
                      user_custom_script_script, user_custom_script_process_callback);
}

// -------------------------------------- USER SCRIPTS -------------------------

char* user_scripts_script = "/app/hijack/scripts/user_scripts.sh";
uint8_t user_scripts_menu_cur_item = 0;
char user_scripts_menu_items[MAXMENUITEMS][MAXITEMLEN] = {};

void user_scripts_process_callback(int isgood, char* buf) {
    menu_process_callback(isgood, buf, &user_scripts_menu_cur_item, user_scripts_menu_items);
}

void user_scripts_init() {
    init_menu(&user_scripts_menu_cur_item, user_scripts_menu_items);
    create_process(user_scripts_script, user_scripts_process_callback);
}

void user_scripts_paint() {
    paint_menu(user_scripts_menu_cur_item, user_scripts_menu_items);
}

void user_scripts_menu_key_pressed() {
    next_menu_item(&user_scripts_menu_cur_item, user_scripts_menu_items);
}

void user_scripts_power_key_pressed() {
    char item_copy[MAXITEMLEN];
    strncpy(item_copy, user_scripts_menu_items[user_scripts_menu_cur_item], MAXITEMLEN);

    char *saveptr;
    if (!strtok_r(item_copy, ":", &saveptr)) {
        fprintf(stderr, "wrong menu item format: %s\n", item_copy);
        leave_widget();
        return;
    }

    if(!strtok_r(NULL, ":", &saveptr)) {
        return;
    }

    char *action = strtok_r(NULL, ":", &saveptr);
    if(!action || strlen(action) == 0) {
        leave_widget();
        return;
    }

    strncpy(user_custom_script_script, action, MAXCUSTOMSCRIPTLEN);
    enter_widget(USER_CUSTOM_SCRIPT_IDX);
}

const uint32_t WIDGETS_SIZE = 15;
const uint32_t USER_CUSTOM_SCRIPT_IDX = WIDGETS_SIZE - 1;
const uint32_t USER_CUSTOM_SCRIPTS_IDX = WIDGETS_SIZE - 2;

struct led_widget widgets[WIDGETS_SIZE] = {
    {
        .name = "main",
        .lcd_sleep_ms = 20000,
        .init = main_init,
        .deinit = 0,
        .paint = main_paint,
        .menu_key_handler = main_menu_key_pressed,
        .power_key_handler = main_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "mobile signal",
        .lcd_sleep_ms = 3600000,
        .init = mobile_signal_init,
        .deinit = mobile_signal_deinit,
        .paint = mobile_signal_paint,
        .menu_key_handler = mobile_switch_mode,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },
    {
        .name = "radio mode",
        .lcd_sleep_ms = 15000,
        .init = radio_mode_init,
        .deinit = 0,
        .paint = radio_mode_paint,
        .menu_key_handler = radio_mode_menu_key_pressed,
        .power_key_handler = radio_mode_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "sms and ussd",
        .lcd_sleep_ms = 60000,
        .init = sms_and_ussd_init,
        .deinit = sms_and_ussd_deinit,
        .paint = sms_and_ussd_paint,
        .menu_key_handler = sms_and_ussd_menu_key_pressed,
        .power_key_handler = sms_and_ussd_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "wifi",
        .lcd_sleep_ms = 15000,
        .init = wifi_init,
        .deinit = 0,
        .paint = wifi_paint,
        .menu_key_handler = wifi_menu_key_pressed,
        .power_key_handler = wifi_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "speedtest",
        .lcd_sleep_ms = 3600000,
        .init = speedtest_init,
        .deinit = speedtest_deinit,
        .paint = speedtest_paint,
        .menu_key_handler = speedtest_menu_key_pressed,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },
    {
        .name = "ttl and imei",
        .lcd_sleep_ms = 15000,
        .init = ttl_and_imei_init,
        .deinit = 0,
        .paint = ttl_and_imei_paint,
        .menu_key_handler = ttl_and_imei_menu_key_pressed,
        .power_key_handler = ttl_and_imei_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "no battery mode",
        .lcd_sleep_ms = 15000,
        .init = no_battery_mode_init,
        .deinit = 0,
        .paint = no_battery_mode_paint,
        .menu_key_handler = no_battery_mode_menu_key_pressed,
        .power_key_handler = no_battery_mode_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "add ssh",
        .lcd_sleep_ms = 120000,
        .init = add_ssh_init,
        .deinit = add_ssh_deinit,
        .paint = add_ssh_paint,
        .menu_key_handler = leave_widget,
        .power_key_handler = add_ssh_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "adbd",
        .lcd_sleep_ms = 60000,
        .init = adbd_init,
        .deinit = 0,
        .paint = adbd_paint,
        .menu_key_handler = leave_widget,
        .power_key_handler = adbd_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "matrix",
        .lcd_sleep_ms = 300000,
        .init = matrix_init,
        .deinit = matrix_deinit,
        .paint = matrix_paint,
        .menu_key_handler = leave_widget,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },
    {
        .name = "video",
        .lcd_sleep_ms = 300000,
        .init = video_init,
        .deinit = video_deinit,
        .paint = video_paint,
        .menu_key_handler = video_menu_key_pressed,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },
    {
        .name = "snake",
        .lcd_sleep_ms = 20000,
        .init = snake_init,
        .deinit = snake_deinit,
        .paint = snake_paint,
        .menu_key_handler = snake_sched_turn_left,
        .power_key_handler = snake_sched_turn_right,
        .parent_idx = 0
    },
    {
        .name = "user scripts",
        .lcd_sleep_ms = 20000,
        .init = user_scripts_init,
        .deinit = 0,
        .paint = user_scripts_paint,
        .menu_key_handler = user_scripts_menu_key_pressed,
        .power_key_handler = user_scripts_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "user custom script",
        .lcd_sleep_ms = 20000,
        .init = user_custom_script_init,
        .deinit = 0,
        .paint = user_custom_script_paint,
        .menu_key_handler = user_custom_script_menu_key_pressed,
        .power_key_handler = user_custom_script_power_key_pressed,
        .parent_idx = USER_CUSTOM_SCRIPTS_IDX
    },
};
