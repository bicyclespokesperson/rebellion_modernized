// The module table behind LoadLibraryA / GetModuleHandleA / FindResourceA, and the handle objects LoadCursorA / LoadIconA /
// LoadAcceleratorsA return. The window system reads the decoded images and tables through the accessors below.
#pragma once
#include <string>
#include <vector>

#include "formats.h"
#include "pe_module.h"

namespace mw::res
{

// LoadLibraryA semantics: `name` is looked up as given (path-translated), then in the configured install directory. The same
// file loaded twice yields the same Module with a reference count.
Module* LoadModule(const std::string& name);
bool FreeModule(Module* module);
// The module of GetModuleHandleA(NULL): <install_dir>/REBEXE.EXE, or SetMainModule()'s file.
Module* MainModule();
void SetMainModule(const std::string& nativePath);
Module* FindLoadedModule(const std::string& name);

// A cursor or icon. System cursors (LoadCursorA(NULL, IDC_*)) carry no image: `systemId` is the IDC_ value (32512 arrow...).
struct CursorObject
{
  int systemId = 0;
  Image image;
};
// Accessors for HCURSOR / HICON / HACCEL handles (opaque pointers to the objects above).
const CursorObject* CursorFromHandle(const void* handle);
const std::vector<Accelerator>* AcceleratorsFromHandle(const void* handle);

} // namespace mw::res
