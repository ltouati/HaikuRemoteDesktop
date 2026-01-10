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
#include <Path.h>
#include <Entry.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <CheckBox.h>
#include <stdio.h>



SettingsWindow::SettingsWindow(Settings* settings)
    : BWindow(BRect(100, 100, 600, 500), "Settings", B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
      fSettings(settings) {

    // Port Input
    BString portStr;
    portStr << fSettings->Port();
    fPortInput = new BTextControl("Port:", portStr.String(), NULL);
    
    // Start on Boot Checkbox
    fStartOnBoot = new BCheckBox("StartOnBoot", "Start automatically on boot", NULL);
    
    // Check if symlink exists
    BPath bootPath;
    find_directory(B_USER_BOOT_DIRECTORY, &bootPath);
    bootPath.Append("launch/HaikuRemoteDesktop");
    BEntry bootEntry(bootPath.Path());
    if (bootEntry.Exists()) {
        fStartOnBoot->SetValue(B_CONTROL_ON);
    }

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
        .Add(fStartOnBoot)
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
            
            // Handle Start on Boot
            BPath bootPath;
            if (find_directory(B_USER_BOOT_DIRECTORY, &bootPath) == B_OK) {
                 bootPath.Append("launch");
                 create_directory(bootPath.Path(), 0777); // Ensure directory exists
                 bootPath.Append("HaikuRemoteDesktop");
                 
                 BEntry bootEntry(bootPath.Path());
                 
                 if (fStartOnBoot->Value() == B_CONTROL_ON) {
                     if (!bootEntry.Exists()) {
                         // Create Symlink
                         BPath appPath;
                         // Assuming standard install location: /system/apps/HaikuRemoteDesktop/HaikuRemoteDesktop
                         // Or try to find via be_app? But we are the preflet.
                         // Standard package install location is reliable.
                         BDirectory bootDir;
                         BPath launchDir;
                         find_directory(B_USER_BOOT_DIRECTORY, &launchDir);
                         launchDir.Append("launch");
                         bootDir.SetTo(launchDir.Path());
                         
                         bootDir.CreateSymLink("HaikuRemoteDesktop", "/system/apps/HaikuRemoteDesktop/HaikuRemoteDesktop", NULL);
                     }
                 } else {
                     if (bootEntry.Exists()) {
                         bootEntry.Remove();
                     }
                 }
            }
            
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
