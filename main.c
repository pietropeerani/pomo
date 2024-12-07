#include <curses.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BORDER_WIDTH 3
#define POMODORO_TIME 25 * 60
#define SHORT_BREAK_TIME 5 * 60
#define MEDIUM_BREAK_TIME 15 * 60

typedef enum
{
    STOPPED,
    RUNNING,
    PAUSED
} TimerState;

void draw_border(WINDOW *win, int width, int height);
void draw_interface(WINDOW *win, int width, int height, int remaining_time, TimerState state, int session, int times[], int menu_size, int selected_menu);
void draw_clock(WINDOW *win, int width, int height);
void save_session(const char *title);
void save_settings(int times[], int size);
void load_settings(int times[], int size);
void display_sessions(WINDOW *win, int width, int height);
void draw_input_window(WINDOW *win, int width, int height, const char *prompt, char *input, int max_input_length);

int main()
{
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    int width, height;
    getmaxyx(stdscr, height, width);

    TimerState state = STOPPED;
    int session = 0;
    const char *menu[] = {"Pomodoro", "Short Break", "Medium Break"};
    int times[] = {POMODORO_TIME, SHORT_BREAK_TIME, MEDIUM_BREAK_TIME};
    int menu_size = sizeof(menu) / sizeof(menu[0]);

    // Load saved settings
    load_settings(times, menu_size);

    int selected_menu = 0;
    int remaining_time = times[selected_menu];
    int start_time = 0;

    WINDOW *main_win = newwin(height, width, 0, 0);
    keypad(main_win, TRUE);
    nodelay(main_win, TRUE);

    int ch;
    while (1)
    {
        werase(main_win);
        getmaxyx(stdscr, height, width); // Update dimensions on resize
        draw_border(main_win, width, height);
        draw_interface(main_win, width, height, remaining_time, state, session, times, menu_size, selected_menu);
        draw_clock(main_win, width, height);
        wrefresh(main_win);

        ch = wgetch(main_win);
        if (ch == 'q')
        {
            break;
        }
        else if (ch == ' ')
        {
            if (state == STOPPED || state == PAUSED)
            {
                state = RUNNING;
                start_time = time(NULL);
            }
            else if (state == RUNNING)
            {
                state = PAUSED;
            }
        }
        else if (ch == 'r')
        {
            state = STOPPED;
            remaining_time = times[selected_menu];
        }
        else if (ch == KEY_LEFT)
        {
            selected_menu = (selected_menu - 1 + menu_size) % menu_size;
            remaining_time = times[selected_menu];
        }
        else if (ch == KEY_RIGHT)
        {
            selected_menu = (selected_menu + 1) % menu_size;
            remaining_time = times[selected_menu];
        }
        else if (ch == '+')
        {
            times[selected_menu] += 5 * 60;
            remaining_time = times[selected_menu];
            save_settings(times, menu_size);
        }
        else if (ch == '-')
        {
            if (times[selected_menu] > 5 * 60)
            {
                times[selected_menu] -= 5 * 60;
                remaining_time = times[selected_menu];
                save_settings(times, menu_size);
            }
        }
        else if (ch == 'l')
        {
            display_sessions(main_win, width, height);
        }

        if (state == RUNNING)
        {
            int elapsed = time(NULL) - start_time;
            remaining_time -= elapsed;
            start_time = time(NULL);
            if (remaining_time <= 0)
            {
                remaining_time = 0;
                state = STOPPED;

                char title[256];
                draw_input_window(main_win, width, height, "Session Title: ", title, sizeof(title));

                save_session(title);
                noecho();
                curs_set(0);

                mvwprintw(main_win, height / 2 + 6, width / 2 - 10, "Session saved: %s", title);
                wrefresh(main_win);
            }
        }

        session = selected_menu;
    }

    delwin(main_win);
    endwin();
    return 0;
}

void draw_border(WINDOW *win, int width, int height)
{
    mvwaddch(win, 0, 0, ACS_ULCORNER);
    mvwaddch(win, 0, width - 1, ACS_URCORNER);
    mvwaddch(win, height - 1, 0, ACS_LLCORNER);
    mvwaddch(win, height - 1, width - 1, ACS_LRCORNER);

    mvwhline(win, 0, 1, ACS_HLINE, width - 2);
    mvwhline(win, height - 1, 1, ACS_HLINE, width - 2);
    mvwvline(win, 1, 0, ACS_VLINE, height - 2);
    mvwvline(win, 1, width - 1, ACS_VLINE, height - 2);
}

