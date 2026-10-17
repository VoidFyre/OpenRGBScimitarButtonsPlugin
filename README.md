# OpenRGB Scimitar Buttons Plugin

An [OpenRGB](https://openrgb.org) plugin that keeps the 12 side buttons of the
**Corsair Scimitar RGB Elite** working while OpenRGB controls the mouse's
lighting, and lets you map each button to any key or key combination.

## The problem

To set the mouse's colours, OpenRGB switches it into Corsair's "software mode".
In that mode the firmware stops sending the side buttons' built-in keys
(`1` – `0`, `-`, `=`). On Windows, iCUE reads the buttons instead; on Linux
nothing does, so the buttons go silent until the mouse is replugged. See
[OpenRGB issue #580](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/580).

## What the plugin does

1. After OpenRGB sets the mouse up (at startup, on rescans and on hotplug), it
   sends the mouse a key input table: the main buttons stay normal HID input and
   the 12 side buttons are reported as Corsair input reports.
2. It reads those reports and types the mapped keys through a virtual keyboard
   (`uinput`), so they work in every application, X11 or Wayland.
3. When OpenRGB closes, it hands the buttons back to the mouse's own hardware
   keys (can be turned off), so they keep working without OpenRGB.

Lighting, effects and DPI are untouched. The plugin only talks to mice that
OpenRGB itself is controlling; if you disable the mouse in OpenRGB, the plugin
leaves it alone.

## The Scimitar Buttons tab

The tab shows the side buttons in the same 3 × 4 layout as the mouse. Each tile
lights up while you hold the matching button. Click a tile's key, then press
the key or combination you want (for example `F5` or `Ctrl + C`); `Esc`
cancels. **Clear** disables a button, **Reset to 1 - =** restores the defaults.
Mappings are saved in OpenRGB's settings.

## Requirements

- Linux
- OpenRGB 1.0 (plugin API version 5)
- A Corsair Scimitar RGB Elite (wired, USB ID `1b1c:1b8b`) enabled in OpenRGB
- Write access to `/dev/uinput`. The package installs a udev rule that grants it
  to the logged-in user and loads the `uinput` module at boot. Steam, Sunshine
  and similar tools often grant it already.

Other Corsair mice with macro buttons probably need different button IDs and
are not supported yet. Reports from other models are welcome.

## Building

```sh
git clone --recurse-submodules https://github.com/VoidFyre/OpenRGBScimitarButtonsPlugin.git
cd OpenRGBScimitarButtonsPlugin
mkdir build && cd build
qmake6 ../OpenRGBScimitarButtonsPlugin.pro PREFIX=/usr
make
```

Build dependencies: Qt 6 (`qt6-base`), `make`, a C++17 compiler and the Linux
kernel headers. The `OpenRGB` submodule (tag `release_1.0`) only provides the
plugin headers.

### Installing

System-wide (plugin, udev rule and module-load file):

```sh
sudo make install
sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=misc
sudo modprobe uinput
```

Or just for your user, if you already have `/dev/uinput` access:

```sh
install -Dm755 libOpenRGBScimitarButtonsPlugin.so ~/.config/OpenRGB/plugins/libOpenRGBScimitarButtonsPlugin.so
```

Close OpenRGB before installing or updating. Overwriting the plugin file in
place while OpenRGB has it loaded crashes OpenRGB.

Arch Linux users can build a package with `packaging/arch/PKGBUILD`.

Restart OpenRGB afterwards; the **Scimitar Buttons** tab appears next to the
other plugin tabs.

## Troubleshooting

- **The tab says it cannot create the virtual keyboard:** you cannot write to
  `/dev/uinput`. Install the udev rule above, then log out and back in.
- **The tab says no Scimitar was found:** the mouse is disabled in OpenRGB (its
  buttons then type their built-in keys anyway) or it is not a wired Scimitar
  RGB Elite.
- **Buttons type twice:** another remapper (such as a `scimitar-keys` service
  or ckb-next) is also reading the mouse. Stop it.

## How it works

The Scimitar RGB Elite uses Corsair's older "NXP" protocol on HID interface 1:

| Command | Meaning |
| --- | --- |
| `07 04 02` / `07 04 01` | Special functions (side buttons, DPI) in software / hardware mode |
| `07 05 02 00 01` | Lighting in software mode |
| `07 40 <n> 00` + `<id> <mode>` pairs | Key input mode per button: `0x80` HID, `0x40` Corsair report |

Button IDs 1 – 8 are the main buttons, 9 – 20 the side buttons. Side button *N*
then shows up at bit *N − 1* of bytes 2 – 3 of report `0x03` on HID interface 0.
Thanks to [ckb-next](https://github.com/ckb-next/ckb-next) for documenting the
protocol.

## License

GPL-2.0-or-later, like OpenRGB. See [LICENSE](LICENSE).
