#ifndef MENU_COMMANDS_H
#define MENU_COMMANDS_H

namespace melonDS {
class NDS;
}

namespace melonDS {
namespace FrontendSDL {

class Video;

bool CheckMenuCommands(NDS& nds, Video& video, bool& quit);
void WriteMenuState(NDS& nds);
void InitMenuCommands(NDS& nds);

} // namespace FrontendSDL
} // namespace melonDS

#endif
