#include "blitzmax/embedded_runtime.h"

#include "diskio_wl.h"
#include "esp_partition.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "wear_levelling.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <sys/utime.h>

#define BMX_FATFS_PATH "/flash"
#define BMX_FATFS_LABEL "storage"

typedef struct BMXESP8266File {
    FILE *file;
    char *path;
} BMXESP8266File;

typedef struct BMXESP8266Directory {
    DIR *directory;
} BMXESP8266Directory;

static int32_t bmx_fs_mounted;
static int32_t bmx_fs_error;
static wl_handle_t bmx_fs_wl_handle = WL_INVALID_HANDLE;

static int32_t bmx_fs_set_error(int32_t error) {
    bmx_fs_error = error;
    return error;
}

static const esp_partition_t *bmx_fs_partition(void) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT, BMX_FATFS_LABEL);
}

static int bmx_fs_partition_blank(void) {
    const esp_partition_t *partition = bmx_fs_partition();
    if (!partition) return 0;
    uint8_t data[256];
    for (size_t offset = 0; offset < partition->size; offset += sizeof(data)) {
        size_t count = partition->size - offset;
        if (count > sizeof(data)) count = sizeof(data);
        if (esp_partition_read(partition, offset, data, count) != ESP_OK) return 0;
        for (size_t index = 0; index < count; ++index) {
            if (data[index] != 0xffu) return 0;
        }
    }
    return 1;
}

static char *bmx_fs_path(const BMXEmbeddedString *path) {
    if (!bmx_fs_mounted) return NULL;
    char *relative = (char *)bmx_embedded_string_to_utf8_string(path);
    if (!relative) return NULL;
    size_t base_size = strlen(BMX_FATFS_PATH);
    size_t relative_size = strlen(relative);
    int slash = relative_size == 0 || relative[0] != '/';
    char *result = (char *)bbMemAlloc(base_size + (size_t)slash + relative_size + 1u);
    if (result) {
        memcpy(result, BMX_FATFS_PATH, base_size);
        if (slash) result[base_size++] = '/';
        memcpy(result + base_size, relative, relative_size + 1u);
    }
    bbMemFree(relative);
    return result;
}

int32_t bmx_esp8266_fatfs_mount(int32_t format_blank) {
    if (bmx_fs_mounted) return bmx_fs_set_error(0);
    esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = format_blank && bmx_fs_partition_blank(),
        .max_files = 8,
        .allocation_unit_size = 4096
    };
    esp_err_t result = esp_vfs_fat_spiflash_mount(BMX_FATFS_PATH,
        BMX_FATFS_LABEL, &config, &bmx_fs_wl_handle);
    if (result == ESP_OK) bmx_fs_mounted = 1;
    else bmx_fs_wl_handle = WL_INVALID_HANDLE;
    return bmx_fs_set_error(result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp8266_fatfs_unmount(void) {
    if (!bmx_fs_mounted) return bmx_fs_set_error(0);
    esp_err_t result = esp_vfs_fat_spiflash_unmount(BMX_FATFS_PATH,
        bmx_fs_wl_handle);
    if (result == ESP_OK) {
        bmx_fs_mounted = 0;
        bmx_fs_wl_handle = WL_INVALID_HANDLE;
    }
    return bmx_fs_set_error(result == ESP_OK ? 0 : -(int32_t)result);
}

int32_t bmx_esp8266_fatfs_format(void) {
    if (bmx_fs_mounted) {
        int32_t result = bmx_esp8266_fatfs_unmount();
        if (result) return result;
    }
    const esp_partition_t *partition = bmx_fs_partition();
    if (!partition) return bmx_fs_set_error(-(int32_t)ESP_ERR_NOT_FOUND);
    esp_err_t result = esp_partition_erase_range(partition, 0, partition->size);
    if (result != ESP_OK) return bmx_fs_set_error(-(int32_t)result);
    return bmx_esp8266_fatfs_mount(1);
}

int32_t bmx_esp8266_fatfs_is_mounted(void) { return bmx_fs_mounted; }
int32_t bmx_esp8266_fatfs_last_error(void) { return bmx_fs_error; }

static int64_t bmx_fs_space(int free_space) {
    if (!bmx_fs_mounted || bmx_fs_wl_handle == WL_INVALID_HANDLE) return -1;
    unsigned char drive = ff_diskio_get_pdrv_wl(bmx_fs_wl_handle);
    if (drive == 0xffu) return -1;
    char path[3] = {(char)('0' + drive), ':', 0};
    DWORD free_clusters = 0;
    FATFS *filesystem = NULL;
    FRESULT result = f_getfree(path, &free_clusters, &filesystem);
    if (result != FR_OK || !filesystem) {
        bmx_fs_set_error(-(int32_t)result);
        return -1;
    }
#if FF_MAX_SS == FF_MIN_SS
    uint64_t sector_size = FF_MAX_SS;
#else
    uint64_t sector_size = filesystem->ssize;
#endif
    uint64_t clusters = free_space ? free_clusters : filesystem->n_fatent - 2u;
    bmx_fs_set_error(0);
    return (int64_t)(clusters * filesystem->csize * sector_size);
}

int64_t bmx_esp8266_fatfs_capacity(void) { return bmx_fs_space(0); }
int64_t bmx_esp8266_fatfs_free(void) { return bmx_fs_space(1); }

void *bmx_esp8266_filesystem_open(const BMXEmbeddedString *path,
        int32_t readable, int32_t write_mode) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) {
        bmx_fs_set_error(-ENOENT);
        return NULL;
    }
    const char *mode = readable ?
        (write_mode == 1 ? "w+b" : write_mode == 2 ? "a+b" : "rb") :
        (write_mode == 2 ? "ab" : "wb");
    BMXESP8266File *handle = (BMXESP8266File *)bbMemAlloc(sizeof(*handle));
    if (handle) {
        handle->file = fopen(native_path, mode);
        handle->path = native_path;
    }
    if (!handle || !handle->file) {
        int error = handle ? errno : ENOMEM;
        bbMemFree(native_path);
        if (handle) bbMemFree(handle);
        bmx_fs_set_error(-error);
        return NULL;
    }
    bmx_fs_set_error(0);
    return handle;
}

