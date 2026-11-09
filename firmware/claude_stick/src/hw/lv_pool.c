#include "hw/lv_pool.h"
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"

void* thep_lv_pool_alloc(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        esp_rom_printf("FATAL lv pool PSRAM (%u bytes)\n", (unsigned)size);
        abort();
    }
    return p;
}
