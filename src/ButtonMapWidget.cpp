/*---------------------------------------------------------*\
| ButtonMapWidget.cpp                                       |
|                                                           |
|   Plugin tab: map each side button to a key combination   |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <algorithm>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>
#include "ButtonMapWidget.h"
#include "KeyNames.h"
#include "ScimitarButtonsPlugin.h"

/*---------------------------------------------------------*\
| Qt reports XKB keycodes, which are evdev codes + 8        |
\*---------------------------------------------------------*/
#define XKB_EVDEV_OFFSET    8

/*---------------------------------------------------------*\
| The side panel is 3 columns by 4 rows, numbered 1-12      |
| left to right, top to bottom                              |
\*---------------------------------------------------------*/
#define SIDE_PANEL_COLUMNS  3

/*---------------------------------------------------------*\
| KeyCaptureButton                                          |
\*---------------------------------------------------------*/
KeyCaptureButton::KeyCaptureButton(QWidget* parent) : QPushButton(parent)
{
    keys_down   = 0;
    capturing   = false;

    setMinimumWidth(140);
    setToolTip("Click, then press a key or key combination. Esc cancels.");

    connect(this, &QPushButton::clicked, this, &KeyCaptureButton::StartCapture);
}

void KeyCaptureButton::SetCombo(const KeyCombo& new_combo)
{
    combo = new_combo;

    if(!capturing)
    {
        setText(combo.empty() ? "(none)" : QString::fromStdString(KeyComboToString(combo)));
    }
}

void KeyCaptureButton::StartCapture()
{
    if(capturing)
    {
        return;
    }

    capturing   = true;
    keys_down   = 0;
    captured.clear();

    setText("Press keys...");
    setFocus(Qt::OtherFocusReason);
    grabKeyboard();
}

void KeyCaptureButton::StopCapture()
{
    if(!capturing)
    {
        return;
    }

    capturing = false;
    releaseKeyboard();
    SetCombo(combo);
}

bool KeyCaptureButton::event(QEvent* event)
{
    /*-----------------------------------------------------*\
    | Let Tab and Shift+Tab be captured instead of moving   |
    | the focus                                             |
    \*-----------------------------------------------------*/
    if(capturing && event->type() == QEvent::KeyPress)
    {
        keyPressEvent(static_cast<QKeyEvent*>(event));
        return(true);
    }

    return(QPushButton::event(event));
}

void KeyCaptureButton::keyPressEvent(QKeyEvent* event)
{
    if(!capturing)
    {
        QPushButton::keyPressEvent(event);
        return;
    }

    if(event->isAutoRepeat())
    {
        return;
    }

    if(event->key() == Qt::Key_Escape && captured.empty())
    {
        StopCapture();
        return;
    }

    int code = (int)event->nativeScanCode() - XKB_EVDEV_OFFSET;

    if(code <= 0 || code > KEY_NAMES_MAX_CODE)
    {
        return;
    }

    if(std::find(captured.begin(), captured.end(), (unsigned short)code) == captured.end())
    {
        captured.push_back((unsigned short)code);
        keys_down++;
        setText(QString::fromStdString(KeyComboToString(captured)) + " ...");
    }
}

void KeyCaptureButton::keyReleaseEvent(QKeyEvent* event)
{
    if(!capturing)
    {
        QPushButton::keyReleaseEvent(event);
        return;
    }

    if(event->isAutoRepeat() || captured.empty())
    {
        return;
    }

    /*-----------------------------------------------------*\
    | The combination is complete once every key is up      |
    \*-----------------------------------------------------*/
    if(--keys_down <= 0)
    {
        KeyCombo result = captured;

        combo = result;
        StopCapture();
        emit ComboCaptured(result);
    }
}

void KeyCaptureButton::focusOutEvent(QFocusEvent* event)
{
    StopCapture();
    QPushButton::focusOutEvent(event);
}

/*---------------------------------------------------------*\
| ButtonTile                                                |
\*---------------------------------------------------------*/
ButtonTile::ButtonTile(unsigned int idx, QWidget* parent) : QFrame(parent)
{
    button_idx = idx;

    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);

    QLabel* number_label = new QLabel(QString::number(button_idx + 1));
    QFont   number_font  = number_label->font();

    number_font.setPointSizeF(number_font.pointSizeF() * 1.6);
    number_font.setBold(true);
    number_label->setFont(number_font);
    number_label->setMinimumWidth(32);
    number_label->setAlignment(Qt::AlignCenter);

    capture_button = new KeyCaptureButton();

    QPushButton* clear_button = new QPushButton("Clear");
    clear_button->setToolTip("Make this button do nothing");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(number_label);
    layout->addWidget(capture_button, 1);
    layout->addWidget(clear_button);

    connect(capture_button, &KeyCaptureButton::ComboCaptured, this, [this](const KeyCombo& combo)
    {
        emit ComboChanged(button_idx, combo);
    });

    connect(clear_button, &QPushButton::clicked, this, [this]()
    {
        capture_button->SetCombo(KeyCombo());
        emit ComboChanged(button_idx, KeyCombo());
    });
}