int32_t bmx_esp8266_filesystem_close(void *opaque) {
    if (!opaque) return 0;
    BMXESP8266File *handle = (BMXESP8266File *)opaque;
    int result = handle->file ? fclose(handle->file) : 0;
    bbMemFree(handle->path);
    bbMemFree(handle);
    if (result) bmx_fs_set_error(-errno);
    return result;
}

int64_t bmx_esp8266_filesystem_read(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || (uint64_t)count > SIZE_MAX) return -1;
    FILE *file = ((BMXESP8266File *)opaque)->file;
    if (!file) return -1;
    size_t result = fread(buffer, 1, (size_t)count, file);
    if (!result && ferror(file)) {
        bmx_fs_set_error(-errno);
        return -1;
    }
    return (int64_t)result;
}

int64_t bmx_esp8266_filesystem_write(void *opaque, void *buffer, int64_t count) {
    if (!opaque || !buffer || count < 0 || (uint64_t)count > SIZE_MAX) return -1;
    FILE *file = ((BMXESP8266File *)opaque)->file;
    if (!file) return -1;
    size_t result = fwrite(buffer, 1, (size_t)count, file);
    if (result != (size_t)count) {
        bmx_fs_set_error(-errno);
        return -1;
    }
    return (int64_t)result;
}

int64_t bmx_esp8266_filesystem_position(void *opaque) {
    FILE *file = opaque ? ((BMXESP8266File *)opaque)->file : NULL;
    return file ? ftell(file) : -1;
}

int64_t bmx_esp8266_filesystem_size(void *opaque) {
    if (!opaque) return -1;
    FILE *file = ((BMXESP8266File *)opaque)->file;
    if (!file) return -1;
    long position = ftell(file);
    if (position < 0 || fseek(file, 0, SEEK_END)) return -1;
    long size = ftell(file);
    (void)fseek(file, position, SEEK_SET);
    return size;
}

int64_t bmx_esp8266_filesystem_seek(void *opaque, int64_t offset,
        int32_t whence) {
    FILE *file = opaque ? ((BMXESP8266File *)opaque)->file : NULL;
    if (!file || offset < LONG_MIN || offset > LONG_MAX ||
            fseek(file, (long)offset, whence)) return -1;
    return ftell(file);
}

static int bmx_fs_reopen(BMXESP8266File *handle, long position) {
    handle->file = fopen(handle->path, "r+b");
    if (!handle->file) return -1;
    if (!fseek(handle->file, position, SEEK_SET)) return 0;
    fclose(handle->file);
    handle->file = NULL;
    return -1;
}

