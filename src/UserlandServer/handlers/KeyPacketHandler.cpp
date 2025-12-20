/*
 * KeyPacketHandler.cpp
 */
#include "KeyPacketHandler.h"
#include <OS.h>
#include <InterfaceDefs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> // for system()

#include <Messenger.h>
#include <Message.h>

// static const char* kInputServerSignature = ...;

KeyPacketHandler::KeyPacketHandler() {
    _InitKeyMap();
}

void
KeyPacketHandler::Handle(NetworkServer *server, NetworkServer::ClientState *client,
                         const haiku::remote::InputEvent &event) {
    if (!event.has_key()) return;

    const haiku::remote::KeyEvent &key = event.key();

    input_packet driverEvent;
    memset(&driverEvent, 0, sizeof(driverEvent));

    driverEvent.type = PACKET_KEY;
    driverEvent.data.key.down = key.down();
    driverEvent.data.key.modifiers = key.modifiers();
    driverEvent.data.key.key_utf32 = key.key_utf32();

    // Use Corrected Map
    uint32_t keyCode = key.key_code();
    uint32_t charCode = key.key_utf32();

    if (keyCode == 0 && !key.key_string().empty()) {
        std::string code = key.key_string();
        if (fKeyMap.find(code) != fKeyMap.end()) {
            KeyInfo info = fKeyMap[code];
            keyCode = info.scancode;
            if (charCode == 0) charCode = info.charcode;
        } else {
            // Unknown Key
        }
    }

    driverEvent.data.key.key_code = keyCode;
    driverEvent.data.key.key_utf32 = charCode;


    port_id inputPort = server->GetInputPort();
    if (inputPort >= 0) write_port(inputPort, 0, &driverEvent, sizeof(driverEvent));
}

