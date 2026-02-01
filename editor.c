#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>

#define VERSION "1.0"
#define TAB_STOP 4
#define MAX_LINE_LENGTH 1024

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Line;

typedef struct {
    Line *lines;
    size_t num_lines;
    size_t capacity;
    int cx, cy;  // Cursor position
    int rowoff, coloff;  // Scroll offset
    int screenrows, screencols;
    char *filename;
    int dirty;  // Modified flag
} EditorState;

struct termios orig_termios;
EditorState E;

// Terminal handling
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int readKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return 'H';
                        case '3': return 'D';
                        case '4': return 'E';
                        case '5': return 'P';
                        case '6': return 'N';
                        case '7': return 'H';
                        case '8': return 'E';
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return 'U';
                    case 'B': return 'D';
                    case 'C': return 'R';
                    case 'D': return 'L';
                    case 'H': return 'H';
                    case 'F': return 'E';
                }
            }
        }
        return '\x1b';
    }
    return c;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

// Line operations
void initLine(Line *line) {
    line->data = NULL;
    line->length = 0;
    line->capacity = 0;
}

void insertCharToLine(Line *line, int at, int c) {
    if (at < 0 || at > line->length) at = line->length;
    
    if (line->length + 1 >= line->capacity) {
        line->capacity = line->capacity == 0 ? 8 : line->capacity * 2;
        line->data = realloc(line->data, line->capacity);
    }
    
    memmove(&line->data[at + 1], &line->data[at], line->length - at);
    line->data[at] = c;
    line->length++;
}

void deleteCharFromLine(Line *line, int at) {
    if (at < 0 || at >= line->length) return;
    memmove(&line->data[at], &line->data[at + 1], line->length - at - 1);
    line->length--;
}

// Editor operations
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.num_lines = 0;
    E.capacity = 0;
    E.lines = NULL;
    E.filename = NULL;
    E.dirty = 0;
    
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;  // Status bar and message bar
}

void insertLine(int at) {
    if (at < 0 || at > E.num_lines) return;
    
    if (E.num_lines >= E.capacity) {
        E.capacity = E.capacity == 0 ? 8 : E.capacity * 2;
        E.lines = realloc(E.lines, sizeof(Line) * E.capacity);
    }
    
    memmove(&E.lines[at + 1], &E.lines[at], sizeof(Line) * (E.num_lines - at));
    initLine(&E.lines[at]);
    E.num_lines++;
    E.dirty = 1;
}

void deleteLine(int at) {
    if (at < 0 || at >= E.num_lines) return;
    free(E.lines[at].data);
    memmove(&E.lines[at], &E.lines[at + 1], sizeof(Line) * (E.num_lines - at - 1));
    E.num_lines--;
    E.dirty = 1;
}

void insertChar(int c) {
    if (E.cy == E.num_lines) {
        insertLine(E.num_lines);
    }
    insertCharToLine(&E.lines[E.cy], E.cx, c);
    E.cx++;
    E.dirty = 1;
}

void insertNewline() {
    if (E.cy < E.num_lines) {
        Line *line = &E.lines[E.cy];
        insertLine(E.cy + 1);
        
        // Move text after cursor to new line
        if (E.cx < line->length) {
            Line *newline = &E.lines[E.cy + 1];
            newline->data = malloc(line->length - E.cx);
            memcpy(newline->data, &line->data[E.cx], line->length - E.cx);
            newline->length = line->length - E.cx;
            newline->capacity = line->length - E.cx;
            line->length = E.cx;
        }
    } else {
        insertLine(E.num_lines);
    }
    
    E.cy++;
    E.cx = 0;
}

void deleteChar() {
    if (E.cy == E.num_lines) return;
    if (E.cx == 0 && E.cy == 0) return;
    
    if (E.cx > 0) {
        deleteCharFromLine(&E.lines[E.cy], E.cx - 1);
        E.cx--;
        E.dirty = 1;
    } else {
        // Join with previous line
        Line *prev = &E.lines[E.cy - 1];
        E.cx = prev->length;
        
        if (E.lines[E.cy].length > 0) {
            prev->data = realloc(prev->data, prev->length + E.lines[E.cy].length);
            memcpy(&prev->data[prev->length], E.lines[E.cy].data, E.lines[E.cy].length);
            prev->length += E.lines[E.cy].length;
        }
        
        deleteLine(E.cy);
        E.cy--;
        E.dirty = 1;
    }
}

// File I/O
void openFile(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        insertLine(0);
        return;
    }
    
    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
            linelen--;
        
        insertLine(E.num_lines);
        Line *row = &E.lines[E.num_lines - 1];
        row->data = malloc(linelen);
        memcpy(row->data, line, linelen);
        row->length = linelen;
        row->capacity = linelen;
    }
    
    free(line);
    fclose(fp);
    E.dirty = 0;
}

int saveFile() {
    if (E.filename == NULL) return -1;
    
    FILE *fp = fopen(E.filename, "w");
    if (!fp) return -1;
    
    for (size_t i = 0; i < E.num_lines; i++) {
        fwrite(E.lines[i].data, 1, E.lines[i].length, fp);
        fwrite("\n", 1, 1, fp);
    }
    
    fclose(fp);
    E.dirty = 0;
    return 0;
}

// Display
void scroll() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
}

