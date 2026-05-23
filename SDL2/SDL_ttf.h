#pragma once

#include <SDL3_ttf/SDL_ttf.h>

#ifndef TTF_GetError
#define TTF_GetError SDL_GetError
#endif

#ifndef TTF_RenderUTF8_Blended
#define TTF_RenderUTF8_Blended(font, text, color) TTF_RenderText_Blended((font), (text), 0, (color))
#endif

#ifndef TTF_SizeUTF8
#define TTF_SizeUTF8(font, text, w, h) (TTF_GetStringSize((font), (text), 0, (w), (h)) ? 0 : -1)
#endif

#ifdef __cplusplus
inline TTF_Font* TTF_OpenFont(const char* file, int ptsize) {
    return TTF_OpenFont(file, static_cast<float>(ptsize));
}
#endif
