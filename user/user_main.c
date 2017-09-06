#include "esp_common.h"
#include "espconn.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "espressif/espconn.h"
#include "espressif/airkiss.h"

#include "distance.h"
#include "wifi_config.h"

LOCAL os_timer_t test_timer;
LOCAL struct espconn user_tcp_conn;
LOCAL struct _esp_tcp user_tcp;
ip_addr_t tcp_server_ip;

int connection_active = 0;

#define NET_DOMAIN "krzycho.wapp.pl"

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length) {
    //received some data from tcp connection

    os_printf("tcp recv !!! %s \r\n", pusrdata);

}

/******************************************************************************
 * FunctionName : user_tcp_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_sent_cb(void *arg) {
    //data sent successfully

    os_printf("tcp sent succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : user_tcp_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
 *******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_discon_cb(void *arg) {
    //tcp disconnect successfully

    os_printf("tcp disconnect succeed !!! \r\n");
    connection_active = 0;
}

const char* REQUEST_HEADER = "GET /distance.php?mac=%x%x%x%x%x%x&cpuid=%x&distance=%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";

LOCAL void ICACHE_FLASH_ATTR

user_sent_data(struct espconn *pespconn) {

    char *pbuf = (char *) os_zalloc(1024);
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);

    uint32_t chip_id = system_get_chip_id();

    int distance = get_distance();

    sprintf(pbuf, REQUEST_HEADER, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], chip_id, distance, NET_DOMAIN);

    espconn_sent(pespconn, pbuf, strlen(pbuf));

    os_free(pbuf);

}

LOCAL void ICACHE_FLASH_ATTR

user_tcp_connect_cb(void *arg) {

    struct espconn *pespconn = arg;

    printf("connect succeed !!! \r\n");

    espconn_regist_recvcb(pespconn, user_tcp_recv_cb);

    espconn_regist_sentcb(pespconn, user_tcp_sent_cb);

    espconn_regist_disconcb(pespconn, user_tcp_discon_cb);
    
    user_sent_data(pespconn);

}

uint32 user_rf_cal_sector_set(void) {
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

LOCAL void ICACHE_FLASH_ATTR
start_connection(ip_addr_t *ipaddr, struct espconn *pespconn) {
    tcp_server_ip.addr = ipaddr->addr;
    memcpy(user_tcp_conn.proto.tcp->remote_ip, &ipaddr->addr, 4); // remote ip of tcp server which get by dns
        
    user_tcp_conn.proto.tcp->remote_port = 80;
    user_tcp_conn.proto.tcp->local_port = espconn_port();

    printf("connect cb!\n");
    espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb);

    printf("connect!\n");
    espconn_connect(&user_tcp_conn);
}


LOCAL void ICACHE_FLASH_ATTR
user_dns_found(const char *name, ip_addr_t *ipaddr, void *arg) {
    struct espconn *pespconn = (struct espconn *) arg;

    if (ipaddr == NULL) {
        os_printf("user_dns_found NULL \r\n");
        return;
    }

    //dns got ip
    os_printf("user_dns_found %d.%d.%d.%d \r\n",
            *((uint8 *) & ipaddr->addr), *((uint8 *) & ipaddr->addr + 1),
            *((uint8 *) & ipaddr->addr + 2), *((uint8 *) & ipaddr->addr + 3));

    if (tcp_server_ip.addr == 0 && ipaddr->addr != 0) {
        // dns succeed, create tcp connection
        start_connection(ipaddr, pespconn);
    }
}

void connect_task(void *pvParameters) {
    printf("connect task start\n");

    for (;;) {
        printf("connect task\n");
        if (connection_active == 0 && wifi_station_get_connect_status() == STATION_GOT_IP) {
            printf("GOT IP!\n");

            user_tcp_conn.proto.tcp = &user_tcp;
            user_tcp_conn.type = ESPCONN_TCP;
            user_tcp_conn.state = ESPCONN_NONE;

            int res = espconn_gethostbyname(&user_tcp_conn, NET_DOMAIN, &tcp_server_ip, user_dns_found);
            
            if(res == ESPCONN_OK) {
                start_connection(&tcp_server_ip, &user_tcp_conn);
            } else if(res == ESPCONN_INPROGRESS) {
                // wait for callback user_dns_found
            }
            connection_active = 1;

            //vTaskDelete(NULL);
        }
        vTaskDelay(500);
    }

    vTaskDelete(NULL);
}

void user_init(void) {
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO2);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

    gpio_output_set(0, 0, GPIO_ID_PIN(12), GPIO_ID_PIN(13));

    printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_opmode(STATION_MODE);

    struct station_config apconfig;

    memset(&apconfig, 0, sizeof (struct station_config));
    memcpy(&apconfig.ssid, ssid, strlen(ssid));
    memcpy(&apconfig.password, ssid_password, strlen(ssid_password));

    espconn_init();
    
    wifi_station_set_config(&apconfig);

    wifi_station_set_auto_connect(TRUE);
    xTaskCreate(connect_task, "connect_task", 4096, NULL, 2, NULL);

}


