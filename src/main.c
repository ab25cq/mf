#include <comelang.h>

using unsafe;

#include <ncurses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define COLS 3

struct sInfo
{
    int cursor;
    int page;
    string path;
    bool app_end;
    
    list<string>* files;
};

int xgetmaxx()
{
    auto ws = new winsize;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, ws);
    
    int result = ws.ws_col;
//Raspberry PI return -1
    if(result == -1 || result == 0) {
        return getmaxx(stdscr);
    }
    else {
        return result;
    }
    
    return result;
}

int xgetmaxy()
{
    auto ws = new winsize;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, ws);
    
    int result = ws.ws_row;
//Raspberry PI return -1
    if(result == -1 || result == 0) {
        return getmaxy(stdscr);
    }
    else {
        return result;
    }
    
    return result;
}

void read_dir(sInfo* info)
{
    info.files = new list<string>();

    DIR* dir = opendir(info.path);

    if(dir == null) {
        info.cursor = 0;
        info.page = 0;
        info.files.push_back(string("NO FILES"));
        return;
    }
    
    struct dirent* entry;
    while(entry = readdir(dir)) {
        info.files.push_back(string(entry->d_name));
    }

    closedir(dir);

    info.files = info.files.sort_with_lambda(int lambda(char* left, char* right) { return strcmp(left, right); });
}

void vd(sInfo* info)
{
    char* line = readline(getenv("PWD") + " > ");
    
    if(line == null) {
        return;
    }

    string cmdline = string(line);
    
    free(line);
    
    char buf[BUFSIZ];
    
    FILE* f = popen(cmdline, "r");
    if(f == NULL) {
        return;
    }
    info.files.push_back(string("."));
    info.files.push_back(string(".."));

    while(1) {
        char file[PATH_MAX];
        char* result = fgets(file, PATH_MAX, f);
        
        if(result == null) {
            break;
        }
        
        info.files.push_back(string(result).chomp());
    }
    if(pclose(f) < 0) {
        return;
    }

    info.files = info.files.sort_with_lambda(int lambda(char* left, char* right) { return strcmp(left, right); });
}

void fix_cursor(sInfo* info);

bool change_directory(sInfo* info, char* path, char* cursor_file)
{
    auto absolute_path = realpath(path, NULL!);
    
    auto absolute_path2 = string(absolute_path);
    
    free(absolute_path);

    info.path = absolute_path2;
    read_dir(info);

    chdir(info.path);
    setenv("PWD", info.path, 1);

    if(cursor_file) {
        int i = 0;
        foreach(it, info.files) {
            if(strcmp(it, cursor_file) == 0) {
                info.cursor = i;
                fix_cursor(info);
                break;
            }
            
            i++;
        }
    }
    else {
        info.cursor = 0;
        info.page = 0;
    }
    
    return true;
}


void fix_cursor(sInfo* info)
{
    int maxx = xgetmaxx();
    int maxy = xgetmaxy()-1;

    if(info.cursor >= info.files.length()) {
        info.cursor = info.files.length() - 1;
    }
    if(info.cursor < 0) {
        info.cursor = 0;
    }

    info.page = info.cursor / (COLS*maxy);
}

void view(sInfo* info)
{
    int maxx = xgetmaxx();
    int maxy = xgetmaxy()-1;

    erase();
    
    int files_in_one_page = maxy * COLS;

    int head = info.page * files_in_one_page;
    int tail = (info.page+1) * files_in_one_page;

    info.files.sublist(head, tail).each {
        string path = info.path + string("/") + it;

        struct stat stat_;
        (void)stat(path, &stat_);

        bool is_dir = S_ISDIR(stat_.st_mode);

        int index = it2;
        int cols = maxx/COLS;
        int x = (index / maxy) * cols;
        int y = index % maxy;
        if(it2+head == info.cursor) {
            using c { attron(A_REVERSE); }
            if(is_dir) {
                mvprintw(y, x, "%s/", it.substring(0, cols-1));
            }
            else {
                mvprintw(y, x, "%s", it.substring(0, cols));
            }
            using c { attroff(A_REVERSE); }
        }
        else {
            if(is_dir) {
                mvprintw(y, x, "%s/", it.substring(0, cols-1));
            }
            else {
                mvprintw(y, x, "%s", it.substring(0, cols));
            }
        }
    }

    using c { attron(A_REVERSE); }
    mvprintw(maxy, 0, "%s page %d files %d head %d tail %d press ? for manual", info.path, info.page, info.files.length(), head, tail);
    using c { attroff(A_REVERSE); }

    refresh();
}


string cursor_path(sInfo* info)
{
    char* file_name = info.files.item(info.cursor, null!);
    return xsprintf("%s/%s", info.path, file_name);
}

string cursor_file(sInfo* info)
{
    return string(info.files.item(info.cursor, null!));
}

void search_file(sInfo* info)
{
    string str = string("");
    while(true) {
        int key = getch();
        
        if(key >= ' ' && key <= '~') {
            str = xsprintf("%s%c", str, key);
            int n = 0;
            foreach(it, info.files) {
                if(strcasestr(it, str)) {
                    info.cursor = n;
                    break;
                }
                n++;
            }
        }
        else {
            break;
        }
    }
}

