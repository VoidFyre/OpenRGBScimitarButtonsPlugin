/*---------------------------------------------------------*\
| ButtonMapWidget.h                                         |
|                                                           |
|   Plugin tab: map each side button to a key combination   |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include "ButtonMap.h"

class ScimitarButtonsPlugin;

/*---------------------------------------------------------*\
| Button that records the next key combination pressed      |
| while it has focus. Esc on its own cancels.               |
\*---------------------------------------------------------*/
class KeyCaptureButton : public QPushButton
{
    Q_OBJECT

public:
    explicit KeyCaptureButton(QWidget* parent = nullptr);

    void                SetCombo(const KeyCombo& combo);

signals:
    void                ComboCaptured(const KeyCombo& combo);

protected:
    bool                event(QEvent* event) override;
    void                keyPressEvent(QKeyEvent* event) override;
    void                keyReleaseEvent(QKeyEvent* event) override;
    void                focusOutEvent(QFocusEvent* event) override;

private:
    void                StartCapture();
    void                StopCapture();

    KeyCombo            combo;
    KeyCombo            captured;
    int                 keys_down;
    bool                capturing;
};

class ButtonTile : public QFrame
{
    Q_OBJECT

public:
    ButtonTile(unsigned int button_idx, QWidget* parent = nullptr);

    void                SetCombo(const KeyCombo& combo);
    void                SetHeld(bool held);

signals:
    void                ComboChanged(unsigned int button_idx, const KeyCombo& combo);

private:
    unsigned int        button_idx;
    KeyCaptureButton*   capture_button;
};

class ButtonMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ButtonMapWidget(ScimitarButtonsPlugin* plugin, QWidget* parent = nullptr);

    void                UpdateStatus();
    void                ShowButtonState(unsigned int button_idx, bool pressed);

private:
    void                OnComboChanged(unsigned int button_idx, const KeyCombo& combo);
    void                OnResetClicked();
    void                RefreshTiles();

    ScimitarButtonsPlugin*  plugin;
    ButtonTile*             tiles[SIDE_BUTTON_COUNT];
    QLabel*                 status_label;
    QCheckBox*              restore_checkbox;
};
