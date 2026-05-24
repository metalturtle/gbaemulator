#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool gba_load_rom(const uint8_t* data, uint32_t size);
void gba_reset(void);
void gba_run_frame(void);
const uint32_t* gba_framebuffer(void);
void gba_set_button(uint32_t button, bool pressed);
const uint8_t* gba_export_save(uint32_t* size);
bool gba_import_save(const uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif
