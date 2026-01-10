/*
 * SettingsWindow.h
 * Configuration UI
 */
#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <Window.h>
#include <TextControl.h>
#include <TextView.h>
#include <CheckBox.h>
#include <Button.h>

class Settings;

class SettingsWindow : public BWindow {
public:
    SettingsWindow(Settings* settings);
    virtual ~SettingsWindow();

    virtual void MessageReceived(BMessage* msg);
    virtual bool QuitRequested();

private:
    Settings* fSettings;
    BTextControl* fPortInput;
    BCheckBox* fStartOnBoot;
    BTextView* fCertView;
    BTextView* fKeyView;
    BButton* fApplyButton;
    BButton* fGenerateButton;
};

#define MSG_APPLY_SETTINGS 'apsG'
#define MSG_GENERATE_CERTS 'gnCr'

#endif // SETTINGS_WINDOW_H
