// Primary game module registration for Deep.
// The editor's "Add C++ Class" wizard wrote a class named Deep over the module's Deep.h/Deep.cpp,
// which removed IMPLEMENT_PRIMARY_GAME_MODULE. Without it the editor cannot load the module
// ("The game module 'Deep' could not be successfully initialized after it was loaded").

#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, Deep, "Deep");
