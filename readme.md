# MultiBound 2 // Starbound Instance Launcher
MultiBound enables you to quickly and easily set up and use multiple separate instances (or profiles) of Starbound with their own mod list and save directory.

## What does this do that MultiBound v0.x doesn't?
MultiBound 2 is built on C++17 and Qt 5.x instead of C# and the pile of crust that is GTK#. This results in far better UI polish, as well as (likely) a smaller memory footprint.

Additionally, MultiBound 2 can automatically install/update Steam Workshop mods for you without having to launch Steam, or even have the Steam version of Starbound at all. (This happens automatically on installing or updating from a Workshop collection.)

## Installing and running
- **Windows:** Unzip anywhere writable and double-click `multibound.exe`.
- **Arch Linux:** Install `multibound-git` from the AUR; "MultiBound 2" will be added to your launcher.
- **Other Linux/\*nix:** Clone from git and compile using QtCreator like you would with any other Qt application.

## How to use
(instructions pending; once you start it up it should be fairly self-explanatory, though!)

## How to compile
Make sure you have:
- Up to date QtCreator

Open up the project and build like you would any other Qt application, configuring with the "Desktop" kit.
