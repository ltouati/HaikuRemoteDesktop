/*
 * RemoteDesktopPreflet.cpp
 * Preferences Application Entry Point
 */
#include <Application.h>
#include <Catalog.h>

#include "Settings.h"
#include "SettingsWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "RemoteDesktopPreflet"

class RemoteDesktopPreflet : public BApplication {
public:
    RemoteDesktopPreflet()
        : BApplication("application/x-vnd.Haiku-RemoteDesktopPreflet") {
    }

    virtual void ReadyToRun() {
        fSettings.Load();
        fWindow = new SettingsWindow(&fSettings);
        fWindow->Show();
    }
    
    virtual void MessageReceived(BMessage* msg) {
        BApplication::MessageReceived(msg);
    }

private:
    Settings fSettings;
    SettingsWindow* fWindow;
};

int main() {
    RemoteDesktopPreflet app;
    app.Run();
    return 0;
}