void ButtonTile::SetCombo(const KeyCombo& combo)
{
    capture_button->SetCombo(combo);
}

void ButtonTile::SetHeld(bool held)
{
    setBackgroundRole(held ? QPalette::Highlight : QPalette::Window);
    setForegroundRole(held ? QPalette::HighlightedText : QPalette::WindowText);
}

/*---------------------------------------------------------*\
| ButtonMapWidget                                           |
\*---------------------------------------------------------*/
ButtonMapWidget::ButtonMapWidget(ScimitarButtonsPlugin* plugin_ptr, QWidget* parent) : QWidget(parent)
{
    plugin = plugin_ptr;

    QLabel* title_label = new QLabel("Corsair Scimitar side buttons");
    QFont   title_font  = title_label->font();

    title_font.setPointSizeF(title_font.pointSizeF() * 1.3);
    title_font.setBold(true);
    title_label->setFont(title_font);

    status_label = new QLabel();
    status_label->setWordWrap(true);

    QLabel* hint_label = new QLabel("Click a button's key to change it, then press the key or key combination you want. "
                                    "Esc cancels. Tiles light up while you hold the matching button on the mouse.");
    hint_label->setWordWrap(true);

    QGridLayout* grid = new QGridLayout();

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT; button_idx++)
    {
        tiles[button_idx] = new ButtonTile(button_idx);
        grid->addWidget(tiles[button_idx], button_idx / SIDE_PANEL_COLUMNS, button_idx % SIDE_PANEL_COLUMNS);

        connect(tiles[button_idx], &ButtonTile::ComboChanged, this, &ButtonMapWidget::OnComboChanged);
    }

    QPushButton* reset_button = new QPushButton("Reset to 1 - =");
    connect(reset_button, &QPushButton::clicked, this, &ButtonMapWidget::OnResetClicked);

    restore_checkbox = new QCheckBox("Hand the buttons back to the mouse when OpenRGB closes");
    restore_checkbox->setToolTip("Sends the mouse back to its built-in 1 - = keys on exit. "
                                 "Otherwise the side buttons stay silent until the mouse is replugged.");
    restore_checkbox->setChecked(plugin->GetRestoreOnExit());
    connect(restore_checkbox, &QCheckBox::toggled, this, [this](bool checked)
    {
        plugin->SetRestoreOnExit(checked);
    });

    QHBoxLayout* bottom_row = new QHBoxLayout();
    bottom_row->addWidget(reset_button);
    bottom_row->addSpacing(16);
    bottom_row->addWidget(restore_checkbox);
    bottom_row->addStretch(1);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(title_label);
    layout->addWidget(status_label);
    layout->addSpacing(8);
    layout->addWidget(hint_label);
    layout->addLayout(grid);
    layout->addLayout(bottom_row);
    layout->addStretch(1);

    RefreshTiles();
    UpdateStatus();
}

void ButtonMapWidget::RefreshTiles()
{
    ButtonMap map = plugin->GetButtonMap();

    for(unsigned int button_idx = 0; button_idx < SIDE_BUTTON_COUNT; button_idx++)
    {
        tiles[button_idx]->SetCombo(map[button_idx]);
    }
}

void ButtonMapWidget::OnComboChanged(unsigned int button_idx, const KeyCombo& combo)
{
    ButtonMap map = plugin->GetButtonMap();

    map[button_idx] = combo;
    plugin->SetButtonMap(map);
}

void ButtonMapWidget::OnResetClicked()
{
    plugin->SetButtonMap(DefaultButtonMap());
    RefreshTiles();
}

void ButtonMapWidget::UpdateStatus()
{
    SideButtonStatus status = plugin->GetStatus();
    QString          text;

    if(!status.keyboard_ok)
    {
        text = "Cannot create the virtual keyboard, so the buttons cannot type. "
               "Check that you can write to /dev/uinput (see the plugin's README).";
    }
    else if(!status.mouse_found)
    {
        text = "No Scimitar RGB Elite found in OpenRGB. If the mouse is not under OpenRGB's control, "
               "its side buttons already type their built-in keys.";
    }
    else if(!status.reading)
    {
        text = "Found the mouse, but cannot read its buttons.";
    }
    else
    {
        text = "Active: the side buttons type the keys below.";
    }

    if(!status.error.empty())
    {
        text += "\n" + QString::fromStdString(status.error);
    }

    status_label->setText(text);
}

void ButtonMapWidget::ShowButtonState(unsigned int button_idx, bool pressed)
{
    if(button_idx < SIDE_BUTTON_COUNT)
    {
        tiles[button_idx]->SetHeld(pressed);
    }
}