void manual(sInfo* info)
{
    clear();
    mvprintw(0,0, "q --> quit");
    mvprintw(1,0, "* --> virtual directory(type shell command, and the result of the command is file list");
    mvprintw(2,0, "ENTER --> run command(type shell command) or insert directory");
    mvprintw(3,0, "~ --> move to home directory");
    mvprintw(4,0, "BACK SPACE ^H --> move to the parent directory");
    mvprintw(5,0, "d --> delete file");
    mvprintw(6,0, "c --> copy file");
    mvprintw(7,0, "m --> move file");
    mvprintw(8,0, "n --> new file");
    mvprintw(9,0, "x --> excute file");
    mvprintw(10,0, "e --> edit file");
    mvprintw(11,0, "LEFT h --> move cursor left");
    mvprintw(12,0, "RIGHT l --> move cursor right");
    mvprintw(13,0, "DOWN j --> move cursor down");
    mvprintw(14,0, "UP k --> move cursor up");
    mvprintw(15,0, "CTRL-L --> reread directory and refresh the window");
    mvprintw(16,0, "/ --> move cursor with searching file");
    mvprintw(17,0, "? --> this manual");
    mvprintw(18,0, ": --> run shell");
    
    refresh();
    getch();
}

void input(sInfo* info)
{
    int maxx = xgetmaxx();
    int maxy = xgetmaxy()-1;

    auto key = getch();

    switch(key) {
        case 'q':
            info.app_end = true;
            break;
            
        case '*':
            endwin();
            info.files.reset();
            vd(info);
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            break;

        case KEY_ENTER:
        case '\n': {
            string path = info.path + string("/") + cursor_file(info);

            struct stat stat_;
            (void)stat(path, &stat_);

            bool is_dir = S_ISDIR(stat_.st_mode);

            if(is_dir) {
                change_directory(info, path, null!);
            }
            else {
                endwin();
                system(xsprintf("shsh -i ' %s' -n 0 -o", cursor_file(info)));
                read_dir(info);
                puts("HIT ANY KEY");
                initscr();
                keypad(stdscr, true);
                raw();
                noecho();
                getchar();
            }
            }
            break;
            
        case '~': {
            string path = string(getenv("HOME"));

            change_directory(info, path, null!);
            }
            break;

        case KEY_BACKSPACE:
        case 8:
        case 127: {
            string current_directory_name = xbasename(info.path);
            string path = info.path + string("/..");
            change_directory(info, path, current_directory_name);
            }
            break;

        case 'd': {
            endwin();
            system(xsprintf("shsh -i 'rm -rf %s' -o", cursor_file(info)));
            read_dir(info);
            puts("HIT ANY KEY");
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            getchar();
            }
            break;

        case 'c': {
            endwin();
            system(xsprintf("shsh -i 'cp -r %s ' -o", cursor_file(info)));
            read_dir(info);
            puts("HIT ANY KEY");
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            getchar();
            }
            break;

        case 'm': {
            endwin();
            system(xsprintf("shsh -i 'mv %s ' -o", cursor_file(info)));
            read_dir(info);
            puts("HIT ANY KEY");
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            getchar();
            }
            break;

        case 'n': {
            endwin();
            system(xsprintf("shsh -i 'touch '"));
            read_dir(info);
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            }
            break;

        case 'x': {
            endwin();
            system(xsprintf("shsh -i ' ./%s' -n 0 -o", cursor_file(info)));
            read_dir(info);
            puts("HIT ANY KEY");
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            getchar();
            }
            break;

        case 'e': {
            endwin();
            system(xsprintf("vin %s", cursor_file(info)));
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            }
            break;

        case KEY_LEFT:
        case 'h':
            if(info.cursor >= maxy) {
                info.cursor-=maxy;
            }
            break;

        case KEY_RIGHT:
        case 'l':
            if(info.cursor + maxy < info.files.length()) {
                info.cursor+=maxy;
            }
            break;

        case KEY_DOWN:
        case 'j':
            info.cursor++;
            break;

        case KEY_UP:
        case 'k':
            info.cursor--;

            break;

        case 'L'-'A'+1:
            clear();
            read_dir(info);
            view(info);
            refresh();
            break;

        case '/':
            search_file(info);
            view(info);
            refresh();
            break;
            
        case '?':
            manual(info);
            break;

        case ':': {
            endwin();
            system("shsh");
            read_dir(info);
            puts("HIT ANY KEY");
            initscr();
            keypad(stdscr, true);
            raw();
            noecho();
            getchar();

            }
            break;
    }

    fix_cursor(info);
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");
    
    sInfo info;
    
    memset(&info, 0, sizeof(sInfo));
    
    char* cwd = getenv("PWD");
    
    info.path = string(cwd);
    
    read_dir(&info);
    
    initscr();
    keypad(stdscr, 1);
    raw();
    noecho();
    
    while(!info.app_end) {
        view(&info);
        input(&info);
    }
    
    endwin();
    
    return 0;
}
