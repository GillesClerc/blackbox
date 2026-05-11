#include "storage.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define TAG "storage"

static sdmmc_card_t *s_card = NULL;

esp_err_t storage_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = STORAGE_PIN_MOSI,
        .miso_io_num     = STORAGE_PIN_MISO,
        .sclk_io_num     = STORAGE_PIN_CLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t ret = spi_bus_initialize(STORAGE_SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = STORAGE_SPI_HOST;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = STORAGE_PIN_CS;
    slot_cfg.host_id = STORAGE_SPI_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = STORAGE_MAX_FILES,
        .allocation_unit_size   = 16 * 1024,
    };

    ret = esp_vfs_fat_sdspi_mount(STORAGE_MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Montage SD échoué: %s", esp_err_to_name(ret));
        spi_bus_free(STORAGE_SPI_HOST);
        return ret;
    }

    ESP_LOGI(TAG, "SD montée sur %s — %s %.1f GB",
             STORAGE_MOUNT_POINT,
             s_card->cid.name,
             (double)((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) / (1024.0 * 1024.0 * 1024.0));
    return ESP_OK;
}

void storage_unmount(void)
{
    if (!s_card) return;
    esp_vfs_fat_sdcard_unmount(STORAGE_MOUNT_POINT, s_card);
    spi_bus_free(STORAGE_SPI_HOST);
    s_card = NULL;
    ESP_LOGI(TAG, "SD démontée");
}

bool storage_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

esp_err_t storage_read_file(const char *path, uint8_t *buf, size_t max_len, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Fichier introuvable: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    size_t n = fread(buf, 1, max_len, f);
    fclose(f);
    if (out_len) *out_len = n;
    ESP_LOGI(TAG, "Lu %zu octets depuis %s", n, path);
    return ESP_OK;
}

esp_err_t storage_write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Impossible de créer: %s", path);
        return ESP_FAIL;
    }
    size_t written = fwrite(buf, 1, len, f);
    fclose(f);
    if (written != len) {
        ESP_LOGE(TAG, "Écriture incomplète: %zu/%zu", written, len);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Écrit %zu octets dans %s", len, path);
    return ESP_OK;
}

void storage_list_dir(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGW(TAG, "Répertoire introuvable: %s", dir_path);
        return;
    }
    ESP_LOGI(TAG, "Contenu de %s :", dir_path);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
    }
    closedir(dir);
}
