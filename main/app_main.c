/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP32 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"

#if defined(CONFIG_BT_ENABLED)
#include "esp_bt.h"
#endif

#include "esp_at.h"
#include "mdns.h"

#include "at_upgrade.h"
#include "at_interface.h"

#ifdef CONFIG_AT_ETHERNET_SUPPORT
#include "at_eth_init.h"
#endif
#include "lwip/sockets.h"
#include "lwip/api.h"

static uint8_t at_exeCmdCipupdate(uint8_t *cmd_name)//add get station ip and ap ip
{

    if (esp_at_upgrade_process(ESP_AT_OTA_MODE_NORMAL, NULL)) {
        esp_at_response_result(ESP_AT_RESULT_CODE_OK);
        esp_at_port_wait_write_complete(portMAX_DELAY);
        esp_restart();
        for (;;) {
        }
    }

    return ESP_AT_RESULT_CODE_ERROR;
}

static uint8_t at_setupCmdCipupdate(uint8_t para_num)
{
    int32_t ota_mode = 0;
    int32_t cnt = 0;
    uint8_t* version = NULL;

    if (esp_at_get_para_as_digit(cnt++, &ota_mode) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (cnt < para_num) {
        if (esp_at_get_para_as_str(cnt++, &version) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }

    if (cnt != para_num) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (esp_at_upgrade_process(ota_mode, version)) {
        esp_at_response_result(ESP_AT_RESULT_CODE_OK);
        esp_at_port_wait_write_complete(portMAX_DELAY);
        esp_restart();
        for (;;) {
        }
    }

    return ESP_AT_RESULT_CODE_ERROR;
}

typedef struct {
    int data[32];
} at_linkConType;

extern uint8_t at_ipMux;
extern at_linkConType *pLink;
extern uint8_t at_netconn_max_num;
extern void esp_at_printf_error_code(esp_err_t err);

static uint8_t at_queryCmdSystime(uint8_t *cmd_name)
{
    struct timeval tv = { .tv_sec = 0,.tv_usec = 0 };
    uint8_t buffer[64];

    gettimeofday(&tv, NULL);

    snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%ld.%06lu\r\n", cmd_name, tv.tv_sec, tv.tv_usec);
    esp_at_port_write_data(buffer, strlen((char*)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setupCmdCipsetopt(uint8_t para_num)
{
    int32_t cnt = 0;
    uint8_t buffer[64];
    at_linkConType *link;
    int linkID = 0;
    char *optname;

    if (at_ipMux) {
        if (esp_at_get_para_as_digit(cnt++, &linkID) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if ((linkID < 0) || (linkID > at_netconn_max_num)) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "ID ERROR\r\n");
            esp_at_port_write_data(buffer, strlen((char*)buffer));

            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else {
        linkID = 0;
    }

    link = &pLink[linkID];
    int s = link->data[22];
    if (s < 0) {
        if (at_ipMux) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "%d,CLOSED\r\n", linkID);
        }
        else {
            snprintf((char*)buffer, sizeof(buffer) - 1, "CLOSED\r\n");
        }
        esp_at_port_write_data(buffer, strlen((char*)buffer));

        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (esp_at_get_para_as_str(cnt++, (uint8_t **)&optname) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (strcmp(optname, "SO_KEEPALIVE") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "SO_RCVTIMEO") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "SO_REUSEADDR") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "IP_ADD_MEMBERSHIP") == 0) {
        struct ip_mreq optval;
        ip_addr_t if_addr;
        char *imr_multiaddr;
        char *imr_interface;

        if (esp_at_get_para_as_str(cnt++, (uint8_t **)&imr_multiaddr) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (esp_at_get_para_as_str(cnt++, (uint8_t **)&imr_interface) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (ipaddr_aton(imr_multiaddr, &if_addr) == 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        inet_addr_from_ip4addr(&optval.imr_multiaddr, ip_2_ip4(&if_addr));

        if (ipaddr_aton(imr_interface, &if_addr) == 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        inet_addr_from_ip4addr(&optval.imr_interface, ip_2_ip4(&if_addr));

        if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "IP_DROP_MEMBERSHIP") == 0) {
        struct ip_mreq optval;
        ip_addr_t if_addr;
        char *imr_multiaddr;
        char *imr_interface;

        if (esp_at_get_para_as_str(cnt++, (uint8_t **)&imr_multiaddr) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (esp_at_get_para_as_str(cnt++, (uint8_t **)&imr_interface) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (ipaddr_aton(imr_multiaddr, &if_addr) == 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        inet_addr_from_ip4addr(&optval.imr_multiaddr, ip_2_ip4(&if_addr));

        if (ipaddr_aton(imr_interface, &if_addr) == 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
        inet_addr_from_ip4addr(&optval.imr_interface, ip_2_ip4(&if_addr));

        if (setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "TCP_KEEPALIVE") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "TCP_KEEPIDLE") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "TCP_KEEPINTVL") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else if (strcmp(optname, "TCP_NODELAY") == 0) {
        int optval = 0;

        if (esp_at_get_para_as_digit(cnt++, &optval) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (cnt != para_num) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval)) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setupCmdCipgetopt(uint8_t para_num)
{
    int32_t cnt = 0;
    const char *cmd_name = "+CIPGETOPT";
    uint8_t buffer[64];
    at_linkConType *link;
    int linkID = 0;
    char *optname;
    socklen_t size = 0;

    if (at_ipMux) {
        if (esp_at_get_para_as_digit(cnt++, &linkID) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        if ((linkID < 0) || (linkID > at_netconn_max_num)) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "ID ERROR\r\n");
            esp_at_port_write_data(buffer, strlen((char*)buffer));

            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else {
        linkID = 0;
    }

    link = &pLink[linkID];
    int s = link->data[22];
    if (s < 0) {
        if (at_ipMux) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "%d,CLOSED\r\n", linkID);
        }
        else {
            snprintf((char*)buffer, sizeof(buffer) - 1, "CLOSED\r\n");
        }
        esp_at_port_write_data(buffer, strlen((char*)buffer));

        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (esp_at_get_para_as_str(cnt++, (uint8_t **)&optname) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (cnt != para_num) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (strcmp(optname, "SO_ERROR") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "SO_KEEPALIVE") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "SO_RCVTIMEO") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "SO_REUSEADDR") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "TCP_KEEPALIVE") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "TCP_KEEPIDLE") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "TCP_KEEPINTVL") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else if (strcmp(optname, "TCP_NODELAY") == 0) {
        int optval = 0;
        size = sizeof(optval);

        if (getsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, &size) < 0) {
            return ESP_AT_RESULT_CODE_ERROR;
        }

        snprintf((char*)buffer, sizeof(buffer) - 1, "%s:%d\r\n", cmd_name, optval);
    }
    else {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    esp_at_port_write_data(buffer, strlen((char*)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static uint8_t at_setupCmdCipqryhst(uint8_t para_num)
{
    char* hostname = NULL;
    int32_t timeout = 0;
    const char *cmd_name = "+CIPQRYHST";
    int32_t cnt = 0;
    struct ip4_addr addr;
    addr.addr = 0;
    uint8_t buffer[64];

    if (esp_at_get_para_as_str(cnt++, (uint8_t **)&hostname) != ESP_AT_PARA_PARSE_RESULT_OK) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    if (cnt < para_num) {
        if (esp_at_get_para_as_digit(cnt++, &timeout) != ESP_AT_PARA_PARSE_RESULT_OK) {
            return ESP_AT_RESULT_CODE_ERROR;
        }
    }
    else {
        timeout = 1000;
    }

    if (cnt != para_num) {
        return ESP_AT_RESULT_CODE_ERROR;
    }

    esp_err_t err = mdns_query_a(hostname, timeout, &addr);
    if (err) {
        if (err == ESP_ERR_NOT_FOUND) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "%s:NOT FOUND\r\n", cmd_name);
            esp_at_port_write_data(buffer, strlen((char*)buffer));
        }
        else if (err == ESP_ERR_TIMEOUT) {
            snprintf((char*)buffer, sizeof(buffer) - 1, "%s:TIMEOUT\r\n", cmd_name);
            esp_at_port_write_data(buffer, strlen((char*)buffer));
        }
        else {
            esp_at_printf_error_code(err);
        }

        return ESP_AT_RESULT_CODE_ERROR;
    }

    snprintf((char*)buffer, sizeof(buffer) - 1, "%s:\"" IPSTR "\"\r\n", cmd_name, IP2STR(&addr));
    esp_at_port_write_data(buffer, strlen((char*)buffer));

    return ESP_AT_RESULT_CODE_OK;
}

static esp_at_cmd_struct at_update_cmd[] = {
    {"+CIUPDATE", NULL, NULL, at_setupCmdCipupdate, at_exeCmdCipupdate},
    {"+SYSTIME", NULL, at_queryCmdSystime, NULL, NULL},
    {"+CIPSETOPT", NULL, NULL, at_setupCmdCipsetopt, NULL},
    {"+CIPGETOPT", NULL, NULL, at_setupCmdCipgetopt, NULL},
    {"+CIPQRYHST", NULL, NULL, at_setupCmdCipqryhst, NULL},
};

void app_main()
{
    uint8_t *version = (uint8_t *)malloc(192);
#ifdef CONFIG_AT_COMMAND_TERMINATOR
    uint8_t cmd_terminator[2] = { CONFIG_AT_COMMAND_TERMINATOR,0 };
#endif

    nvs_flash_init();
    at_interface_init();

    esp_err_t err = mdns_init();
    if (err) {
        printf("mdns init fail\r\n");
    }

    sprintf((char*)version, "compile time:%s %s\r\n", __DATE__, __TIME__);
#ifdef CONFIG_ESP_AT_FW_VERSION
    if ((strlen(CONFIG_ESP_AT_FW_VERSION) > 0) && (strlen(CONFIG_ESP_AT_FW_VERSION) <= 128)) {
        printf("%s\r\n", CONFIG_ESP_AT_FW_VERSION);
        strcat((char*)version, CONFIG_ESP_AT_FW_VERSION);
    }
#endif
    esp_at_module_init(CONFIG_LWIP_MAX_SOCKETS - 1, version);  // reserved one for server
    free(version);

#ifdef CONFIG_AT_BASE_COMMAND_SUPPORT
    if (esp_at_base_cmd_regist() == false) {
        printf("regist base cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_WIFI_COMMAND_SUPPORT
    if (esp_at_wifi_cmd_regist() == false) {
        printf("regist wifi cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_NET_COMMAND_SUPPORT
    if (esp_at_net_cmd_regist() == false) {
        printf("regist net cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_BLE_COMMAND_SUPPORT
    if (esp_at_ble_cmd_regist() == false) {
        printf("regist ble cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_BT_COMMAND_SUPPORT
    if (esp_at_bt_cmd_regist() == false) {
        printf("regist bt cmd fail\r\n");
    }
#endif

#if defined(CONFIG_BT_ENABLED)
#ifdef CONFIG_AT_BLE_COMMAND_SUPPORT
#if !defined(CONFIG_AT_BT_COMMAND_SUPPORT)
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif
#else
#if defined(CONFIG_AT_BT_COMMAND_SUPPORT)
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
#else
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif
#endif
#endif

#ifdef CONFIG_AT_ETHERNET_SUPPORT
    if (at_eth_init() == false) {
        printf("ethernet init fail\r\n");
    }
    else {
        if (esp_at_eth_cmd_regist() == false) {
            printf("regist ethernet cmd fail\r\n");
        }
    }
#endif

#ifdef CONFIG_AT_FS_COMMAND_SUPPORT
    if (esp_at_fs_cmd_regist() == false) {
        printf("regist fs cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_EAP_COMMAND_SUPPORT
    if (esp_at_eap_cmd_regist() == false) {
        printf("regist eap cmd fail\r\n");
    }
#endif

#ifdef CONFIG_AT_COMMAND_TERMINATOR
    esp_at_custom_cmd_line_terminator_set((uint8_t*)&cmd_terminator);
#endif

    esp_at_custom_cmd_array_regist(at_update_cmd, sizeof(at_update_cmd) / sizeof(at_update_cmd[0]));
    at_custom_init();
}
