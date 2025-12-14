#ifndef VIRTUAL_MOUSE_H
#define VIRTUAL_MOUSE_H

#include <InputServerDevice.h>
#include <InterfaceDefs.h>
#include <Input.h>
#include <SupportDefs.h>

#ifndef B_MOUSE_POINTING_DEVICE
#define B_MOUSE_POINTING_DEVICE	1
#endif

// Packet Types
enum {
    PACKET_MOUSE = 1,
    PACKET_KEY   = 2
};

// Unified Input Packet
struct input_packet {
    int32 type;
    union {
        struct {
            float x;        // 0.0 to 1.0
            float y;        // 0.0 to 1.0
            uint32 buttons; // Mask
            float wheel_x;
            float wheel_y;
        } mouse;
        struct {
            uint32 key_code; // Haiku Key Code
            bool down;       // true=KeyDown, false=KeyUp
            uint32 modifiers; 
            uint32 key_utf32;
        } key;
    } data;
};

class VirtualMouse : public BInputServerDevice {
public:
    VirtualMouse();
    virtual ~VirtualMouse();

    virtual status_t InitCheck();
    virtual status_t Start(const char* name, void* cookie);
    virtual status_t Stop(const char* name, void* cookie);
    virtual status_t Control(const char* name, void* cookie, uint32 command, BMessage* message);

private:
    static status_t _InputLoop(void* arg);
    
    thread_id fThread;
    port_id   fPort;
    volatile bool fRunning;

    // State Tracking
    float fLastX;
    float fLastY;
    uint32 fLastButtons;
    
    // Click Handling
    bigtime_t fClickSpeed;
    int32     fLastClickBtn;
    bigtime_t fLastClickTime;
    int32     fClickCount;
    
    int32     fRef;
};

// Required Export
extern "C" _EXPORT BInputServerDevice* instantiate_input_device();

#endif // VIRTUAL_MOUSE_H