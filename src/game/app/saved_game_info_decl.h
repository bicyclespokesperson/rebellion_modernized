#pragma once
// Declaration of the saved-game header record (0x28 bytes, 0x411730-0x411970 in the multiplayer window's address
// range); its owner reconstructs it. The shell reads and writes it around every save file and describes the game
// in the load list.
#include <cstdint>

#include "core/game_string.h"

class istream;
class ostream;

class SavedGameInfo {
public:
    SavedGameInfo();                       // 0x411730
    SavedGameInfo(const SavedGameInfo&);   // 0x4117c0
    virtual ~SavedGameInfo();              // 0x4118f0 (scalar deleting destructor first in vtable 0x6587e0)
    SavedGameInfo& operator=(const SavedGameInfo&);  // 0x411860
    void Read(istream* in);                // 0x411900
    void Write(ostream* out);              // 0x411970
    void SetName(String name);             // 0x415890 (this module; the argument is passed by value)
    String GetName() const;                // 0x408d90

    String m_name;           // +0x04
    int m_unknown10;         // +0x10 stored by the application as m_unknown130 (1 in a freshly written header)
    int m_unknown14;         // +0x14
    int m_gameType;          // +0x18 RebellionApplication::m_gameType
    int m_slot;              // +0x1c save slot
    int m_side;              // +0x20 RebellionApplication::m_side
    int m_unknown24;         // +0x24
};
