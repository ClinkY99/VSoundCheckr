# VSoundCheckr
Hello, welcome to VSoundCheckr (Working Title). This software is a **WORK IN PROGRESS** personal project, with the intention of streamlining the virtual soundcheck process.
Currently VSoundCheckr is only built for Windows, but support for MAC and Linux is planned. VSoundCheckr is licensed under the GNU General Public License v3.0

*Please note: at this time I will not be accepting contributions to the project due to time constraints. This will likely change in the future*

### Features
- Supports multitrack recording and playback
- Midi interface with audio board to save snapshots during recording, allowing for easy seeking during playback
- Midi interface with audio board to allow macro's to control the software (See below for macro values)
- See roadmap for upcoming and planned features

## Roadmap
*Please note: this project is still under active development, but takes lower priority than my studies at university. Development might be slow throughout the year.*

Upcoming and planned features:
- GUI (currently VSoundCheckr is a console based application, I am aiming to develop a custom UI for easier use)
- Exporting to wav and mp4
- MAC and LINUX port

## Known Issues
> [!WARNING]
> If you exit the program by closing the command prompt window, due to limitations with console based applications the program will not run important cleanup functions. This could lead to a bluescreen on windows pc while using ASIO as well as not properly closing save database leading to potential loss of data.
> 
> PLEASE EXIT USING INTERNAL MENU TO AVOID THIS

## Macro Values
VSoundCheckr supports a number of midi macros for interfacing with the audio board. All of the midi macros are received as a program change command.

| Action | Midi Command |
|--------|--------------|
| Play | 1 |
| Record | 2 |
| Pause | 3 |
| End Playback Mode (if not recording or playing back will exit midi mode) | 4 |
| Jump Forward 15s | 7 |
| Jump Backward 15s | 8 |
| Jump Forward 30s | 5 |
| Jump Backward 30s | 6 |
| Return to last fired snapshot | 9 |

This photo shows the setup for the macro which will cause the program to play

<img width="1942" height="1189" alt="image" src="https://github.com/user-attachments/assets/2658a685-7b5a-4922-ab6e-bb747c5654a9" />

## Snapshots
Snapshots are recorded using control change midi actions. This is the default command fired when a DiGiCo board fires a snapshot. For other boards support is planned for the future.

By default snapshot 0, with CC command value [16, 0] will be created at time 00:00. The snapshot that you start the recording on should have this midi value.

This photo highlights important areas for setting up midi send via snapshot

<img width="2000" height="1457" alt="image" src="https://github.com/user-attachments/assets/ba970782-04ba-4068-afd7-3715bb44215e" />


## License
This project is licensed under the terms of the GNU General Public License v3.0 (GPLv3).
See the [LICENSE](LICENSE) file for details.
