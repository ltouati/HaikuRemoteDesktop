#include "VirtualMouse.h"
#include <Message.h>
#include <OS.h>
#include <InterfaceDefs.h>
#include <syslog.h>

#define PORT_NAME "virtual_mouse_input"

// --- Entry Point ---
BInputServerDevice *instantiate_input_device() {
    return new VirtualMouse();
}

// --- Implementation ---

// Global static instances to ensure lifetime persists as long as the add-on is loaded
static input_device_ref sMouseDevice = {(char *) "VirtualMouse", B_POINTING_DEVICE, nullptr};
static input_device_ref sKeybdDevice = {(char *) "VirtualKeyboard", B_KEYBOARD_DEVICE, nullptr};
static input_device_ref *sDeviceList[3] = {&sMouseDevice, &sKeybdDevice, nullptr};

static VirtualMouse *sMouse = nullptr;
static VirtualMouse *sKeyboard = nullptr;
static int32 sRef = 0;

VirtualMouse::VirtualMouse()
    : BInputServerDevice(), fThread(-1), fPort(-1), fRunning(false),
      fLastX(0.5f), fLastY(0.5f), fLastButtons(0),
      fClickSpeed(500000), fLastClickBtn(-1), fLastClickTime(0), fClickCount(0) {
    get_click_speed(&fClickSpeed);
}

VirtualMouse::~VirtualMouse() {
    if (fRunning) Stop(nullptr, nullptr);
}

status_t VirtualMouse::InitCheck() {
    RegisterDevices(sDeviceList);
    syslog(LOG_INFO, "VirtualMouse: Loaded");
    return B_OK;
}

status_t VirtualMouse::Start(const char *name, void *cookie) {
    if (strcmp(name, "VirtualMouse") == 0)
        sMouse = this;
    else if (strcmp(name, "VirtualKeyboard") == 0)
        sKeyboard = this;

    if (atomic_add(&sRef, 1) > 0)
        return B_OK;

    // 1. Create the mailbox (Port)
    fPort = create_port(100, PORT_NAME);
    if (fPort < 0) return fPort;

    // 2. Start the watcher thread
    fRunning = true;
    fThread = spawn_thread(_InputLoop, "VirtualMouseWatcher", B_NORMAL_PRIORITY, this);

    if (fThread < 0) {
        delete_port(fPort);
        return fThread;
    }

    resume_thread(fThread);
    return B_OK;
}

status_t VirtualMouse::Stop(const char *name, void *cookie) {
    if (strcmp(name, "VirtualMouse") == 0)
        sMouse = nullptr;
    else if (strcmp(name, "VirtualKeyboard") == 0)
        sKeyboard = nullptr;

    if (atomic_add(&sRef, -1) > 1)
        return B_OK;

    fRunning = false;
    // Killing the port forces read_port to fail in the thread, exiting the loop cleanly
    syslog(LOG_INFO, "VirtualMouse: Stopping 1");
    if (fPort >= 0) {
        delete_port(fPort);
        fPort = -1;
    }
    syslog(LOG_INFO, "VirtualMouse: Stopping 2");
    status_t result;
    wait_for_thread(fThread, &result);
    syslog(LOG_INFO, "VirtualMouse: Stopping 3");
    return B_OK;
}

status_t VirtualMouse::Control(const char *name, void *cookie, uint32 command, BMessage *message) {
    return B_OK;
}

