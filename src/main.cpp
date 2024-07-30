#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include "core/engine.hpp"

int SDL_AppInit(void** appstate_pp, int argc, char** argv) {
    *appstate_pp = new Engine();
    Engine* engine_p = static_cast<Engine*>(*appstate_pp);
    engine_p->init();
    return SDL_APP_CONTINUE;
}
int SDL_AppEvent(void* appstate_p, const SDL_Event* event_p) {
    Engine* engine_p = static_cast<Engine*>(appstate_p);
    engine_p->execute_event(event_p);
    return SDL_APP_CONTINUE;
}
int SDL_AppIterate(void* appstate_p) {
    Engine* engine_p = static_cast<Engine*>(appstate_p);
    if (!engine_p->_running) return SDL_APP_SUCCESS;
    if (engine_p->_rendering) engine_p->execute_frame();
    return SDL_APP_CONTINUE;
}
void SDL_AppQuit(void* appstate_p) {
    Engine* engine_p = static_cast<Engine*>(appstate_p);
    engine_p->destroy();
    delete engine_p;
}