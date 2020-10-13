#ifndef INCLUDE_CONSTANTS_CPP
#define INCLUDE_CONSTANTS_CPP

/**
 * A bunch of constants that's required to be known globally.
 */

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int MAX_CONE_LIGHTS = 4;
const int MAX_POINT_LIGHTS = 6;
const int MAX_TEXT_LENGTH = 80;
const int MAX_TEXT_CHARS = 10240;

int VIEWPORT_WIDTH = WINDOW_WIDTH;
int VIEWPORT_HEIGHT = WINDOW_HEIGHT;
int FRAMEBUFFER_WIDTH = VIEWPORT_WIDTH * 2;
int FRAMEBUFFER_HEIGHT = VIEWPORT_HEIGHT * 2;
int TEXT_HEIGHT = VIEWPORT_HEIGHT / 21;

#endif