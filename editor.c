#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

#define TAB_STOP 4

typedef struct { char *data; size_t len, cap; } Line;

typedef struct {
    Line *lines;
    size_t nlines, cap;
    int cx, cy, rowoff, coloff, rows, cols, dirty;
    char *filename;
} Editor;

struct termios orig_termios;
Editor E;

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
    perror(s);
    exit(1);
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
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
    char c;
    while (read(STDIN_FILENO, &c, 1) != 1);
    if (c != '\x1b') return c;
    
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1)
        return '\x1b';
    
    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
            if (seq[2] == '~') {
                switch (seq[1]) {
                    case '1': case '7': return 'H'; // Home
                    case '3': return 4;             // Delete (Forward)
                    case '4': case '8': return 'E'; // End
                    case '5': return 'P';           // Page Up
                    case '6': return 'N';           // Page Down
                }
            }
        } else {
            switch (seq[1]) {
                case 'A': return 'U';  // Up
                case 'B': return 'D';  // Down
                case 'C': return 'R';  // Right
                case 'D': return 'L';  // Left
                case 'H': return 'H';  // Home
                case 'F': return 'E';  // End
            }
        }
    }
    return '\x1b';
}

void initLine(Line *l) { l->data = NULL; l->len = l->cap = 0; }

void insertCharToLine(Line *l, int at, int c) {
    if (at < 0 || at > l->len) at = l->len;
    if (l->len + 1 >= l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->data = realloc(l->data, l->cap);
    }
    memmove(&l->data[at + 1], &l->data[at], l->len - at);
    l->data[at] = c;
    l->len++;
}

void deleteCharFromLine(Line *l, int at) {
    if (at < 0 || at >= l->len) return;
    memmove(&l->data[at], &l->data[at + 1], l->len - at - 1);
    l->len--;
}

void insertLine(int at) {
    if (at < 0 || at > E.nlines) return;
    if (E.nlines >= E.cap) {
        E.cap = E.cap ? E.cap * 2 : 8;
        E.lines = realloc(E.lines, sizeof(Line) * E.cap);
    }
    memmove(&E.lines[at + 1], &E.lines[at], sizeof(Line) * (E.nlines - at));
    initLine(&E.lines[at]);
    E.nlines++;
    E.dirty = 1;
}

void deleteLine(int at) {
    if (at < 0 || at >= E.nlines) return;
    free(E.lines[at].data);
    memmove(&E.lines[at], &E.lines[at + 1], sizeof(Line) * (E.nlines - at - 1));
    E.nlines--;
    E.dirty = 1;
}

void insertChar(int c) {
    if (E.cy == E.nlines) insertLine(E.nlines);
    insertCharToLine(&E.lines[E.cy], E.cx, c);
    E.cx++;
    E.dirty = 1;
}

void insertNewline() {
    if (E.cy < E.nlines) {
        Line *l = &E.lines[E.cy];
        insertLine(E.cy + 1);
        if (E.cx < l->len) {
            Line *nl = &E.lines[E.cy + 1];
            nl->data = malloc(l->len - E.cx);
            memcpy(nl->data, &l->data[E.cx], l->len - E.cx);
            nl->len = nl->cap = l->len - E.cx;
            l->len = E.cx;
        }
    } else {
        insertLine(E.nlines);
    }
    E.cy++;
    E.cx = 0;
}

void deleteChar() {
    if (E.cy == E.nlines || (E.cx == 0 && E.cy == 0)) return;
    if (E.cx > 0) {
        deleteCharFromLine(&E.lines[E.cy], E.cx - 1);
        E.cx--;
    } else {
        Line *prev = &E.lines[E.cy - 1];
        E.cx = prev->len;
        if (E.lines[E.cy].len > 0) {
            prev->data = realloc(prev->data, prev->len + E.lines[E.cy].len);
            memcpy(&prev->data[prev->len], E.lines[E.cy].data, E.lines[E.cy].len);
            prev->len += E.lines[E.cy].len;
        }
        deleteLine(E.cy);
        E.cy--;
    }
    E.dirty = 1;
}

void deleteCharForward() {
    if (E.cy >= E.nlines) return;
    Line *l = &E.lines[E.cy];
    if (E.cx < l->len) {
        deleteCharFromLine(l, E.cx);
        E.dirty = 1;
    } else if (E.cy < E.nlines - 1) {
        Line *next = &E.lines[E.cy + 1]; // Join with next line
        if (next->len > 0) {
            l->data = realloc(l->data, l->len + next->len);
            memcpy(&l->data[l->len], next->data, next->len);
            l->len += next->len;
        }
        deleteLine(E.cy + 1);
        E.dirty = 1;
    }
}

