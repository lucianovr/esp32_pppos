// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "exs82w.h"

#define PWR_ON_PIN	(14)

#define MODEM_RESULT_CODE_POWERDOWN "POWERED DOWN"

static const char *DCE_TAG = "exs82w";
#define DCE_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(DCE_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)

/**
 * @brief EXS82W Modem
 *
 */
typedef struct {
    void *priv_resource; /*!< Private resource */
    modem_dce_t parent;  /*!< DCE parent class */
} exs82w_modem_dce_t;

/**
 * @brief Handle response from AT+CESQ
 */
static esp_err_t exs82w_handle_cesq(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    exs82w_modem_dce_t *exs82w_dce = __containerof(dce, exs82w_modem_dce_t, parent);
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else if (!strncmp(line, "+CESQ", strlen("+CESQ"))) {
        /* store value of rssi and ber */
        uint32_t **csq = exs82w_dce->priv_resource;
        /* +CESQ: <rxlev>, <ber>, <rscp>, <ecno>, <rsrq>, <rsrp> */
        /* retrieve only <rsrq> and <rsrp> */
        sscanf(line, "%*s%*d%*d%*d%*d%d,%d", csq[0], csq[1]);
        err = ESP_OK;
    }
    return err;
}

/**
 * @brief Handle response from +++
 */
static esp_err_t exs82w_handle_exit_data_mode(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_NO_CARRIER)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    }
    return err;
}

/**
 * @brief Handle response from ATD*99#
 */
static esp_err_t exs82w_handle_atd_ppp(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_CONNECT)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    }
    return err;
}

/**
 * @brief Handle response from AT+CGMM
 */
static esp_err_t exs82w_handle_cgmm(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else {
        int len = snprintf(dce->name, MODEM_MAX_NAME_LENGTH, "%s", line);
        if (len > 2) {
            /* Strip "\r\n" */
            strip_cr_lf_tail(dce->name, len);
            err = ESP_OK;
        }
    }
    return err;
}

/**
 * @brief Handle response from AT+CGSN
 */
static esp_err_t exs82w_handle_cgsn(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else {
        int len = snprintf(dce->imei, MODEM_IMEI_LENGTH + 1, "%s", line);
        if (len > 2) {
            /* Strip "\r\n" */
            strip_cr_lf_tail(dce->imei, len);
            err = ESP_OK;
        }
    }
    return err;
}

/**
 * @brief Handle response from AT+CIMI
 */
static esp_err_t exs82w_handle_cimi(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else {
        int len = snprintf(dce->imsi, MODEM_IMSI_LENGTH + 1, "%s", line);
        if (len > 2) {
            /* Strip "\r\n" */
            strip_cr_lf_tail(dce->imsi, len);
            err = ESP_OK;
        }
    }
    return err;
}

/**
 * @brief Handle response from AT+COPS?
 */
static esp_err_t exs82w_handle_cops(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else if (!strncmp(line, "+COPS", strlen("+COPS"))) {
        /* there might be some random spaces in operator's name, we can not use sscanf to parse the result */
        /* strtok will break the string, we need to create a copy */
        size_t len = strlen(line);
        char *line_copy = malloc(len + 1);
        strcpy(line_copy, line);
        /* +COPS: <mode>[, <format>[, <oper>[, <Act>]]] */
        char *str_ptr = NULL;
        char *p[5];
        uint8_t i = 0;
        /* strtok will broke string by replacing delimiter with '\0' */
        p[i] = strtok_r(line_copy, ",", &str_ptr);
        while (p[i]) {
            p[++i] = strtok_r(NULL, ",", &str_ptr);
        }
        if (i >= 3) {
            int len = snprintf(dce->oper, MODEM_MAX_OPERATOR_LENGTH, "%s", p[2]);
            if (len > 2) {
                /* Strip "\r\n" */
                strip_cr_lf_tail(dce->oper, len);
                err = ESP_OK;
            }
        }
        if (i >= 4) {
            dce->act = (uint8_t)strtol(p[3], NULL, 0);
        }
        free(line_copy);
    }
    return err;
}

/**
 * @brief Handle response from AT^SMSO
 */
static esp_err_t exs82w_handle_power_down(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    }
    return err;
}

