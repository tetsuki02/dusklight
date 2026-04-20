#include "dusk/hook.hpp"
#include "dusk/mod_api.h"

extern "C" {

void mod_init(DuskModAPI* api) {
    dusk::init(api);
}

void mod_tick(DuskModAPI* api) {
    (void)api;
}

void mod_cleanup(DuskModAPI* api) {
    (void)api;
}

}
