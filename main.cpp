#include <ncurses.h>
#include <string>
#include <vector>
#include <fstream>

struct Task {
    std::string text;
    bool done = false;
};

struct TaskData {
    std::vector<Task> daily, weekly, monthly;
};

TaskData tasks;
int cursor_line = 0;

void load_tasks() {
    std::ifstream in("tasks.dat", std::ios::binary);
    if (!in) return;

    auto read_vec = [&](std::vector<Task>& vec) {
        size_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        vec.resize(size);
        for (auto& task : vec) {
            size_t len;
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            task.text.resize(len);
            in.read(&task.text[0], len);
            in.read(reinterpret_cast<char*>(&task.done), sizeof(task.done));
        }
    };

    read_vec(tasks.daily);
    read_vec(tasks.weekly);
    read_vec(tasks.monthly);
    in.close();
}

void save_tasks() {
    std::ofstream out("tasks.dat", std::ios::binary | std::ios::trunc);

    auto write_vec = [&](const std::vector<Task>& vec) {
        size_t size = vec.size();
        out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto& task : vec) {
            size_t len = task.text.size();
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            out.write(task.text.data(), len);
            out.write(reinterpret_cast<const char*>(&task.done), sizeof(task.done));
        }
    };

    write_vec(tasks.daily);
    write_vec(tasks.weekly);
    write_vec(tasks.monthly);
    out.close();
}

enum TaskSection { HEADER, DAILY, WEEKLY, MONTHLY };

struct CursorMap {
    TaskSection section;
    int index; // index in the task list (-1 for headers)
};

std::vector<CursorMap> build_cursor_map() {
    std::vector<CursorMap> map;
    map.push_back({HEADER, -1});  // === Daily ===
    for (size_t i = 0; i < tasks.daily.size(); ++i)
        map.push_back({DAILY, (int)i});
    map.push_back({HEADER, -2});  // === Weekly ===
    for (size_t i = 0; i < tasks.weekly.size(); ++i)
        map.push_back({WEEKLY, (int)i});
    map.push_back({HEADER, -3});  // === Monthly ===
    for (size_t i = 0; i < tasks.monthly.size(); ++i)
        map.push_back({MONTHLY, (int)i});
    return map;
}

std::string prompt_input(const std::string& prompt, const std::string& initial = "") {
    echo();
    curs_set(1);
    char input[256];
    mvprintw(LINES - 2, 0, "%s: ", prompt.c_str());
    clrtoeol();
    mvgetnstr(LINES - 2, prompt.size() + 2, input, 255);
    noecho();
    curs_set(0);
    return std::string(input);
}

void draw_screen(const std::vector<CursorMap>& map) {
    clear();
    mvprintw(0, 0, "Minimal Task Manager | [a] Add  [e] Edit  [d] Delete  [↑↓] Move  [Space] Toggle  [q] Quit");

    int line = 2;
    for (size_t i = 0; i < map.size(); ++i) {
        const auto& entry = map[i];

        if (i == (size_t)cursor_line)
            attron(A_REVERSE);

        if (entry.section == HEADER) {
            if (entry.index == -1)
                mvprintw(line, 0, "=== Daily Tasks ===");
            else if (entry.index == -2)
                mvprintw(line, 0, "=== Weekly Tasks ===");
            else
                mvprintw(line, 0, "=== Monthly Tasks ===");
        } else {
            const Task* task = nullptr;
            if (entry.section == DAILY) task = &tasks.daily[entry.index];
            if (entry.section == WEEKLY) task = &tasks.weekly[entry.index];
            if (entry.section == MONTHLY) task = &tasks.monthly[entry.index];

            mvprintw(line, 2, "[%c] %s", task->done ? 'x' : ' ', task->text.c_str());
        }

        if (i == (size_t)cursor_line)
            attroff(A_REVERSE);

        ++line;
    }

    refresh();
}

void add_task(TaskSection section) {
    std::string input = prompt_input("Add Task");
    if (input.empty()) return;
    Task new_task{input, false};
    if (section == DAILY) tasks.daily.push_back(new_task);
    if (section == WEEKLY) tasks.weekly.push_back(new_task);
    if (section == MONTHLY) tasks.monthly.push_back(new_task);
}

void delete_task(TaskSection section, int index) {
    if (index < 0) return;
    if (section == DAILY && index < (int)tasks.daily.size()) tasks.daily.erase(tasks.daily.begin() + index);
    if (section == WEEKLY && index < (int)tasks.weekly.size()) tasks.weekly.erase(tasks.weekly.begin() + index);
    if (section == MONTHLY && index < (int)tasks.monthly.size()) tasks.monthly.erase(tasks.monthly.begin() + index);
}

void edit_task(TaskSection section, int index) {
    if (index < 0) return;
    Task* task = nullptr;
    if (section == DAILY && index < (int)tasks.daily.size()) task = &tasks.daily[index];
    if (section == WEEKLY && index < (int)tasks.weekly.size()) task = &tasks.weekly[index];
    if (section == MONTHLY && index < (int)tasks.monthly.size()) task = &tasks.monthly[index];
    if (!task) return;

    std::string edited = prompt_input("Edit Task", task->text);
    if (!edited.empty()) task->text = edited;
}

void toggle_task(TaskSection section, int index) {
    Task* task = nullptr;
    if (section == DAILY && index < (int)tasks.daily.size()) task = &tasks.daily[index];
    if (section == WEEKLY && index < (int)tasks.weekly.size()) task = &tasks.weekly[index];
    if (section == MONTHLY && index < (int)tasks.monthly.size()) task = &tasks.monthly[index];
    if (task) task->done = !task->done;
}

int main() {
    load_tasks();
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    std::vector<CursorMap> cursor_map = build_cursor_map();
    draw_screen(cursor_map);

    int ch;
    while ((ch = getch()) != 'q') {
        cursor_map = build_cursor_map();
        if (cursor_line >= (int)cursor_map.size())
            cursor_line = cursor_map.size() - 1;

        CursorMap entry = cursor_map[cursor_line];

        switch (ch) {
            case KEY_UP:
                if (cursor_line > 0) cursor_line--;
                break;
            case KEY_DOWN:
                if (cursor_line < (int)cursor_map.size() - 1) cursor_line++;
                break;
            // === inside the main loop’s switch … case ===
            case 'a': {
                TaskSection target = entry.section;

        /* ★ Allow “Add” on headers ― map header ID → section ★ */
                if (entry.section == HEADER) {
                if (entry.index == -1)      target = DAILY;
                else if (entry.index == -2) target = WEEKLY;
                else                        target = MONTHLY;
                }
                add_task(target);
                break;
            }
            case 'd':
                if (entry.section != HEADER) delete_task(entry.section, entry.index);
                break;
            case 'e':
                if (entry.section != HEADER) edit_task(entry.section, entry.index);
                break;
            case ' ':
                if (entry.section != HEADER) toggle_task(entry.section, entry.index);
                break;
        }

        draw_screen(cursor_map);
    }

    save_tasks();
    endwin();
    return 0;
}

