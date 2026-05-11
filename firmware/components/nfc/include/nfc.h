#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// PN532 NFC — I2C adresse 0x24 (ADD1=0, ADD0=0)
// IMPORTANT : jumper I2C doit être soudé sur le module AliExpress
// (par défaut les modules sortent en mode SPI/HSU)
#define NFC_PN532_ADDR      0x24
#define NFC_UID_MAX_LEN     7   // 4 ou 7 octets selon le tag

typedef struct {
    uint8_t uid[NFC_UID_MAX_LEN];
    uint8_t uid_len;
    char    uid_str[24];    // ex: "04:AB:CD:EF"
} nfc_tag_t;

// Initialise le PN532 (SAMConfiguration — mode normal).
esp_err_t nfc_init(i2c_master_bus_handle_t bus);

// Tente de lire un tag ISO14443A. Bloque jusqu'à timeout_ms.
// Retourne ESP_OK si tag détecté, ESP_ERR_TIMEOUT sinon.
esp_err_t nfc_read_tag(nfc_tag_t *out, uint32_t timeout_ms);

// Vérification rapide non-bloquante (timeout 150ms).
bool nfc_tag_present(void);
