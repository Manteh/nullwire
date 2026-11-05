#pragma once
// Raw (unaccelerated) mouse input for the mouse->stick bridge. macOS only.
namespace MacMouse {
void start();                          // idempotent; registers GCMouse handlers
void setCapture(bool on);              // hide cursor + stop it moving
bool captured();
void takeDeltas(double &dx, double &dy); // counts since last call; dy is +down (screen convention)
unsigned takeEventCount();              // mouse reports received since last call
void setHeadlessCapture(bool on);      // test channel: capture without touching the real cursor
void inject(double dx, double dy);     // test channel: exact mouse counts
void setTestButtons(unsigned bits);    // test channel: bit0 fire (R2), bit1 aim (L2)
void beep(int times);                  // audible cue, the player is watching the TV
unsigned buttons();                    // bit0 left, bit1 right, bit2 middle, bit3 back, bit4 forward
}
