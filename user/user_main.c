#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
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
user_tcp_sent_cb(void *arg)
{
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
user_tcp_discon_cb(void *arg)
{
   //tcp disconnect successfully
    
    os_printf("tcp disconnect succeed !!! \r\n");
}

const char* REQUEST_HEADER = "GET /distance.php?mac=%s&cpuid=%s&distance=%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n";
LOCAL void ICACHE_FLASH_ATTR

user_sent_data(struct espconn *pespconn)

{

    char *pbuf = (char *)os_zalloc(1024);

    sprintf(pbuf, REQUEST_HEADER, "intive.com");

    espconn_sent(pespconn, pbuf, strlen(pbuf));

    os_free(pbuf);

}


LOCAL void ICACHE_FLASH_ATTR

user_tcp_connect_cb(void *arg)

{

    struct espconn *pespconn = arg;

    printf("connect succeed !!! \r\n");

    espconn_regist_recvcb(pespconn, user_tcp_recv_cb);

    espconn_regist_sentcb(pespconn, user_tcp_sent_cb);

    espconn_regist_disconcb(pespconn, user_tcp_discon_cb);

    user_sent_data(pespconn);

}


uint32 user_rf_cal_sector_set(void)
{
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


static void request_scan(void);
static void scan_done_cb(void* arg, STATUS status);;

static void scan_done_cb (void* arg, STATUS status)
{
    /*struct bss_info *bssInfo = (struct bss_info*)arg;
    bssInfo = STAILQ_NEXT(bssInfo, next);

    while (bssInfo != NULL) {
        printf("%-32s %02X:%02X:%02X:%02X:%02X:%02X, ch %2d, auth %d, hid %d, rssi %d\n\r", 
                   bssInfo->ssid, 
                   bssInfo->bssid[0], bssInfo->bssid[1], bssInfo->bssid[2],
                   bssInfo->bssid[3], bssInfo->bssid[4], bssInfo->bssid[5],
                   bssInfo->channel,
                   bssInfo->authmode, bssInfo->is_hidden,
                   bssInfo->rssi);
        bssInfo = STAILQ_NEXT(bssInfo, next);
    }*/


    os_delay_us(10000);

    request_scan();

	int sta_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == sta_socket) 
	{
		close(sta_socket);
		vTaskDelay(1000 / portTICK_RATE_MS);
		printf("ESP8266 TCP client task > socket fail!\n");
		//continue;
	}
	else
	{
		printf("ESP8266 TCP client task > socket ok!\n");
	}
}

static void request_scan()
{
    struct scan_config sc;
    sc.ssid = NULL;
    sc.bssid = NULL;
    sc.channel = 0;
    sc.show_hidden = 1;

    wifi_station_scan(&sc, scan_done_cb);
}

void scan_task(void *pvParameters)
{
    request_scan();
    
    vTaskDelete(NULL);
}

void connect_task(void *pvParameters)
{

 printf("connect task start\n");

    for(;;) {

 printf("connect task\n"); 
 
 int inited = 0; 
 
 if (inited == 0 && wifi_station_get_connect_status() == STATION_GOT_IP) {

  printf("GOT IP!\n");



  user_tcp_conn.proto.tcp = &user_tcp;

  user_tcp_conn.type = ESPCONN_TCP;

  user_tcp_conn.state = ESPCONN_NONE;



  const char esp_tcp_server_ip[4] = {91,229,213,15};



  memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_tcp_server_ip, 4);

  user_tcp_conn.proto.tcp->remote_port = 80;

  user_tcp_conn.proto.tcp->local_port = espconn_port();

  

  //pespconn.proto.tcp->remote_port = 80;

  //uint32_t addr = ipaddr_addr("91.229.213.15");

  //memcpy(pespconn.proto.tcp->remote_ip, &addr, 4);

  

  printf("connect cb!\n");

  espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb);

  printf("connect!\n");

  espconn_connect(&user_tcp_conn);

  inited = 1;

  vTaskDelete(NULL);

 }

 //int i;

 //for(i=0;i<10;i++) {

  vTaskDelay(500);

 //}

    }

    vTaskDelete(NULL);

}




void user_init(void)
{
  // init gpio sussytem
  gpio_init();

  // configure UART TXD to be GPIO1, set as output
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO2); 
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
  //gpio_output_set(0, 0, (1 << 12), (1 << 13));

  gpio_output_set(0, 0, GPIO_ID_PIN(12), GPIO_ID_PIN(13));

    printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_opmode(STATION_MODE);

	struct station_config apconfig;

	memset(&apconfig, 0, sizeof(station_config));
	memcpy(&apconfig.ssid, ssid, strlen(ssid));
	memcpy(&apconfig.password, ssid_password, strlen(ssid_password));
	
	wifi_station_set_config(&apconfig);
    
	wifi_station_set_auto_connect(TRUE);
    	xTaskCreate(connect_task, "connect_task", 4096, NULL, 2, NULL);
  	
}