void openFile(char *filename) {
    E.filename = strdup(filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) { insertLine(0); return; }
    
    char *line = NULL;
    size_t linecap = 0;
    ssize_t len;
    while ((len = getline(&line, &linecap, fp)) != -1) {
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) len--;
        insertLine(E.nlines);
        Line *row = &E.lines[E.nlines - 1];
        row->data = malloc(len);
        memcpy(row->data, line, len);
        row->len = row->cap = len;
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

int saveFile() {
    FILE *fp = fopen(E.filename, "w");
    if (!fp) return -1;
    for (size_t i = 0; i < E.nlines; i++) {
        fwrite(E.lines[i].data, 1, E.lines[i].len, fp);
        fputc('\n', fp);
    }
    fclose(fp);
    E.dirty = 0;
    return 0;
}

void scroll() {
    if (E.cy < E.rowoff) E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.rows) E.rowoff = E.cy - E.rows + 1;
    if (E.cx < E.coloff) E.coloff = E.cx;
    if (E.cx >= E.coloff + E.cols) E.coloff = E.cx - E.cols + 1;
}

void refreshScreen() {
    scroll();
    char buf[65536];
    int len = 0;
    
    len += sprintf(&buf[len], "\x1b[?25l\x1b[H");
    
    for (int y = 0; y < E.rows; y++) {
        int row = y + E.rowoff;
        if (row >= E.nlines) {
            buf[len++] = '~';
        } else {
            int show = E.lines[row].len - E.coloff;
            if (show < 0) show = 0;
            if (show > E.cols) show = E.cols;
            if (show > 0) {
                memcpy(&buf[len], &E.lines[row].data[E.coloff], show);
                len += show;
            }
        }
        len += sprintf(&buf[len], "\x1b[K\r\n");
    }
    
    len += sprintf(&buf[len], "\x1b[7m%.20s - %zu lines %s",
        E.filename, E.nlines, E.dirty ? "(modified)" : ""); // Status bar
    int slen = 20 + 10 + (E.dirty ? 10 : 0);
    char pos[20];
    int plen = sprintf(pos, "%d/%zu", E.cy + 1, E.nlines);
    while (slen++ < E.cols - plen) buf[len++] = ' ';
    len += sprintf(&buf[len], "%s\x1b[m\r\n", pos);
    
    len += sprintf(&buf[len], "\x1b[KCTRL+S:Save | CTRL+X:Save+Quit | CTRL+Q:Quit"); // Help bar
    
    len += sprintf(&buf[len], "\x1b[%d;%dH\x1b[?25h",
        E.cy - E.rowoff + 1, E.cx - E.coloff + 1); // Cursor
    
    write(STDOUT_FILENO, buf, len);
}

void moveCursor(int key) {
    Line *row = (E.cy < E.nlines) ? &E.lines[E.cy] : NULL;
    switch (key) {
        case 'L': if (E.cx > 0) E.cx--; else if (E.cy > 0) { E.cy--; E.cx = E.lines[E.cy].len; } break;
        case 'R': if (row && E.cx < row->len) E.cx++; else if (row && E.cy < E.nlines-1) { E.cy++; E.cx = 0; } break;
        case 'U': if (E.cy > 0) E.cy--; break;
        case 'D': if (E.cy < E.nlines) E.cy++; break;
    }
    row = (E.cy < E.nlines) ? &E.lines[E.cy] : NULL;
    if (E.cx > (row ? row->len : 0)) E.cx = row ? row->len : 0;
}

void quit() {
    write(STDOUT_FILENO, "\x1b[2J\x1b[H", 7);
    exit(0);
}

void processKeypress() {
    int c = readKey();
    switch (c) {
        case '\r': insertNewline(); break;
        case 'H': E.cx = 0; break;
        case 'E': if (E.cy < E.nlines) E.cx = E.lines[E.cy].len; break;
        case 'P': case 'N': for (int i = E.rows; i--;) moveCursor(c == 'P' ? 'U' : 'D'); break;
        case 'U': case 'D': case 'L': case 'R': moveCursor(c); break;
        case 127: case 8: deleteChar(); break;
        case 19: saveFile(); break;          // Ctrl-S
        case 24: saveFile(); quit(); break;  // Ctrl-X
        case 17: quit(); break;              // Ctrl-Q
        default: if (c >= 32 && c < 127) insertChar(c); break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    
    enableRawMode();
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) die("ioctl");
    E.rows = ws.ws_row - 2;
    E.cols = ws.ws_col;
    
    openFile(argv[1]);
    
    while (1) {
        refreshScreen();
        processKeypress();
    }
    return 0;
}
