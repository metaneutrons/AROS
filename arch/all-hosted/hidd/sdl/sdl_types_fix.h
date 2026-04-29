/* Fix SDL_config_minimal.h types for AArch64 */
#ifndef _AROS_SDL_TYPES_FIX_H
#define _AROS_SDL_TYPES_FIX_H

/* Pre-define the types that SDL_config_minimal.h gets wrong on arm64 */
#include <stdint.h>
#include <stddef.h>

/* Prevent SDL_config_minimal.h from redefining these */
#define _SDL_config_minimal_h

/* Provide the SDL config defines that the minimal config would set */
#define SDL_AUDIO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_DUMMY 1

#endif