/**
 * @brief Hang up
 *
 * @param dce Modem DCE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_hang_up(modem_dce_t *dce)
{
    modem_dte_t *dte = dce->dte;
    dce->handle_line = esp_modem_dce_handle_response_default;
    DCE_CHECK(dte->send_cmd(dte, "AT&D2\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
    DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "hang up failed", err);
    ESP_LOGD(DCE_TAG, "hang up ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Get signal quality
 * TODO fix params
 * @param dce Modem DCE object
 * @param rssi received signal strength indication
 * @param ber bit error ratio
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_get_signal_quality(modem_dce_t *dce, uint32_t *rssi, uint32_t *ber)
{
    modem_dte_t *dte = dce->dte;
    exs82w_modem_dce_t *exs82w_dce = __containerof(dce, exs82w_modem_dce_t, parent);
    uint32_t *resource[2] = {rssi, ber};
    exs82w_dce->priv_resource = resource;
    dce->handle_line = exs82w_handle_cesq;
    DCE_CHECK(dte->send_cmd(dte, "AT+CESQ\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
    DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "inquire signal quality failed", err);
    ESP_LOGD(DCE_TAG, "inquire signal quality ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Set Working Mode
 *
 * @param dce Modem DCE object
 * @param mode woking mode
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_set_working_mode(modem_dce_t *dce, modem_mode_t mode)
{
    modem_dte_t *dte = dce->dte;
    switch (mode) {
    case MODEM_COMMAND_MODE:
        vTaskDelay(pdMS_TO_TICKS(1000)); // spec: 1s delay for the modem to recognize the escape sequence
        dce->handle_line = exs82w_handle_exit_data_mode;
        if (dte->send_cmd(dte, "+++", MODEM_COMMAND_TIMEOUT_MODE_CHANGE) != ESP_OK) {
            // "+++" Could fail if we are already in the command mode.
            // in that case we ignore the timeout and re-sync the modem
            ESP_LOGI(DCE_TAG, "Sending \"+++\" command failed");
            dce->handle_line = esp_modem_dce_handle_response_default;
            DCE_CHECK(dte->send_cmd(dte, "AT\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
            DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "sync failed", err);
        } else {
            DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "enter command mode failed", err);
        }
        ESP_LOGD(DCE_TAG, "enter command mode ok");
        dce->mode = MODEM_COMMAND_MODE;
        break;
    case MODEM_PPP_MODE:
        dce->handle_line = exs82w_handle_atd_ppp;
        DCE_CHECK(dte->send_cmd(dte, "ATD*99***1#\r", MODEM_COMMAND_TIMEOUT_MODE_CHANGE) == ESP_OK, "send command failed", err);
        if (dce->state != MODEM_STATE_SUCCESS) {
            // Initiate PPP mode could fail, if we've already "dialed" the data call before.
            // in that case we retry with "ATO" to just resume the data mode
            ESP_LOGD(DCE_TAG, "enter ppp mode failed, retry with ATO");
            dce->handle_line = exs82w_handle_atd_ppp;
            DCE_CHECK(dte->send_cmd(dte, "ATO\r", MODEM_COMMAND_TIMEOUT_MODE_CHANGE) == ESP_OK, "send command failed", err);
            DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "enter ppp mode failed", err);
        }
        ESP_LOGD(DCE_TAG, "enter ppp mode ok");
        dce->mode = MODEM_PPP_MODE;
        break;
    default:
        ESP_LOGW(DCE_TAG, "unsupported working mode: %d", mode);
        goto err;
        break;
    }
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Power down
 *
 * @param exs82w_dce exs82w object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_power_down(modem_dce_t *dce)
{
    modem_dte_t *dte = dce->dte;
    dce->handle_line = exs82w_handle_power_down;
    DCE_CHECK(dte->send_cmd(dte, "AT^SMSO\r", MODEM_COMMAND_TIMEOUT_POWEROFF) == ESP_OK, "send command failed", err);
    DCE_CHECK(dce->state == MODEM_STATE_SUCCESS, "power down failed", err);
    ESP_LOGD(DCE_TAG, "power down ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Get DCE module name
 *
 * @param exs82w_dce exs82w object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_get_module_name(exs82w_modem_dce_t *exs82w_dce)
{
    modem_dte_t *dte = exs82w_dce->parent.dte;
    exs82w_dce->parent.handle_line = exs82w_handle_cgmm;
    DCE_CHECK(dte->send_cmd(dte, "AT+CGMM\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
    DCE_CHECK(exs82w_dce->parent.state == MODEM_STATE_SUCCESS, "get module name failed", err);
    ESP_LOGD(DCE_TAG, "get module name ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Get DCE module IMEI number
 *
 * @param exs82w_dce exs82w object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_get_imei_number(exs82w_modem_dce_t *exs82w_dce)
{
    modem_dte_t *dte = exs82w_dce->parent.dte;
    exs82w_dce->parent.handle_line = exs82w_handle_cgsn;
    DCE_CHECK(dte->send_cmd(dte, "AT+CGSN\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
    DCE_CHECK(exs82w_dce->parent.state == MODEM_STATE_SUCCESS, "get imei number failed", err);
    ESP_LOGD(DCE_TAG, "get imei number ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Get DCE module IMSI number
 *
 * @param exs82w_dce exs82w object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_get_imsi_number(exs82w_modem_dce_t *exs82w_dce)
{
    modem_dte_t *dte = exs82w_dce->parent.dte;
    exs82w_dce->parent.handle_line = exs82w_handle_cimi;
    DCE_CHECK(dte->send_cmd(dte, "AT+CIMI\r", MODEM_COMMAND_TIMEOUT_DEFAULT) == ESP_OK, "send command failed", err);
    DCE_CHECK(exs82w_dce->parent.state == MODEM_STATE_SUCCESS, "get imsi number failed", err);
    ESP_LOGD(DCE_TAG, "get imsi number ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Get Operator's name
 *
 * @param dce Modem DCE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
static esp_err_t exs82w_get_operator_name(modem_dce_t *dce)
{
    modem_dte_t *dte = dce->dte;
    exs82w_modem_dce_t *exs82w_dce = __containerof(dce, exs82w_modem_dce_t, parent);
    exs82w_dce->parent.handle_line = exs82w_handle_cops;
    DCE_CHECK(dte->send_cmd(dte, "AT+COPS?\r", MODEM_COMMAND_TIMEOUT_OPERATOR) == ESP_OK, "send command failed", err);
    DCE_CHECK(exs82w_dce->parent.state == MODEM_STATE_SUCCESS, "get network operator failed", err);
    ESP_LOGD(DCE_TAG, "get network operator ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}

/**
 * @brief Deinitialize exs82w object
 *
 * @param dce Modem DCE object
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on fail
 */
