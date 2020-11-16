#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_event.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_ESP_CONSOLE_UART

static const char *TAG = "scan";
////////////////////////
const int pin_LED = 27;
const int pin_BTN = 25;
wifi_ap_record_t data[DEFAULT_SCAN_LIST_SIZE];
const static char nu[]="";
int dem = 0;
////////////////////////
int state = 0;
const static char http_html_hdr[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_index_hml[] =
		"<!DOCTYPE html>"
		"<html>"
		"<head>"
		"<title>Test</title>"
//		"<script type='text/javascript' src='popup.js'></script>"
		"</head>"
		"<body>"
							"<h2 style='text-align: center; padding: 10px; text-transform: uppercase; letter-spacing: 1px;color: #e67e32;font-size: 40px;'>Dang Nhap Wifi</h2>"
		 	 	 	 	"<form>"
				                    "<div  style ='padding-bottom: 10px; text-align: center;'>"
				                    	"<label for='ssid'>Ten Wifi</label>"
				                    	"<input style=' margin-left: 29px;' type='text' name='ssid' size='30'/><br>"
				                    "<br><label for='pass'>Mat Khau </label>"
				                    "<input style ='margin-left:10px' type='password' name='pass' size='30'/>"
				                "</div>"

							"<div style='display: inline-block; margin-left: 45%;'>"
								"<a><button onclick= \"ketnoi()\"; style ='font-size: 20px;color: white;background-color: #e67e32; border-radius: 20px;text-align: center;'>Ket noi</button></a>"
//								"<a href='/scan'><button onclick= \"scan()\"; style ='margin-left:10px;font-size: 20px;color: white;background-color: #e67e32; border-radius: 20px;text-align: center;'>Quet Wifi</button></a>"
							"</div>"
						"<script>"
						"function ketnoi() {"
						"alert('Da Luu SSID va Pass');"
						"}"
						"</script>"
				"</form>"
				"<a><button onclick= \"scan()\"; style ='margin-left:44%;font-size: 20px;color: white;background-color: #e67e32; border-radius: 20px;text-align: center;'>Quet Wifi</button></a>"
				"<script>"
				"function scan() {"
				"alert('Dang Thuc Hien Quet WIFI');"
				"window.location.assign('http://192.168.10.1/scan');"
				"}"
				"</script>"
		"</body>"
		"</html>";


//#define EXAMPLE_WIFI_SSID "Villam Tech LAB"
//#define EXAMPLE_WIFI_PASS "taokhongbiet"
//////////
//#define EXAMPLE_WIFI_SSID "CHI HAI MIA"
//#define EXAMPLE_WIFI_PASS "0352249760"

//setup wifi
#define CONFIG_AP_SSID "NARUTO UZUMAKI"
#define CONFIG_AP_PASSWORD "12345678"

#define CONFIG_AP_CHANNEL 0
#define CONFIG_AP_BEACON_INTERVAL 100
#define CONFIG_AP_MAX_CONNECTIONS 4
#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_PSK

const int DIODE_PIN = 5;

char c[30];
char pass[20];

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

// set AP CONFIG values
#ifdef CONFIG_AP_HIDE_SSID
	#define CONFIG_AP_SSID_HIDDEN 1
#else
	#define CONFIG_AP_SSID_HIDDEN 0
#endif

// Event group
static EventGroupHandle_t event_group;
const int STA_CONNECTED_BIT = BIT0;
const int STA_DISCONNECTED_BIT = BIT1;


// AP event handler
static esp_err_t event_handlerAP(void *ctx, system_event_t *event)
{
    switch(event->event_id) {

    case SYSTEM_EVENT_AP_START:
		printf("Access point started\n");
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
		xEventGroupSetBits(event_group, STA_CONNECTED_BIT);
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
		xEventGroupSetBits(event_group, STA_DISCONNECTED_BIT);
		break;

	default:
        break;
    }

	return ESP_OK;
}

// print the list of connected stations
void printStationList()
{
	printf(" Connected stations:\n");
	printf("--------------------------------------------------\n");

	wifi_sta_list_t wifi_sta_list;
	tcpip_adapter_sta_list_t adapter_sta_list;

	memset(&wifi_sta_list, 0, sizeof(wifi_sta_list));
	memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));

	ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&wifi_sta_list));
	ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list));

	for(int i = 0; i < adapter_sta_list.num; i++) {

		tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
         printf("%d - mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x - IP: %s\n", i + 1,
				station.mac[0], station.mac[1], station.mac[2],
				station.mac[3], station.mac[4], station.mac[5],
				ip4addr_ntoa(&(station.ip)));
	}

	printf("\n");
}

