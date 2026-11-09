#pragma once
#include <stddef.h>

// Pool de memória do LVGL na PSRAM. Ligado ao LVGL pelo lv_conf.h:
//   LV_MEM_POOL_INCLUDE "hw/lv_pool.h"
//   LV_MEM_POOL_ALLOC(size) thep_lv_pool_alloc(size)
// O lv_mem_core_builtin.c chama uma vez, em lv_init(), com LV_MEM_SIZE.
// Libera a RAM interna para as sessões TLS (~40–50 KB cada).

#ifdef __cplusplus
extern "C" {
#endif

// Aloca `size` bytes na PSRAM. Sem PSRAM, imprime "FATAL lv pool PSRAM" e
// chama abort(): nunca devolve NULL.
void* thep_lv_pool_alloc(size_t size);

#ifdef __cplusplus
}
#endif
