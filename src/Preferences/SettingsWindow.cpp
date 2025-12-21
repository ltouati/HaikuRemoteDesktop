/*
 * SettingsWindow.cpp
 * Configuration UI
 */
#include "SettingsWindow.h"
#include <GroupLayout.h>
#include <GroupView.h>
#include "SettingsWindow.h"
#include "Settings.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <ScrollView.h>
#include <Alert.h>
#include <stdio.h>



SettingsWindow::SettingsWindow(Settings* settings)
    : BWindow(BRect(100, 100, 600, 500), "Settings", B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
      fSettings(settings) {

    // Port Input
    BString portStr;
    portStr << fSettings->Port();
    fPortInput = new BTextControl("Port:", portStr.String(), NULL);
    
    // Certificate View
    fCertView = new BTextView("CertView");
    fCertView->MakeEditable(true);
    fCertView->SetText(fSettings->ReadContent(fSettings->SSLCertPath()).String());
    
    BScrollView* certScroll = new BScrollView("CertScroll", fCertView, 0, false, true);
    certScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 100));

    // Key View
    fKeyView = new BTextView("KeyView");
    fKeyView->MakeEditable(true);
    fKeyView->SetText(fSettings->ReadContent(fSettings->SSLKeyPath()).String());
    
    BScrollView* keyScroll = new BScrollView("KeyScroll", fKeyView, 0, false, true);
    keyScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 100));

    // Buttons
    fGenerateButton = new BButton("Generate Keys", new BMessage(MSG_GENERATE_CERTS));
    fApplyButton = new BButton("Apply", new BMessage(MSG_APPLY_SETTINGS));

    BLayoutBuilder::Group<>(this, B_VERTICAL, 10)
        .SetInsets(B_USE_WINDOW_INSETS)
        .Add(fPortInput)
        .Add(new BStringView("Label", "SSL Certificate:"))
        .Add(certScroll)
        .Add(new BStringView("Label", "SSL Private Key:"))
        .Add(keyScroll)
        .AddGroup(B_HORIZONTAL)
            .Add(fGenerateButton)
            .AddGlue()
            .Add(fApplyButton)
        .End();
        
    CenterOnScreen();
}

SettingsWindow::~SettingsWindow() {
}

void SettingsWindow::MessageReceived(BMessage* msg) {
    switch (msg->what) {
        case MSG_GENERATE_CERTS: {
            BAlert* alert = new BAlert("Confirm", "Are you sure you want to regenerate the certificates? Any manual changes will be lost.", "Cancel", "Regenerate");
            int32 result = alert->Go();
            if (result == 1) {
                if (fSettings->GenerateCertificates() == B_OK) {
                   fCertView->SetText(fSettings->ReadContent(fSettings->SSLCertPath()).String());
                   fKeyView->SetText(fSettings->ReadContent(fSettings->SSLKeyPath()).String());
                } else {
                   (new BAlert("Error", "Failed to generate certificates.", "OK"))->Go();
                }
            }
            break;
        }
        case MSG_APPLY_SETTINGS: {
            uint16 port = atoi(fPortInput->Text());
            if (port > 0) fSettings->SetPort(port);
            
            // Write content back to files
            fSettings->WriteContent(fSettings->SSLCertPath(), fCertView->Text());
            fSettings->WriteContent(fSettings->SSLKeyPath(), fKeyView->Text());
            
            if (fSettings->Save() == B_OK) {
                // Notify Server App to Restart Server Logic
                BMessenger messenger("application/x-vnd.Haiku-ScreenCaster");
                if (messenger.IsValid()) {
                    BMessage updateMsg('stch'); // Settings Changed
                    messenger.SendMessage(&updateMsg);
                }
            } else {
                fprintf(stderr, "Failed to save settings\n");
            }
            break;
        }
        default:
            BWindow::MessageReceived(msg);
    }
}

bool SettingsWindow::QuitRequested() {
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}
