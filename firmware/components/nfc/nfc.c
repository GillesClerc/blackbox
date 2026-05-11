#include "nfc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

#define TAG "nfc"

// Commandes PN532
#define CMD_SAM_CONFIG          0x14
#define CMD_IN_LIST_PASSIVE     0x4A

// TFI (Transport Frame Identifier)
#define TFI_HOST_TO_PN532       0xD4
#define TFI_PN532_TO_HOST       0xD5

static i2c_master_dev_handle_t s_dev = NULL;

// --- Couche transport PN532 ---

static esp_err_t pn532_build_frame(uint8_t *frame, uint8_t *frame_len,
                                   uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t len = 2 + data_len;  // TFI + CMD + data
    uint8_t lcs = (uint8_t)(-(int)len);
    uint8_t dcs = TFI_HOST_TO_PN532 + cmd;
    for (int i = 0; i < data_len; i++) dcs += data[i];
    dcs = (uint8_t)(-(int)dcs);

    frame[0] = 0x00;              // preamble
    frame[1] = 0x00;              // start code
    frame[2] = 0xFF;              // start code
    frame[3] = len;               // length
    frame[4] = lcs;               // length checksum
    frame[5] = TFI_HOST_TO_PN532; // TFI
    frame[6] = cmd;
    memcpy(&frame[7], data, data_len);
    frame[7 + data_len] = dcs;
    frame[8 + data_len] = 0x00;   // postamble
    *frame_len = 9 + data_len;
    return ESP_OK;
}

static esp_err_t pn532_wait_ready(uint32_t timeout_ms)
{
    uint8_t status = 0;
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        // Le premier octet renvoyé par PN532 en I2C est le statut : 0x01 = prêt
        if (i2c_master_receive(s_dev, &status, 1, pdMS_TO_TICKS(20)) == ESP_OK
            && status == 0x01) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t pn532_read_ack(void)
{
    // ACK = [status=0x01, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00]
    uint8_t buf[7];
    esp_err_t ret = pn532_wait_ready(500);
    if (ret != ESP_OK) return ret;
    ret = i2c_master_receive(s_dev, buf, sizeof(buf), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;
    if (buf[1] != 0x00 || buf[2] != 0x00 || buf[3] != 0xFF ||
        buf[4] != 0x00 || buf[5] != 0xFF) {
        ESP_LOGE(TAG, "ACK invalide");
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

static esp_err_t pn532_send_cmd(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t frame[32];
    uint8_t frame_len;
    pn532_build_frame(frame, &frame_len, cmd, data, data_len);
    esp_err_t ret = i2c_master_transmit(s_dev, frame, frame_len, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;
    return pn532_read_ack();
}

static esp_err_t pn532_read_response(uint8_t expected_cmd,
                                     uint8_t *data, uint8_t *data_len,
                                     uint32_t timeout_ms)
{
    esp_err_t ret = pn532_wait_ready(timeout_ms);
    if (ret != ESP_OK) return ret;

    // Lire entête + buffer large (32 octets de données max)
    uint8_t raw[40];
    ret = i2c_master_receive(s_dev, raw, sizeof(raw), pdMS_TO_TICKS(100));
    if (ret != ESP_OK) return ret;

    // raw[0] = status, raw[1]=0x00, raw[2]=0x00, raw[3]=0xFF
    // raw[4] = LEN, raw[5] = LCS, raw[6] = TFI, raw[7] = CMD+1, raw[8..] = data
    if (raw[0] != 0x01 || raw[6] != TFI_PN532_TO_HOST || raw[7] != expected_cmd + 1) {
        ESP_LOGE(TAG, "Réponse inattendue (status=%02X tfi=%02X cmd=%02X)",
                 raw[0], raw[6], raw[7]);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uint8_t len = raw[4] - 2;  // LEN inclut TFI + CMD, on soustrait les 2
    if (data_len) *data_len = len;
    if (data && len) memcpy(data, &raw[8], len);
    return ESP_OK;
}

// --- API publique ---

esp_err_t nfc_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = NFC_PN532_ADDR,
        .scl_speed_hz    = 100000,  // PN532 : 100 kHz max en I2C
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PN532 non trouvé sur le bus I2C");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // délai wake-up

    // SAMConfiguration : mode normal, pas de IRQ
    uint8_t sam_data[] = { 0x01, 0x14, 0x00 };
    ret = pn532_send_cmd(CMD_SAM_CONFIG, sam_data, sizeof(sam_data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SAMConfiguration échouée");
        return ret;
    }

    uint8_t rsp_len;
    ret = pn532_read_response(CMD_SAM_CONFIG, NULL, &rsp_len, 500);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "PN532 initialisé");
    return ESP_OK;
}

esp_err_t nfc_read_tag(nfc_tag_t *out, uint32_t timeout_ms)
{
    if (!s_dev || !out) return ESP_ERR_INVALID_STATE;

    // InListPassiveTarget : 1 cible, ISO14443A (106 kbps)
    uint8_t cmd_data[] = { 0x01, 0x00 };
    esp_err_t ret = pn532_send_cmd(CMD_IN_LIST_PASSIVE, cmd_data, sizeof(cmd_data));
    if (ret != ESP_OK) return ret;

    uint8_t rsp[20];
    uint8_t rsp_len;
    ret = pn532_read_response(CMD_IN_LIST_PASSIVE, rsp, &rsp_len, timeout_ms);
    if (ret != ESP_OK) return ret;

    // rsp[0] = NbTg (nb de tags trouvés)
    if (rsp[0] == 0) return ESP_ERR_NOT_FOUND;

    // rsp[1]=Tg, rsp[2..3]=ATQA, rsp[4]=SAK, rsp[5]=NfcIdLength, rsp[6..]=NfcId
    uint8_t uid_len = rsp[5];
    if (uid_len > NFC_UID_MAX_LEN) uid_len = NFC_UID_MAX_LEN;

    out->uid_len = uid_len;
    memcpy(out->uid, &rsp[6], uid_len);

    // Formater UID en string "04:AB:CD:EF"
    char *p = out->uid_str;
    for (int i = 0; i < uid_len; i++) {
        if (i > 0) *p++ = ':';
        p += sprintf(p, "%02X", out->uid[i]);
    }
    *p = '\0';

    ESP_LOGI(TAG, "Tag détecté : %s", out->uid_str);
    return ESP_OK;
}

bool nfc_tag_present(void)
{
    nfc_tag_t tag;
    return nfc_read_tag(&tag, 150) == ESP_OK;
}
