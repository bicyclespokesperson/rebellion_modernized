#pragma once

// Stand-in for the drag-list module's ui/cool_scrollable.h (that version is canonical and replaces this file at
// merge): the interface a scrolled control gives its scroll bars (vtable 0x66e118). CoolScrollBar only calls
// OnScrollEnd; the other slots are declared so the vtable index matches.
class CoolScrollable {
public:
    virtual ~CoolScrollable();                 // slot 0, 0x60a440 (scalar deleting destructor)
    virtual int PageUp();                      // slot 1
    virtual int PageDown();                    // slot 2
    virtual int LineUp();                      // slot 3
    virtual int LineDown();                    // slot 4
    virtual int ScrollVertical(int delta);     // slot 5
    virtual int PageLeft();                    // slot 6
    virtual int PageRight();                   // slot 7
    virtual int LineRight();                   // slot 8
    virtual int LineLeft();                    // slot 9
    virtual int ScrollHorizontal(int delta);   // slot 10
    virtual void OnScrollEnd();                // slot 11: the mouse button was released over a bar
};