// --- The Core Logic ---
status_t VirtualMouse::_InputLoop(void *arg) {
    VirtualMouse * self = (VirtualMouse *) arg;
    int32 msgCode;
    input_packet packet;
    // syslog(LOG_INFO, "[*] VirtualMouse::_InputLoop()\n");
    while (self->fRunning) {
        // Block until Python sends data
        ssize_t bytes = read_port(self->fPort, &msgCode, &packet, sizeof(packet));


        if (bytes == sizeof(packet)) {
            bigtime_t now = system_time();

            if (packet.type == PACKET_MOUSE) {
                // 1. Motion Event
                if (packet.data.mouse.x != self->fLastX || packet.data.mouse.y != self->fLastY) {
                    BMessage *message = new BMessage(B_MOUSE_MOVED);
                    if (message) {
                        message->AddInt64("when", now);
                        message->AddInt32("be:device_subtype", B_MOUSE_POINTING_DEVICE);
                        message->AddInt32("buttons", packet.data.mouse.buttons);
                        message->AddFloat("x", packet.data.mouse.x);
                        message->AddFloat("y", packet.data.mouse.y);
                        message->AddFloat("be:tablet_x", packet.data.mouse.x);
                        message->AddFloat("be:tablet_y", packet.data.mouse.y);
                        // syslog(LOG_INFO, "[*] VirtualMouse::Motion Event\n");   
                        if (sMouse) sMouse->EnqueueMessage(message);
                        else delete message;
                    }

                    self->fLastX = packet.data.mouse.x;
                    self->fLastY = packet.data.mouse.y;
                }

                // 2. Button Events
                uint32 changes = packet.data.mouse.buttons ^ self->fLastButtons;
                if (changes != 0) {
                    for (int i = 0; i < 32; i++) {
                        uint32 mask = 1 << i;
                        if (changes & mask) {
                            bool down = (packet.data.mouse.buttons & mask);

                            BMessage *event = new BMessage(down ? B_MOUSE_DOWN : B_MOUSE_UP);
                            event->AddInt64("when", now);
                            event->AddInt32("buttons", packet.data.mouse.buttons);
                            event->AddFloat("be:tablet_x", self->fLastX);
                            event->AddFloat("be:tablet_y", self->fLastY);
                            event->AddFloat("x", self->fLastX);
                            event->AddFloat("y", self->fLastY);

                            if (down) {
                                if (i == self->fLastClickBtn &&
                                    (now - self->fLastClickTime) < self->fClickSpeed) {
                                    self->fClickCount++;
                                } else {
                                    self->fClickCount = 1;
                                }
                                self->fLastClickBtn = i;
                                self->fLastClickTime = now;
                                event->AddInt32("clicks", self->fClickCount);
                            }

                            if (sMouse) sMouse->EnqueueMessage(event);
                            else delete event;
                        }
                    }
                    self->fLastButtons = packet.data.mouse.buttons;
                }

                // 3. Wheel Events
                if (packet.data.mouse.wheel_x != 0.0f || packet.data.mouse.wheel_y != 0.0f) {
                    BMessage *wheel = new BMessage(B_MOUSE_WHEEL_CHANGED);
                    if (wheel) {
                        wheel->AddInt64("when", now);
                        wheel->AddFloat("be:wheel_delta_x", packet.data.mouse.wheel_x);
                        wheel->AddFloat("be:wheel_delta_y", packet.data.mouse.wheel_y);
                        if (sMouse) sMouse->EnqueueMessage(wheel);
                        else delete wheel;
                    }
                }
            } else if (packet.type == PACKET_KEY) {
                // Keyboard Event
                // We receive full Modifier and Char Code data from Browser

                uint32 what = packet.data.key.down ? B_KEY_DOWN : B_KEY_UP;

                uint32 c = packet.data.key.key_utf32;

                // Handle Control Keys (Ctrl-C -> 0x03)
                if (packet.data.key.modifiers & B_CONTROL_KEY) {
                    if (c >= 'a' && c <= 'z') c -= 96;
                    else if (c >= 'A' && c <= 'Z') c -= 64;
                }

                // Convert UTF-32 to UTF-8
                char utf8[5] = {0};
                if (c < 0x80) {
                    utf8[0] = (char) c;
                } else if (c < 0x800) {
                    utf8[0] = 0xC0 | (c >> 6);
                    utf8[1] = 0x80 | (c & 0x3F);
                } else if (c < 0x10000) {
                    utf8[0] = 0xE0 | (c >> 12);
                    utf8[1] = 0x80 | ((c >> 6) & 0x3F);
                    utf8[2] = 0x80 | (c & 0x3F);
                } else {
                    utf8[0] = 0xF0 | (c >> 18);
                    utf8[1] = 0x80 | ((c >> 12) & 0x3F);
                    utf8[2] = 0x80 | ((c >> 6) & 0x3F);
                    utf8[3] = 0x80 | (c & 0x3F);
                }

                // syslog(LOG_INFO, "VirtualMouse: Key Code 0x%x Char 0x%x Mods 0x%x (%s)\n", 
                //    packet.data.key.key_code, c, packet.data.key.modifiers, packet.data.key.down ? "Down" : "Up");

                BMessage *event = new BMessage(what);
                event->AddInt64("when", now);
                event->AddInt32("key", packet.data.key.key_code);
                event->AddInt32("modifiers", packet.data.key.modifiers);
                event->AddString("bytes", utf8);
                event->AddInt32("raw_char", c);

                // Legacy single-byte field (often used by system filters)
                // Use first byte of UTF-8 ?? Or just ASCII value if fits?
                // Usually it's the specific mapped byte.
                // If I send "byte", InputServer checks it for Command+Space etc.
                if (utf8[0]) {
                    event->AddInt8("byte", (int8) utf8[0]);
                }


                if (sKeyboard) {
                    sKeyboard->EnqueueMessage(event);
                    // syslog(LOG_INFO, "VirtualMouse: Enqueued Key Event 0x%x\n", 
                    //    packet.data.key.key_code);
                } else {
                    syslog(LOG_ERR, "VirtualMouse: sKeyboard is nullptr! Event dropped.\n");
                    delete event;
                }
            }
        }
    }
    return B_OK;
}