static esp_err_t exs82w_deinit(modem_dce_t *dce)
{
    exs82w_modem_dce_t *exs82w_dce = __containerof(dce, exs82w_modem_dce_t, parent);
    if (dce->dte) {
        dce->dte->dce = NULL;
    }
    free(exs82w_dce);
    return ESP_OK;
}

modem_dce_t *exs82w_init(modem_dte_t *dte)
{
    DCE_CHECK(dte, "DCE should bind with a DTE", err);
    /* malloc memory for exs82w_dce object */
    exs82w_modem_dce_t *exs82w_dce = calloc(1, sizeof(exs82w_modem_dce_t));
    DCE_CHECK(exs82w_dce, "calloc exs82w_dce failed", err);
    /* Bind DTE with DCE */
    exs82w_dce->parent.dte = dte;
    dte->dce = &(exs82w_dce->parent);
    /* Bind methods */
    exs82w_dce->parent.handle_line = NULL;
    exs82w_dce->parent.sync = esp_modem_dce_sync;
    exs82w_dce->parent.echo_mode = esp_modem_dce_echo;
    exs82w_dce->parent.store_profile = esp_modem_dce_store_profile;
    exs82w_dce->parent.set_flow_ctrl = esp_modem_dce_set_flow_ctrl;
    exs82w_dce->parent.define_pdp_context = esp_modem_dce_define_pdp_context;
    exs82w_dce->parent.hang_up = exs82w_hang_up;
    exs82w_dce->parent.get_signal_quality = exs82w_get_signal_quality;
    exs82w_dce->parent.get_battery_status = NULL;
    exs82w_dce->parent.get_operator_name = exs82w_get_operator_name;
    exs82w_dce->parent.set_working_mode = exs82w_set_working_mode;
    exs82w_dce->parent.power_down = exs82w_power_down;
    exs82w_dce->parent.deinit = exs82w_deinit;

    gpio_reset_pin(PWR_ON_PIN);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(PWR_ON_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(PWR_ON_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_set_level(PWR_ON_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PWR_ON_PIN, 0);

    vTaskDelay(pdMS_TO_TICKS(12000));

    /* Sync between DTE and DCE */
    DCE_CHECK(esp_modem_dce_sync(&(exs82w_dce->parent)) == ESP_OK, "sync failed", err_io);
    /* Close echo */
    DCE_CHECK(esp_modem_dce_echo(&(exs82w_dce->parent), false) == ESP_OK, "close echo mode failed", err_io);
    /* Get Module name */
    DCE_CHECK(exs82w_get_module_name(exs82w_dce) == ESP_OK, "get module name failed", err_io);
    /* Get IMEI number */
    DCE_CHECK(exs82w_get_imei_number(exs82w_dce) == ESP_OK, "get imei failed", err_io);
    /* Get IMSI number */
    DCE_CHECK(exs82w_get_imsi_number(exs82w_dce) == ESP_OK, "get imsi failed", err_io);
    /* Get operator name */
    DCE_CHECK(exs82w_get_operator_name(&(exs82w_dce->parent)) == ESP_OK, "get operator name failed", err_io);
    return &(exs82w_dce->parent);
err_io:
    free(exs82w_dce);
err:
    return NULL;
}
