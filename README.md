# What is this?

CloudpilotEmu is a web-based emulator for PalmOS. It emulates both Dragonball
based devices (OS 1-4) and the Palm Tungsten E2 (ARM-based, OS5). Emulation
of Dragonball devices is based on the original POSE emulator, and OS5
emulation is based on [uARM](https://github.com/uARM-Palm/uARM).

<img src="doc/images/m515.jpeg" width="310" height="671" alt="Palm m515 emulation"></img>
&nbsp;
<img src="doc/images/v.jpeg" width="310" height="671" alt="Palm Vx emulation"></img>
&nbsp;
<img src="doc/images/t675c.jpeg" width="310" height="671" alt="Tungsten W emulation"></img>
&nbsp;
<img src="doc/images/e2.jpeg" width="310" height="671" alt="Palm Pilot emulation"></img>

# The emulator

The current version of the emulator can be found at

https://cloudpilot-emu.github.io/app

The emulator can be added as an app to the homescreen of iOS and Android devices.
Please check out the documentation in the app. A native app for iOS is available
in the [Appstore](https://apps.apple.com/de/app/cloudpilotemu/id6478502699).

## Self hosting

Starting with version 1.10.12, the release files on
[https://github.com/cloudpilot-emu/cloudpilot-emu/releases](Github)
include the prebuilt web app for self hosting.

CloudpilotEmu is a purely client side application, so you can simply serve
the unzipped files with a web server of your choice. However, some parts
of CloudpilotEmu (notably PCM audio) will only work if the app is served
via HTTPS.

## Embedded emulator

The is an embedded version of CloudpilotEmu that allows you to embed the emulator
into your own web pages. Please check the
[documentation](https://cloudpilot-emu.github.io/embedded/)
for more details.

## ROM files

In order to use the emulator you need to provide a ROM file for the device you
want to emulate. You can find a collection of ROMs
[on PalmDB](https://palmdb.net/app/palm-roms-complete).

## Networking support

CloudpilotEmu supports networking by tunneling network connections over a
custom proxy server via a websocket connection. Please check out the
[dedicated networking documentation](doc/networking.md) for setting up
networking with CloudpilotEmu.

## Emulated devices

The following devices are currently emulated:

- Pilot, Palm Pilot
- Palm III
- Palm IIIxe, Palm IIIx, Palm IIIe
- Palm IIIc
- Palm Vx, Palm V
- Palm VII, Palm VII EZ, Palm VIIx
- Palm m500, Palm m505, Palm m515
- Palm m520 (an unreleased prototype of a highres 5xx)
- Palm m100, Palm m105, Palm m125, Palm m130
- Palm i705
- Tungsten W (silkscreen version)
- Tungsten E2, E3 (a widescreen version of the E2 created specifically for emulation by
  Dmitry Grinberg)
- rePalm paravirualized (a paravirtualized version of PalmOS created by Dmitry Grinberg
  that offers substantial performance improvements over emulated read hardware)
- Tungsten C, Tungsten T3 (native `cp-uarm` app only, see [below](#os5))
- Handera 330, 330c (the lost color version of the 330)
- Sony PEG-S300, PEG-S320
- Sony PEG-S500C series
- Sony PEG-T400 series
- Sony PEG-N600C series
- Sony PEG-T600C series
- Sony PEG-N700C series
- Sony PEG-T650C series
- Sony PEG-NR70 series
- Acer S1x
- Legend P168 (no SD card support)

## Other versions

A preview build of the next version of CloudpilotEmu is available
[here](https://cloudpilot-emu.github.io/app-preview).

# Notes on emulation of OS4 and earlier

## Relationship to POSE

CloudpilotEmu is derived from the original POSE sources. It is not a
straightforward port, though, as the original code has been adapted and partly
rewritten to fit well with a browser environment. In addition, it contains
bug fixes and many new features. In particular:

- The codebase has been updated to build and run on both 32bit and 64bit
  little endian systems. Big endian systems should work, but I cannot test
  this (are there even any relevant big endian systems left?).
- POSE was built as a tool for debugging and profiling PalmOS applications.
  Most of this functionality has been removed in CloudpilotEmu.
- All UI parts and in particular threading were removed from the source.
- Dispatch and timing have been partly rewritten, timing should now be pretty
  close to the original device.
- Adjusting the grayscale in 2bpp mode works.
- All 16MB of the Palm m515 are usable.
- A few conditions that can cause interrupt storms have been fixed.
- Savestate code has been rewritten to work with a fixed buffer without
  allocations.
- Event injection works without generating null events.
- Reminders and alarms trigger propely.
- Clié devices use full MQ11xx video acceleration.
- SD card and Memory Stick emulation.
- A GDB stub for debugging PalmOS and apps written with a suitable toolchain.

# Notes on OS5 emulation

## Relationship to uARM

OS5 emulation is based on Dmitry Grinberg's [uARM](https://github.com/uARM-Palm/uARM).
emulator. However, CloudpilotEmu is not a straightforward port, but a fork that trails behind
Dmitry's original sources and adds the following of features:

- Optimizations that give a significant performance boost at the expense of RAM and
  portability.
- PACE replaced with direct m68k emulation on the host, using the same m68k core as POSE.
- Accurate timing.
- Audio emulation.
- Savestates.
- Power off (which crashes the emulator) is disabled.
- Database installation and export.

## Performance

OS5 was designed to run on ARM CPUs clocked at several 100 MHz. Those chips were
fast enough to play music, decode video and run demanding games and even
emulators.

CloudpilotEmu pulls many tricks and optimizations to run OS5 as fast as
possible, and applications targeting OS4 or earlier will run at comparable
speeds on OS5, but native OS5 software requires a fast host. Apple Silicon Macs
and recent iOS devices (iPhone 13 or later) run even demanding ARM apps at
decent or even faster-than-real speeds, and the same goes for fast x86 desktops,
but older and slower devices (which includes most Android devices) may struggle.
Many apps still work fine, but your mileage may vary.

## Paravirtualization

CloudpilotEmu supports a paravirtualized version of rePalm created by Dmitry
Grinberg. Instead of real hardware, this platform implements a minimal interface
between host and guest and runs about 40% fastern than an emulated E2.

In addition, rePalm paravirtualized supports a range of resolutions and
RAM sizes that you can choose from when creating a session.

You can download a suitable ROM from the
[CloudpilotEmu website](https://cloudpilot-emu.github.io)
and from [PalmDB](https://palmdb.net/app/palm-roms-complete).

# Native app

## OS4 and earlier

In addition to the web version, CloudpilotEmu can be built as a native app.
The native version targets developers and does not provide skins or audio
support. However, it adds a GDB stub that allows for remote debugging via
GDB. You can find more documentation on debugging with CloudpilotEmu
[here](doc/debugging.md).

The native app does not come with a GUI, but is controlled with a CLI
instead. The CLI provides commands for loading and saving images, mounting
card images, generating traces of m68k code and much more. The CLI supports
tab completion, and there is a `help` command that provides further
documentation.

Networking is supported in the native app both natively and via the proxy.
For proxy support, the app needs to be built against libcurl 8.0.0 or newer,
and libcurl needs to be configured with websocket support.

The native app can be built and run on MacOS, Linux and on the WSL for
Windows. Please see below for build instructions. Once built, the app can
be started from the command line by supplying a session image or a ROM file
as command line argument. Command line options exist for activating the
GDN stub, mounting a card on launch and more. Run `cloudpilot-emu --help` in
order to get an overview of the supported options.

## OS5

The OS5 emulation part of CloudpilotEmu likewise comes with a native app.
Similar to `cloudpilot-emu`, `cp-uarm` provides a CLI and a set of
command line options, and help is accessible with `--help` on the commandline
and `help` in the CLI.

`cp-uarm` is started with a NOR ROM image or a saved session:

```
    $ src/uarm/cp-uarm <rom or session file>
```

The device is detected from the ROM. In addition to the devices supported by
the web app, `cp-uarm` can run Tungsten C and Tungsten T3 ROMs:

- The Tungsten C is emulated with 64MB of RAM and a 320x320 display without a
  graffiti area. Its hardware keyboard is driven by the host keyboard, see
  [below](#typing-on-the-tungsten-c).
- The Tungsten T3 is emulated with 64MB of RAM and a 320x480 display with the
  dynamic input area.

On both devices touch input is translated directly to screen coordinates, so
digitizer calibration during setup always succeeds. See
[known issues](#known-issues-and-limitations) for the current limitations.

### Keyboard mappings

The emulator window maps these host keys to the device buttons:

| Host key                | Device                                     | Devices                |
| ----------------------- | ------------------------------------------ | ---------------------- |
| F1 - F4                 | Application buttons 1 - 4                  | all                    |
| Cursor keys             | 5-way navigator up / down / left / right   | all                    |
| Return                  | 5-way select (Return types on the TC)      | all except Tungsten C  |
| Keypad Enter            | 5-way select                               | all                    |
| Escape                  | Power button                               | Tungsten E2, E3        |
| F5 or Home              | Home                                       | Tungsten C             |
| F6                      | Menu                                       | Tungsten C             |

F1 - F4 follow the order of the buttons on the real device. On the
Tungsten C, for example, they are Date Book, Address, VersaMail and Web. On
Mac laptops, keypad Enter is fn + Return.

### Typing on the Tungsten C

The Tungsten C has a built-in QWERTY keyboard instead of a graffiti area. In
`cp-uarm` you type on it with the host keyboard. Each character is converted to
the key presses that produce it on the real device, including Shift or the
blue Function key:

- Letters a - z, space and period map to the keys with the same label.
  Capital letters are typed with Shift. Like the real device, the OS
  capitalizes the first letter after a period, `!` or `?` by itself.
- Return, Backspace and Tab map to Return, Backspace and Tab on the device.
  On the Tungsten C, use keypad Enter for 5-way select.
- Digits and most punctuation live on the Function layer of the letter keys:

  | Character | `1` | `2` | `3` | `4` | `5` | `6` | `7` | `8` | `9` | `0` |
  | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | Fn +      | Q   | W   | E   | R   | T   | Y   | U   | I   | O   | P   |

  | Character | `:` | `!` | `)` | `+` | `-` | `*` | `_` | `"` | `'` | `@` | `?` | `,` | `/` | `#` | `$` | `(` |
  | --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
  | Fn +      | A   | B   | C   | D   | F   | G   | H   | J   | K   | L   | M   | N   | S   | V   | Z   | X   |

- The keyboard cannot produce other characters (for example `; = % & < > [ ] { } \ ^ ~ |`)
  or anything outside ASCII, so the emulator ignores them. Enter them with
  the on-screen keyboard instead: press F6 for the menu and choose
  Edit → Keyboard.
- The OS drops key presses that arrive too quickly. Characters are therefore
  played back at about 12 per second (each key change is held for 40 ms of
  emulated time), and fast typing shows up with a short delay.

# Reporting issues

Please report issues on the [Github tracker](https://github.com/cloudpilot-emu/cloudpilot-emu/issues).

# Known issues and limitations

- iOS: changing device orientation between portrait and landscape may mess
  up the layout of the app. This is an iOS bug that can be worked around by
  rotating the device by 180° in portrait and rotating back.
- Audio timing is not perfect and processed at the refresh rate of the emulator
  (usually the same as the screen refresh rate of the host device). Sound
  effects that rely on quickly modulating the audio signal may not be
  reproduced correctly.
- On rare occasions installing files or changing the hotsync name can catch
  PalmOS on the wrong foot and crash the emulator. In particular, this happens
  if a file is installed on a virtual Palm IIIc with PalmOS 3.5 while the
  launcher is active and the category menu is open.
- Tungsten W, i705, PEG-T650C and PEG-NR70 cannot enter and exit sleep correctly,
  and the corresponding functionality has been disabled.
- On Clié devices, audio is not emulated beyond the usual beeps.
- The MQ-1168 video chip in PEG-NR70 series devices is not emulated beyond
  framebuffer support, 2D acceleration is not available.
- Formatting an unformatted memory stick in Clié devices that run PalmOS 3.x
  locks up PalmOS.
- On OS5, keyboard input, clipboard and network integration are not
  currently available. The one exception is typing on the hardware keyboard
  of the Tungsten C in `cp-uarm`.
- The emulated Tungsten|W does not provide a full 5-way D-pad, even though
  the skin suggests otherwise.
- There is no support for changing the hotsync user name on OS5. Please use
  MultiUserHack or a similar application for that.
- Tungsten C: only available in the native `cp-uarm` app. Characters that
  are not on the hardware keyboard cannot be typed directly (see
  [above](#typing-on-the-tungsten-c)), the power and hotsync buttons are not
  mapped, Wi-Fi is not emulated, and boot takes about 30 seconds of emulated time at the default
  speed of 100 MIPS (the real device runs at 400 MHz, use `--mips` to go
  faster).
- Tungsten T3: only available in the native `cp-uarm` app. SD cards are not
  supported (the SD controller is only partially emulated), there is no
  audio, and the voice recorder button, the hotsync button and the power
  button are not mapped.

# Building

The build is controlled by `src/Makefile`. You can tune the compiler and linker
options by copying `Makefile.local.example` to `Makefile.local` and editing it.

The following instructions apply to Linux and MacOS.

Please not the parts of the source code for the native build reside in a submodule
which needs to be checked out and updated together with the main code. This
can be done with

```
    $ git clone --recurse-submodules
    $ git submodule update --recursive
```

## Web app

### Emulator

The emulator is written in C++ and compiled to Web Assembly using emscripten. In
order to build the WASM binary you need to install a recent version of the
emscripten toolchain. CloudpilotEmu currently builds with 4.0.10.

With the SDK properly set up you can build the source via

```
    $ make emscripten
```

### Javascript

The web app is written in Typescript using [Angular](https://angular.io) and
[Ionic](https://ionicframework.com). In order to build
you need [NodeJS](https://nodejs.org/en/) and [yarn](https://yarnpkg.com)
installed. Go to the `web` directory and do

```
    $ yarn install
    $ yarn gen-revision
    $ yarn build
    $ yarn generate-worker
```

The result will be in the `build-pwa` subdir. The last step generates a service
worker for offline operation. You can skip it if you don't require offline
operation, the app will work fine without it.

Note that you need to build the WASM binary (see above) before building the web
app.

If you want to do development on the frontend, you can launch a live build
on `http://localhost:4200` by running

```
    $ yarn start
```

## Native build

### MacOS / Linux

Building the native version of CloudpilotEmu requires SDL2, sdl2-image and libcurl.

Note that for network support via proxy to actually work, libcurl must be at
least version 8.0.0 and needs to be built with websocket support, which may or
not be the case for the libcurl shipped with your system. CloudpilotEmu will
build and run against any version of libcurl, but the proxy will be disabled.
On MacOS you can use libcurl from homebrew (which enables websocket support), on
Ubuntu you currently need to build libcurl yourself for websocket support.

On Ubuntu the following will give you the necessary packages:

```
    $ apt-get install libreadline-dev libcurl4-openssl-dev libsdl2-image-dev libsdl2-dev
```

On Fedora

```
    $ dnf install readline-devel libcurl-devel SDL2_image-devel SDL2-devel
```

On MacOS, install the dependencies with [Homebrew](https://brew.sh)

```
    $ brew install sdl2 sdl2_image readline curl
```

MacOS ships libedit in place of GNU readline, and the build fails against it.
Homebrew installs readline and curl "keg-only", so you need to add their paths
in `src/Makefile.local` (see `src/Makefile.local.example`):

```
INCLUDE_EXTRA = -I/opt/homebrew/include -I/opt/homebrew/opt/readline/include \
	-I/opt/homebrew/opt/curl/include

LDFLAGS_NATIVE = \
	$(shell sdl2-config --libs) -lSDL2_image -lreadline -pthread -lresolv -lcurl \
	-L/opt/homebrew/lib -L/opt/homebrew/opt/readline/lib -L/opt/homebrew/opt/curl/lib
```

If you leave out the curl paths, the build links against the system libcurl,
which lacks websocket support, and the network proxy is disabled.

If you cloned without `--recurse-submodules`, initialize the submodules first
with `git submodule update --init --recursive`.

The build is accomplished with

```
    $ make bin
```

and you will up with `src/cloudpilot/cloudpilot-emu` and `src/uarm/cp-uarm` binaries.

### Windows

Windows binaries can be cross built with MinGW using the provided dockerfile

```
    $ docker buildx build --out ./out -f Dockerfile.build-mingw .
```

This will build binaries for x64 and x64 and place the in the `out` folder. Native builds
on Windows are also possible, but no build infrastructure is provided.

# Credits

- Artwork for CloudpilotEmu was done by Paolo Lazatin.
- SDCTL emulation to enable the full 16MB of RAM on the m515 is taken from
  [Mu](https://github.com/meepingsnesroms/Mu).
- Zipfile I/O uses the [zip library](https://github.com/kuba--/zip).
- Protobuf (de)serialization in C++ is done using the awesome
  [NanoPB](https://github.com/nanopb/nanopb) library.
- The native build uses [argparse](https://github.com/p-ranav/argparse) and
  [uri-library](https://github.com/ben-zen/uri-library).
- Stacktraces in the native
  build are based on Farooq Melas [gist](https://gist.github.com/fmela/591333).
- HTTP and Websocket I/O in the native build use
  [libcurl](https://curl.se/libcurl/c/libcurl-ws.html).
- FAT support
  is built on [dosfstools](https://github.com/dosfstools/dosfstools) and
  [FatFs](http://elm-chan.org/fsw/ff).
- Parts of the GDB stub are taken from
  [uARM](https://github.com/uARM-Palm/uARM).
- Dmitry Grinberg for lots of support and PalmOS wisdom, uARM and rePalm

# LICENSE

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.