int32_t bmx_esp8266_filesystem_resize(void *opaque, int64_t size) {
    if (!opaque || size < 0 || size > LONG_MAX) return -1;
    BMXESP8266File *handle = (BMXESP8266File *)opaque;
    if (!handle->file) return -1;
    long position = ftell(handle->file);
    if (position < 0 || fflush(handle->file)) return -1;
    int result = fclose(handle->file);
    handle->file = NULL;

    struct stat info;
    if (!result) result = stat(handle->path, &info);
    if (!result && size < info.st_size) {
        result = truncate(handle->path, (off_t)size);
    } else if (!result && size > info.st_size) {
        FILE *file = fopen(handle->path, "r+b");
        if (!file || fseek(file, (long)size - 1, SEEK_SET) || fputc(0, file) == EOF || fflush(file)) {
            result = -1;
        }
        if (file) fclose(file);
    }

    if (!result && position > size) position = (long)size;
    if (bmx_fs_reopen(handle, position)) return -1;
    return result;
}

int32_t bmx_esp8266_filesystem_flush(void *opaque) {
    FILE *file = opaque ? ((BMXESP8266File *)opaque)->file : NULL;
    return file ? fflush(file) : -1;
}

int32_t bmx_esp8266_filesystem_stat(const BMXEmbeddedString *path,
        int32_t *type, int64_t *size, int64_t *modified, int64_t *created,
        int64_t *accessed, int32_t *read_only) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) return -1;
    struct stat info;
    int result = stat(native_path, &info);
    bbMemFree(native_path);
    if (result) return -1;
    if (type) *type = S_ISDIR(info.st_mode) ? 2 : 1;
    if (size) *size = info.st_size;
    if (modified) *modified = info.st_mtime;
    if (created) *created = info.st_ctime;
    if (accessed) *accessed = info.st_atime;
    if (read_only) *read_only = (info.st_mode & S_IWUSR) == 0;
    return 0;
}

int32_t bmx_esp8266_filesystem_set_time(const BMXEmbeddedString *path,
        int64_t time, int32_t time_type) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) return -1;
    struct stat info;
    struct utimbuf times;
    int result = stat(native_path, &info);
    if (!result) {
        times.actime = time_type == 2 ? time : info.st_atime;
        times.modtime = time_type == 0 ? time : info.st_mtime;
        result = esp_vfs_utime(native_path, &times);
    }
    bbMemFree(native_path);
    return result;
}

int32_t bmx_esp8266_filesystem_mkdir(const BMXEmbeddedString *path) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) return -1;
    int result = mkdir(native_path, 0777);
    bbMemFree(native_path);
    return result;
}

int32_t bmx_esp8266_filesystem_remove(const BMXEmbeddedString *path) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) return -1;
    int result = remove(native_path);
    bbMemFree(native_path);
    return result;
}

int32_t bmx_esp8266_filesystem_rename(const BMXEmbeddedString *old_path,
        const BMXEmbeddedString *new_path) {
    char *old_native = bmx_fs_path(old_path);
    char *new_native = bmx_fs_path(new_path);
    if (!old_native || !new_native) {
        if (old_native) bbMemFree(old_native);
        if (new_native) bbMemFree(new_native);
        return -1;
    }
    int result = rename(old_native, new_native);
    bbMemFree(old_native);
    bbMemFree(new_native);
    return result;
}

void *bmx_esp8266_filesystem_directory_open(const BMXEmbeddedString *path) {
    char *native_path = bmx_fs_path(path);
    if (!native_path) return NULL;
    BMXESP8266Directory *handle =
        (BMXESP8266Directory *)bbMemAlloc(sizeof(*handle));
    if (handle) handle->directory = opendir(native_path);
    bbMemFree(native_path);
    if (!handle || !handle->directory) {
        if (handle) bbMemFree(handle);
        return NULL;
    }
    return handle;
}

const BMXEmbeddedString *bmx_esp8266_filesystem_directory_next(void *opaque) {
    if (!opaque) return &bmx_embedded_empty_string;
    struct dirent *entry = readdir(((BMXESP8266Directory *)opaque)->directory);
    return entry ?
        bmx_embedded_string_from_utf8_string((const uint8_t *)entry->d_name) :
        &bmx_embedded_empty_string;
}

void bmx_esp8266_filesystem_directory_close(void *opaque) {
    if (!opaque) return;
    BMXESP8266Directory *handle = (BMXESP8266Directory *)opaque;
    closedir(handle->directory);
    bbMemFree(handle);
}
