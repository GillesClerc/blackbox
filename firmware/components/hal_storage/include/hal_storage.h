#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// SD card sur SPI2, bus partagé avec l'e-ink SSD1680 (MOSI=11, CLK=12 communs).
// MISO=15 et CS=47 : 13 est le BUSY e-ink et 10 son CS (FSD §3.2.2, pins gelées).
#define STORAGE_PIN_MOSI    11
#define STORAGE_PIN_MISO    15
#define STORAGE_PIN_CLK     12
#define STORAGE_PIN_CS      47

#define STORAGE_MOUNT_POINT "/sdcard"
#define STORAGE_MAX_FILES   8

// Monte la carte SD et initialise le filesystem FAT.
esp_err_t hal_storage_init(void);

// Démonte proprement.
void hal_storage_unmount(void);

// Retourne true si le fichier existe.
bool hal_storage_file_exists(const char *path);

// Lit un fichier entier dans buf (alloué par l'appelant, taille max_len).
// *out_len reçoit le nombre d'octets lus.
esp_err_t hal_storage_read_file(const char *path, uint8_t *buf, size_t max_len, size_t *out_len);

// Écrit buf dans path (crée ou écrase).
esp_err_t hal_storage_write_file(const char *path, const uint8_t *buf, size_t len);

// Liste les fichiers d'un répertoire (log uniquement, pour debug).
void hal_storage_list_dir(const char *dir_path);
