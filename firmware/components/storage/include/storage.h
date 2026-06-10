#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// SD card via SPI2 (FSPI), pins IO_MUX natifs : SPI3 est pris par les yeux GC9A01,
// GPIO14 = CS oeil droit, GPIO19/20 = USB D-/D+ → interdits.
#define STORAGE_PIN_MOSI    11
#define STORAGE_PIN_MISO    13
#define STORAGE_PIN_CLK     12
#define STORAGE_PIN_CS      10

#define STORAGE_MOUNT_POINT "/sdcard"
#define STORAGE_MAX_FILES   8

// Monte la carte SD et initialise le filesystem FAT.
esp_err_t storage_init(void);

// Démonte proprement.
void storage_unmount(void);

// Retourne true si le fichier existe.
bool storage_file_exists(const char *path);

// Lit un fichier entier dans buf (alloué par l'appelant, taille max_len).
// *out_len reçoit le nombre d'octets lus.
esp_err_t storage_read_file(const char *path, uint8_t *buf, size_t max_len, size_t *out_len);

// Écrit buf dans path (crée ou écrase).
esp_err_t storage_write_file(const char *path, const uint8_t *buf, size_t len);

// Liste les fichiers d'un répertoire (log uniquement, pour debug).
void storage_list_dir(const char *dir_path);