void draw_interface(WINDOW *win, int width, int height, int remaining_time, TimerState state, int session, int times[], int menu_size, int selected_menu)
{
    int minutes = remaining_time / 60;
    int seconds = remaining_time % 60;

    int center_y = height / 2;
    int center_x = width / 2;

    char *ascii_art[] = {
        "   .|||||||||.",
        "  |||||||||||||",
        " /. `|||||||||||",
        "o__,_||||||||||'"};

    wattron(win, A_BOLD);  // Bold ON
    for (int i = 0; i < 4; i++)
    {
        mvwprintw(win, center_y - 3 + i, center_x - strlen(ascii_art[i]) / 2, "%s", ascii_art[i]);
    }
    wattroff(win, A_BOLD);  // Bold OFF

    // Add space
    int spacer_y = center_y + 2;

    mvwprintw(win, spacer_y, center_x - 5, "%02d:%02d", minutes, seconds);

    // Calculate the total width of all menu items
    int total_menu_width = 0;
    for (int i = 0; i < menu_size; i++)
    {
        char time_text[16];
        int menu_minutes = times[i] / 60;
        int menu_seconds = times[i] % 60;
        snprintf(time_text, sizeof(time_text), "%d min", menu_minutes);
        if (menu_seconds > 0)
        {
            snprintf(time_text + strlen(time_text), sizeof(time_text) - strlen(time_text), " %02d", menu_seconds);
        }
        total_menu_width += strlen(time_text) + 4;
    }

    // Calculate the starting X position to center the menu horizontally
    int start_x = (width - total_menu_width) / 2;

    // Dynamically update the menu display based on the timer's current value
    for (int i = 0; i < menu_size; i++)
    {
        char time_text[16];
        int menu_minutes = times[i] / 60;
        int menu_seconds = times[i] % 60;
        snprintf(time_text, sizeof(time_text), "%d min", menu_minutes);
        if (menu_seconds > 0)
        {
            snprintf(time_text + strlen(time_text), sizeof(time_text) - strlen(time_text), " %02d", menu_seconds);
        }

        if (i == selected_menu)
        {
            wattron(win, A_REVERSE);
        }

        mvwprintw(win, spacer_y + 2, start_x, "%s", time_text);

        start_x += strlen(time_text) + 4; // Add space between menu items

        if (i == selected_menu)
        {
            wattroff(win, A_REVERSE);
        }
    }

    // Display the current state of the timer (Running, Paused, Stopped)
    if (state == RUNNING)
    {
        mvwprintw(win, spacer_y + 4, center_x - 6, "Running");
    }
    else if (state == PAUSED)
    {
        mvwprintw(win, spacer_y + 4, center_x - 5, "Paused");
    }
    else
    {
        mvwprintw(win, spacer_y + 4, center_x - 5, "Stopped");
    }
}

void draw_clock(WINDOW *win, int width, int height)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    mvwprintw(win, height - BORDER_WIDTH - 1, width - strlen(time_str) - 2, "%s", time_str);
}

void save_session(const char *title)
{
    FILE *file = fopen("sessions.txt", "a");
    if (file)
    {
        time_t now = time(NULL);
        fprintf(file, "%s - %s\n", title, ctime(&now));
        fclose(file);
    }
}

void save_settings(int times[], int size)
{
    FILE *file = fopen("settings.txt", "w");
    if (file)
    {
        for (int i = 0; i < size; i++)
        {
            fprintf(file, "%d\n", times[i]);
        }
        fclose(file);
    }
}

void load_settings(int times[], int size)
{
    FILE *file = fopen("settings.txt", "r");
    if (file)
    {
        for (int i = 0; i < size; i++)
        {
            if (fscanf(file, "%d", &times[i]) != 1)
            {
                break;
            }
        }
        fclose(file);
    }
}

void display_sessions(WINDOW *win, int width, int height)
{
    clear();
    FILE *file = fopen("sessions.txt", "r");
    if (file)
    {
        char line[256];
        int y = 1;
        while (fgets(line, sizeof(line), file))
        {
            mvprintw(y++, 1, "%s", line);
        }
        fclose(file);
    }
    mvprintw(height - 2, 1, "Press any key to return...");
    refresh();
    getch();
    clear();
}

void draw_input_window(WINDOW *win, int width, int height, const char *prompt, char *input, int max_input_length)
{
    int input_win_height = 5;
    int input_win_width = 40;

    int start_y = height / 2 - input_win_height / 2;
    int start_x = width / 2 - input_win_width / 2;

    WINDOW *input_win = newwin(input_win_height, input_win_width, start_y, start_x);
    box(input_win, 0, 0);                     // Draw border
    mvwprintw(input_win, 1, 2, "%s", prompt);
    wrefresh(input_win);

    echo();
    curs_set(1);
    wgetnstr(input_win, input, max_input_length - 1);
    noecho();
    curs_set(0);

    delwin(input_win); // Delete the input window
}
