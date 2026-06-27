#if 1

#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

/* Delay stubs (niet nodig op pc) */
#define LV_DRV_DELAY_INCLUDE  <stdint.h>
#define LV_DRV_DELAY_US(us)
#define LV_DRV_DELAY_MS(ms)

/* Display stub */
#define LV_DRV_DISP_INCLUDE         <stdint.h>
#define LV_DRV_DISP_CMD_DATA(val)
#define LV_DRV_DISP_RST(val)
#define LV_DRV_DISP_SPI_CS(val)
#define LV_DRV_DISP_SPI_WR_BYTE(data)
#define LV_DRV_DISP_SPI_WR_ARRAY(adr, n)
#define LV_DRV_DISP_PAR_CS(val)
#define LV_DRV_DISP_PAR_SLOW
#define LV_DRV_DISP_PAR_FAST
#define LV_DRV_DISP_PAR_WR_WORD(data)
#define LV_DRV_DISP_PAR_WR_ARRAY(adr, n)

/* Input stub */
#define LV_DRV_INDEV_INCLUDE     <stdint.h>
#define LV_DRV_INDEV_RST(val)
#define LV_DRV_INDEV_IRQ_READ    0
#define LV_DRV_INDEV_SPI_CS(val)
#define LV_DRV_INDEV_SPI_XCHG_BYTE(data) 0
#define LV_DRV_INDEV_I2C_START
#define LV_DRV_INDEV_I2C_STOP
#define LV_DRV_INDEV_I2C_RESTART
#define LV_DRV_INDEV_I2C_WR(data)
#define LV_DRV_INDEV_I2C_READ(last_read) 0

/* ── SDL display + muis driver (480×480) ── */
#define USE_SDL               1
#define SDL_HOR_RES           480
#define SDL_VER_RES           480
#define SDL_ZOOM              1
#define SDL_DOUBLE_BUFFERED   0
#define SDL_INCLUDE_PATH      <SDL2/SDL.h>
#define SDL_DUAL_DISPLAY      0

/* Alles anders uit */
#define USE_SDL_GPU           0
#define USE_MONITOR           0
#define USE_WINDOWS           0
#define USE_WIN32DRV          0
#define USE_GTK               0
#define USE_WAYLAND           0
#define USE_SSD1963           0
#define USE_ILI9341           0
#define USE_HX8357D           0
#define USE_ILI9486L          0
#define USE_ST7565             0
#define USE_ST7735S            0
#define USE_UC1610            0
#define USE_SHARP_MIP         0
#define USE_MONO_WIN32        0
#define USE_GC9A01            0
#define USE_R61581            0
#define USE_FBDEV             0
#define USE_DRM               0
#define USE_XPT2046           0
#define USE_FT5406EE8         0
#define USE_AD_TOUCH          0
#define USE_EVDEV             0
#define USE_LIBINPUT          0
#define USE_MOUSE             0
#define USE_MOUSEWHEEL        0
#define USE_KEYBOARD          0
#define USE_XKB               0

#endif /* LV_DRV_CONF_H */
#endif
