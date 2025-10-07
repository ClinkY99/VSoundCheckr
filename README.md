# VSC+
Hello and Welcome VSC+ (Working Title). This software is a **WORK IN PROGRESS** personal project, with the intention of streamlining the virtual soundcheck process.
Currently VSC+ is only built for Windows, but support for MAC and Linux is planned. VSC+ is licensed under the GNU General Public License v3.0

### Features
- Supports multitrack recording and playback
- Midi interface with audio board to save snapshots during recording, allowing for easy seeking during playback
- Midi interface with audio board to allow macro's to control the software (See below for macro values)
- See roadmap for upcoming and planned features

## Roadmap
Please note: this project is still under active development, but takes lower priotity than my studies at university. Development might be slow throughout the year.

Upcoming and planned features:
- GUI (currently VSC+ is a console based application, I am aiming to develop a custom UI for easier use)
- Exporting to wav and mp4
- MAC and LINUX port

## Macro Values
VSC+ supports a number of midi macros for interfacing with the audio board. All of the midi macros are received as a program change command.

| Action | Midi Command |
|--------|--------------|
| Play | 0 |
| Record | 1 |
| Pause | 2 |
| End Playback Mode (if not recording or playing back will exit midi mode) | 3 |
| Jump Forward 15s | 6 |
| Jump Backward 15s | 7 |
| Jump Forward 30s | 4 |
| Jump Backward 30s | 5 |
| Return to last fired snapshot | 8 |

## Snapshots
Snapshots are recorded using control change midi actions. This is the default command fired when a DiGiCo board fires a snapshot. for other boards support is planned for the future

## License
This project is licensed under the terms of the GNU General Public License v3.0 (GPLv3).
See the [LICENSE](LICENSE) file for details.
