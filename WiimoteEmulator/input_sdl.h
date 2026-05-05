#ifndef INPUT_SDL_H
#define INPUT_SDL_H

#include <stdbool.h>
#include "input.h"

void input_sdl_init(void);

extern struct input_source input_source_sdl;

#endif
