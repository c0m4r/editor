# Simple Text Editor

A lightweight terminal-based text editor for GNU/Linux with zero external dependencies. Written in pure C using only standard libraries.

## Features

- No external dependencies (only standard C library)
- Terminal-based interface
- Basic text editing capabilities
- File loading and saving
- Cursor navigation with arrow keys
- Status bar showing filename, line count, and modification status
- Syntax: works on any GNU/Linux system with a terminal

## Building

Simply compile with:
```bash
make
```

Or manually:
```bash
gcc -Wall -Wextra -std=c99 -pedantic -o editor editor.c
```

## Usage

To create a new file:
```bash
./editor
```

To open an existing file:
```bash
./editor filename.txt
```

## Keyboard Shortcuts

### Navigation
- **Arrow Keys**: Move cursor up/down/left/right
- **Home**: Move to beginning of line
- **End**: Move to end of line
- **Page Up/Down**: Scroll by screen height

### Editing
- **Enter**: Insert new line
- **Backspace**: Delete character before cursor
- **Ctrl-D**: Delete character at cursor
- **Any printable character**: Insert at cursor

### File Operations
- **Ctrl-S**: Save file
- **Ctrl-Q**: Quit editor

## Technical Details

- Uses ANSI escape sequences for terminal control
- Implements raw mode terminal handling
- Dynamic memory allocation for lines
- No dependencies on ncurses, termbox, or any other library
- Pure POSIX C with standard library only

## Requirements

- GNU/Linux operating system
- GCC or compatible C compiler
- Terminal emulator with ANSI escape code support

## Installation

To install system-wide (requires root):
```bash
sudo make install
```

This will copy the binary to `/usr/local/bin/`.

## Notes

- The editor saves files exactly as you see them
- Modified files are marked with "(modified)" in the status bar
- Currently no confirmation on quit with unsaved changes (planned feature)
- Maximum line length is dynamically allocated

## License

This is a simple educational text editor. Feel free to use and modify as needed.