void
KeyPacketHandler::_InitKeyMap() {
    // Common Keys


    // F-Keys
    fKeyMap["F1"] = {0x02, 0};
    fKeyMap["F2"] = {0x03, 0};
    fKeyMap["F3"] = {0x04, 0};
    fKeyMap["F4"] = {0x05, 0};
    fKeyMap["F5"] = {0x06, 0};
    fKeyMap["F6"] = {0x07, 0};
    fKeyMap["F7"] = {0x08, 0};
    fKeyMap["F8"] = {0x09, 0};
    fKeyMap["F9"] = {0x0A, 0};
    fKeyMap["F10"] = {0x0B, 0};
    fKeyMap["F11"] = {0x0C, 0};
    fKeyMap["F12"] = {0x0D, 0};

    // Modifiers
    // Modifiers
    fKeyMap["ShiftLeft"] = {0x4B, 0};
    fKeyMap["ShiftRight"] = {0x56, 0};
    fKeyMap["ControlLeft"] = {0x5C, 0};
    fKeyMap["ControlRight"] = {0x60, 0};
    fKeyMap["AltLeft"] = {0x5D, 0};
    fKeyMap["AltRight"] = {0x5F, 0};
    fKeyMap["MetaLeft"] = {0x66, 0};
    fKeyMap["MetaRight"] = {0x67, 0};

    // Numbers Row
    // Tilde=0x11, 1=0x12 ... 0=0x1b, -=0x1c, ==0x1d, Backspace=0x1e
    fKeyMap["Backquote"] = {0x11, '`'};
    fKeyMap["Key1"] = {0x12, '1'};
    fKeyMap["Key2"] = {0x13, '2'};
    fKeyMap["Key3"] = {0x14, '3'};
    fKeyMap["Key4"] = {0x15, '4'};
    fKeyMap["Key5"] = {0x16, '5'};
    fKeyMap["Key6"] = {0x17, '6'};
    fKeyMap["Key7"] = {0x18, '7'};
    fKeyMap["Key8"] = {0x19, '8'};
    fKeyMap["Key9"] = {0x1a, '9'};
    fKeyMap["Key0"] = {0x1b, '0'};
    fKeyMap["Minus"] = {0x1c, '-'};
    fKeyMap["Equal"] = {0x1d, '='};
    fKeyMap["Backspace"] = {0x1e, 0x08};

    // QWERTY Row 1
    // Tab=0x26, Q=0x27 ... P=0x30, [=0x31, ]=0x32, \=0x33
    fKeyMap["Tab"] = {0x26, 0x09};
    fKeyMap["KeyQ"] = {0x27, 'q'};
    fKeyMap["KeyW"] = {0x28, 'w'};
    fKeyMap["KeyE"] = {0x29, 'e'};
    fKeyMap["KeyR"] = {0x2a, 'r'};
    fKeyMap["KeyT"] = {0x2b, 't'};
    fKeyMap["KeyY"] = {0x2c, 'y'};
    fKeyMap["KeyU"] = {0x2d, 'u'};
    fKeyMap["KeyI"] = {0x2e, 'i'};
    fKeyMap["KeyO"] = {0x2f, 'o'};
    fKeyMap["KeyP"] = {0x30, 'p'};
    fKeyMap["BracketLeft"] = {0x31, '['};
    fKeyMap["BracketRight"] = {0x32, ']'};
    fKeyMap["Backslash"] = {0x33, '\\'};

    // QWERTY Row 2
    // Caps=0x3b, A=0x3c ... L=0x44, ;=0x45, '=0x46, Enter=0x47
    fKeyMap["CapsLock"] = {0x3b, 0};
    fKeyMap["KeyA"] = {0x3c, 'a'};
    fKeyMap["KeyS"] = {0x3d, 's'};
    fKeyMap["KeyD"] = {0x3e, 'd'};
    fKeyMap["KeyF"] = {0x3f, 'f'};
    fKeyMap["KeyG"] = {0x40, 'g'};
    fKeyMap["KeyH"] = {0x41, 'h'};
    fKeyMap["KeyJ"] = {0x42, 'j'};
    fKeyMap["KeyK"] = {0x43, 'k'};
    fKeyMap["KeyL"] = {0x44, 'l'};
    fKeyMap["Semicolon"] = {0x45, ';'};
    fKeyMap["Quote"] = {0x46, '\''};
    fKeyMap["Enter"] = {0x47, 0x0a};

    // QWERTY Row 3
    // ShiftL=0x4b, Z=0x4c ... M=0x52, ,=0x53, .=0x54, /=0x55, ShiftR=0x56
    fKeyMap["KeyZ"] = {0x4c, 'z'};
    fKeyMap["KeyX"] = {0x4d, 'x'};
    fKeyMap["KeyC"] = {0x4e, 'c'};
    fKeyMap["KeyV"] = {0x4f, 'v'};
    fKeyMap["KeyB"] = {0x50, 'b'};
    fKeyMap["KeyN"] = {0x51, 'n'};
    fKeyMap["KeyM"] = {0x52, 'm'};
    fKeyMap["Comma"] = {0x53, ','};
    fKeyMap["Period"] = {0x54, '.'};
    fKeyMap["Slash"] = {0x55, '/'};

    // Bottom Row
    fKeyMap["Space"] = {0x5e, 0x20};

    // Arrows
    fKeyMap["ArrowLeft"] = {0x61, 0x1c};
    fKeyMap["ArrowDown"] = {0x62, 0x1f};
    fKeyMap["ArrowRight"] = {0x63, 0x1d};
    fKeyMap["ArrowUp"] = {0x57, 0x1e};

    // Navigation
    fKeyMap["Insert"] = {0x1F, 0x05}; // Check 0x1f collision? 0x1f is usually Key 6 on numpad or similar?
    // BeBook says:
    // Insert=0x1f, Home=0x20, PgUp=0x21, Delete=0x34, End=0x35, PgDn=0x36
    // ArrowUp=0x57, ArrowLeft=0x61, ArrowDown=0x62, ArrowRight=0x63
    fKeyMap["Insert"] = {0x1F, 0x05};
    fKeyMap["Delete"] = {0x34, 0x7f};
    fKeyMap["Home"] = {0x20, 0x01};
    fKeyMap["End"] = {0x35, 0x04};
    fKeyMap["PageUp"] = {0x21, 0x0b};
    fKeyMap["PageDown"] = {0x36, 0x0c};
}