/**
 * lv_conf.h — config do LVGL 9.2 para o Claude Usage Stick (JC4832W535).
 *
 * Compile com -DLV_CONF_INCLUDE_SIMPLE (ver build.sh) para que o LVGL ache
 * este arquivo pelo include path do sketch. Alternativa: copiar este arquivo
 * para a pasta de libraries do Arduino (um nível acima da pasta `lvgl`).
 *
 * Arquivo parcial: o que não estiver definido aqui usa o default do
 * lv_conf_internal.h. As opções abaixo são as que importam para esta placa.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Guarda obrigatória: lv_conf_internal.h inclui este arquivo também durante a
   montagem dos .S do LVGL (ex.: draw/sw/blend/helium/lv_blend_helium.S). Sem a
   guarda, o assembler recebe os typedef de stdint.h e falha com
   "unknown opcode or format name 'typedef'". Ver o aviso no lv_conf_internal.h. */
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/*====================
   COLOR
 *====================*/
#define LV_COLOR_DEPTH 16
/* Pipeline validado no bring-up usa cópia direta RGB565 (sem swap).
   Se vermelho/azul saírem trocados, mude para 1. */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMÓRIA
   Pool do LVGL (objetos/estilos), alocado na PSRAM em lv_init() por
   thep_lv_pool_alloc (src/hw/lv_pool.c), para deixar a RAM interna às
   sessões TLS. As duas macros só valem com LV_MEM_ADR == 0 (o padrão);
   são lidas por lv_mem_core_builtin.c. O include resolve pelo -I src do
   build.sh. O buffer de render full-screen (480x320x2) é alocado à parte
   na PSRAM, dentro do sketch.
 *=========================*/
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN
#define LV_MEM_SIZE            (96 * 1024U)
#define LV_MEM_POOL_INCLUDE    "hw/lv_pool.h"
#define LV_MEM_POOL_ALLOC(size) thep_lv_pool_alloc(size)

/*====================
   HAL / SISTEMA
 *====================*/
#define LV_USE_OS LV_OS_NONE
/* tick vem de lv_tick_set_cb(millis) no sketch */

/*====================
   RENDER
 *====================*/
#define LV_USE_DRAW_SW 1

/*====================
   FONTES
 *====================*/
/* Fase 3 (item 42): a UI usa as JetBrains Mono de src/ui/fonts/. Da
 * Montserrat sobra só a 14: fallback das fontes de texto (LV_SYMBOL_*, ex.:
 * o ↻ do rodapé) e fonte padrão do LVGL. As outras (12, 16-48) saíram depois
 * de conferir com grep que nada mais as usa. */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   LOG (só na build de desenvolvimento, -DTHEP_DEV)
 *====================*/
#ifdef THEP_DEV
  #define LV_USE_LOG 1
  #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
  #define LV_LOG_PRINTF 0      /* a saída vai pelo callback do setup() (Serial) */
#endif

/*====================
   WIDGETS usados
 *====================*/
#define LV_USE_LABEL        1
#define LV_USE_BUTTON       1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_BAR          1
#define LV_USE_LIST         1
#define LV_USE_TEXTAREA     1
#define LV_USE_KEYBOARD     1
#define LV_USE_CANVAS       1
#define LV_USE_IMAGE        1
#define LV_USE_LINE         1
#define LV_USE_ARC          1
#define LV_USE_SPINNER      1
#define LV_USE_TILEVIEW     0   /* fase 3: shell + menu no lugar do tileview */
#define LV_USE_CHART        1

#endif /*LV_CONF_H*/
