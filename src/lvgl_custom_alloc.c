// src/lvgl_psram_alloc.c  (compile as C; if C++, wrap with extern "C")
#include "lvgl.h"
#include "esp_heap_caps.h"
#include <string.h>

static inline void* psram_malloc(size_t sz) {
    void* p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(!p) p = heap_caps_malloc(sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // fallback
    return p;
}

static inline void* psram_realloc(void* ptr, size_t sz) {
    void* p = heap_caps_realloc(ptr, sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if(!p && sz) p = heap_caps_realloc(ptr, sz, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return p;
}

/* LVGL v9 when LV_STDLIB_CUSTOM is selected */
void lv_mem_init(void) { /* nothing to init */ }
void lv_mem_deinit(void) { /* nothing to deinit */ }

void* lv_malloc(size_t size) { return psram_malloc(size); }
void* lv_malloc_zeroed(size_t size) {
    void* p = psram_malloc(size);
    if(p) memset(p, 0, size);
    return p;
}
void* lv_realloc(void* ptr, size_t new_size) { return psram_realloc(ptr, new_size); }
void  lv_free(void* p) { if(p) heap_caps_free(p); }