void drawRows(char *buf, int *len) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        
        if (filerow >= E.num_lines) {
            if (E.num_lines == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Simple Editor -- version %s", VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    buf[(*len)++] = '~';
                    padding--;
                }
                while (padding--) buf[(*len)++] = ' ';
                
                memcpy(&buf[*len], welcome, welcomelen);
                *len += welcomelen;
            } else {
                buf[(*len)++] = '~';
            }
        } else {
            int len_to_show = E.lines[filerow].length - E.coloff;
            if (len_to_show < 0) len_to_show = 0;
            if (len_to_show > E.screencols) len_to_show = E.screencols;
            if (len_to_show > 0) {
                memcpy(&buf[*len], &E.lines[filerow].data[E.coloff], len_to_show);
                *len += len_to_show;
            }
        }
        
        buf[(*len)++] = '\x1b';
        buf[(*len)++] = '[';
        buf[(*len)++] = 'K';
        buf[(*len)++] = '\r';
        buf[(*len)++] = '\n';
    }
}

void drawStatusBar(char *buf, int *len) {
    buf[(*len)++] = '\x1b';
    buf[(*len)++] = '[';
    buf[(*len)++] = '7';
    buf[(*len)++] = 'm';
    
    char status[80], rstatus[80];
    int slen = snprintf(status, sizeof(status), "%.20s - %zu lines %s",
        E.filename ? E.filename : "[No Name]", E.num_lines,
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%zu",
        E.cy + 1, E.num_lines);
    
    if (slen > E.screencols) slen = E.screencols;
    memcpy(&buf[*len], status, slen);
    *len += slen;
    
    while (slen < E.screencols) {
        if (E.screencols - slen == rlen) {
            memcpy(&buf[*len], rstatus, rlen);
            *len += rlen;
            break;
        } else {
            buf[(*len)++] = ' ';
            slen++;
        }
    }
    
    buf[(*len)++] = '\x1b';
    buf[(*len)++] = '[';
    buf[(*len)++] = 'm';
    buf[(*len)++] = '\r';
    buf[(*len)++] = '\n';
}

void drawMessageBar(char *buf, int *len) {
    buf[(*len)++] = '\x1b';
    buf[(*len)++] = '[';
    buf[(*len)++] = 'K';
    
    char msg[80];
    int msglen = snprintf(msg, sizeof(msg), 
        "Ctrl-S: Save | Ctrl-Q: Quit");
    if (msglen > E.screencols) msglen = E.screencols;
    memcpy(&buf[*len], msg, msglen);
    *len += msglen;
}

void refreshScreen() {
    scroll();
    
    char buf[65536];
    int len = 0;
    
    buf[len++] = '\x1b';
    buf[len++] = '[';
    buf[len++] = '?';
    buf[len++] = '2';
    buf[len++] = '5';
    buf[len++] = 'l';
    
    buf[len++] = '\x1b';
    buf[len++] = '[';
    buf[len++] = 'H';
    
    drawRows(buf, &len);
    drawStatusBar(buf, &len);
    drawMessageBar(buf, &len);
    
    char cursor_buf[32];
    int cursor_len = snprintf(cursor_buf, sizeof(cursor_buf), "\x1b[%d;%dH",
        (E.cy - E.rowoff) + 1, (E.cx - E.coloff) + 1);
    memcpy(&buf[len], cursor_buf, cursor_len);
    len += cursor_len;
    
    buf[len++] = '\x1b';
    buf[len++] = '[';
    buf[len++] = '?';
    buf[len++] = '2';
    buf[len++] = '5';
    buf[len++] = 'h';
    
    write(STDOUT_FILENO, buf, len);
}

// Input
void moveCursor(int key) {
    Line *row = (E.cy >= E.num_lines) ? NULL : &E.lines[E.cy];
    
    switch (key) {
        case 'L':
            if (E.cx != 0) E.cx--;
            else if (E.cy > 0) {
                E.cy--;
                E.cx = E.lines[E.cy].length;
            }
            break;
        case 'R':
            if (row && E.cx < row->length) E.cx++;
            else if (row && E.cx == row->length && E.cy < E.num_lines - 1) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case 'U':
            if (E.cy != 0) E.cy--;
            break;
        case 'D':
            if (E.cy < E.num_lines) E.cy++;
            break;
    }
    
    row = (E.cy >= E.num_lines) ? NULL : &E.lines[E.cy];
    int rowlen = row ? row->length : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

void processKeypress() {
    int c = readKey();
    
    switch (c) {
        case '\r':
            insertNewline();
            break;
            
        case 'H':
            E.cx = 0;
            break;
            
        case 'E':
            if (E.cy < E.num_lines)
                E.cx = E.lines[E.cy].length;
            break;
            
        case 'P':
        case 'N':
            {
                int times = E.screenrows;
                while (times--)
                    moveCursor(c == 'P' ? 'U' : 'D');
            }
            break;
            
        case 'U':
        case 'D':
        case 'L':
        case 'R':
            moveCursor(c);
            break;
            
        case 127:
        case 8:
            deleteChar();
            break;
            
        case 4:  // Ctrl-D (Delete)
            if (E.cy < E.num_lines && E.cx < E.lines[E.cy].length)
                moveCursor('R');
            deleteChar();
            break;
            
        case 19:  // Ctrl-S
            if (saveFile() == 0) {
                // Saved successfully
            }
            break;
            
        case 17:  // Ctrl-Q
            if (E.dirty) {
                // Could add confirmation here
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
            
        default:
            if (c >= 32 && c < 127) {
                insertChar(c);
            }
            break;
    }
}

// Main
int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    
    if (argc >= 2) {
        openFile(argv[1]);
    } else {
        insertLine(0);
    }
    
    while (1) {
        refreshScreen();
        processKeypress();
    }
    
    return 0;
}
