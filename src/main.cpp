#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
//
#include "engine.hpp"

int SDL_AppInit(void** pp_appstate, int argc, char** argv) {
    *pp_appstate = new Engine();
    Engine* p_engine = static_cast<Engine*>(*pp_appstate);
    p_engine->init();
    return 0;
}
int SDL_AppEvent(void* p_appstate, const SDL_Event* p_event) {
    Engine* p_engine = static_cast<Engine*>(p_appstate);
    p_engine->execute_event(p_event);
    return 0;
}
int SDL_AppIterate(void* p_appstate) {
    Engine* p_engine = static_cast<Engine*>(p_appstate);
    if (!p_engine->_running) return 1;
    if (p_engine->_rendering) p_engine->execute_frame();
    return 0;
}
void SDL_AppQuit(void* p_appstate) {
    Engine* p_engine = static_cast<Engine*>(p_appstate);
    p_engine->destroy();
    delete p_engine;
}