// Monitor task, receive Wifi AP events
void monitor_task(void *pvParameter)
{
	while(1) {

		EventBits_t staBits = xEventGroupWaitBits(event_group, STA_CONNECTED_BIT | STA_DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
		if((staBits & STA_CONNECTED_BIT) != 0)
		{
			printf("New station connected\n\n");

		}else printf("A station disconnected\n\n");
	}
}

// Station list task, print station list every 10 seconds
void station_list_task(void *pvParameter)
{
	while(1) {

		printStationList();
		vTaskDelay(10000 / portTICK_RATE_MS);
	}
}

static void esp_AP(void)
{

	// disable the default wifi logging
		esp_log_level_set("wifi", ESP_LOG_NONE);

		// create the event group to handle wifi events
		event_group = xEventGroupCreate();

		// initialize NVS
		ESP_ERROR_CHECK(nvs_flash_init());

		// initialize the tcp stack
		tcpip_adapter_init();

		// stop DHCP server
		ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

		// assign a static IP to the network interface
		tcpip_adapter_ip_info_t info;
	    memset(&info, 0, sizeof(info));
		IP4_ADDR(&info.ip, 192, 168, 10, 1);
	    IP4_ADDR(&info.gw, 192, 168, 10, 1);
	    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
		ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
		// start the DHCP server
	    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

		// initialize the wifi event handler
		ESP_ERROR_CHECK(esp_event_loop_init(event_handlerAP, NULL));

		// initialize the wifi stack in AccessPoint mode with config in RAM
				wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
				ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
				ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
				ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

		wifi_config_t ap_config = {
	        .ap = {
	            .ssid = CONFIG_AP_SSID,
	            .password = CONFIG_AP_PASSWORD,
				.ssid_len = 0,
				.channel = CONFIG_AP_CHANNEL,
				.authmode = CONFIG_AP_AUTHMODE,
				.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
				.max_connection = CONFIG_AP_MAX_CONNECTIONS,
				.beacon_interval = CONFIG_AP_BEACON_INTERVAL,
	        },
	    };
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

		// start the wifi interface
		ESP_ERROR_CHECK(esp_wifi_start());
		 xTaskCreate(&monitor_task, "monitor_task", 2048, NULL, 5, NULL);
		 xTaskCreate(&station_list_task, "station_list_task", 2048, NULL, 5, NULL);
}


//////////////// scan
static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    dem = ap_count;
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
//        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
//        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
//        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
        data[i]=ap_info[i];
        printf("aaa %s\n", data[i].ssid);
    }

}
/////////////////////////////

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

/*
 static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
*/
void test(){
	printf("chay vao roi nha\n");
}
static void http_server_netconn_serve(struct netconn *conn)
{
  struct netbuf *inbuf;
  char *buf;
  u16_t buflen;
  err_t err;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */
  err = netconn_recv(conn, &inbuf);

  if (err == ERR_OK) {
    netbuf_data(inbuf, (void**)&buf, &buflen);

    /* Is this an HTTP GET command? (only check the first 5 chars, since
    there are other formats for GET, and we're keeping it very simple )*/
    	    if (buflen>=5 &&
    	            buf[0]=='G' &&
    	            buf[1]=='E' &&
    	            buf[2]=='T' &&
    	            buf[3]==' ' &&
    	            buf[4]=='/') {
    	    	if ( buf[5]=='s'&& buf[6]=='c'&&
    	    	     buf[7]=='a'&& buf[8]=='n' && buf[9]==' ')
    	    	  {
    	    		test();
    	    	  }
    	    	if(buf[4]=='/')
    	    	{
    	    	for(int i = 0; i < 30; i++)
    	    	 {

    	    	    if (	buf[i]=='s'&&
    	    	            buf[i+1]=='s'&&
    	    	            buf[i+2]=='i'&&
    	    	            buf[i+3]=='d'&&
    	    	            buf[i+4]=='=') {
    	    	            	int j=0;
    	    	            	i+=5;
    	    	            	while(buf[i] != '&') {
    	    	            		*(c+j) = buf[i];
    	    	            		i++;j++;
    	    	            		//printf("%s\n",ssid);
    	    	            	}
    	    	            	c[j]='\0';
    	    	              	printf("%s\n",c);
    	    	              	j=0;
    	    	              	i+=6;
    	    	              	while(buf[i] != ' '){
    	    	              	     *(pass+j) = buf[i];
    	    	              	     i++;j++;
    	    	              	}
    	    	              	pass[j]='\0';
    	    	              	printf("%s\n",pass);
    	    	    }
    	    	 }
    	    	}
       /////////////////////
      netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);

      /* Send our HTML page */
      netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
    }

  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);

}


static void http_server(void *pvParameters)
{
  struct netconn *conn, *newconn;
  err_t err;
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
       http_server_netconn_serve(newconn);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}

//////////////////////
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ////

    		gpio_pad_select_gpio(pin_BTN);
        	gpio_pad_select_gpio(pin_LED);
        	//setup BTN
        	gpio_set_direction(pin_BTN, GPIO_MODE_INPUT);
        	gpio_set_pull_mode(pin_BTN, GPIO_PULLDOWN_ONLY);
        	//setup LED
        	gpio_pad_select_gpio(pin_LED);
        	gpio_set_direction(pin_LED, GPIO_MODE_OUTPUT);
    // check nut nhan
      /*a:*/    while(1){
        		if(gpio_get_level(pin_BTN)==1){
        			printf("da nhan\n");
        			gpio_set_level(pin_LED,1);
        			wifi_scan();
        			sleep(2);
        			break;
        		}
        		else
        		{
        			gpio_set_level(pin_LED,0);
        			printf("chua nhan\n");
        			sleep(2);
        		}
        	}
        printf("xu ly xong\n");
        // in ra list wifi
        for(int i=0; i < dem; i++)
        {
        	printf("aaa2 %s\n",data[i].ssid);

        }
//        goto a;

    ////
    esp_AP();

    xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);



}
