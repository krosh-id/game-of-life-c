#include <cstdint>
#include <iostream>
#include <memory> // Для работы с умными указателями
#include <fstream> // Для работы с файлами
#include <regex>// Для работы с регулярными выражениями

#include "SDL.h"
using namespace std;

// Определение типа для хранения данных клетки (uint32_t - беззнаковое целое число 32 бита)
typedef unique_ptr<uint32_t[]> cells;

int SCALE = 20;
int WIDTH;
int HEIGHT;
int SIM_SPEED;
int BOARD_SIZE = WIDTH * HEIGHT;
size_t BOARD_SIZE_BYTES = BOARD_SIZE * sizeof(uint32_t);;
uint32_t DEAD = 0x00000000;
uint32_t ALIVE  = 0x00FF0000;

// Чтение параметров из файла конфигурации
void readConfigFile(int& width, int& height, int& sim_speed) {
    ifstream configFile("config.ini");
    if (!configFile.is_open()) {
        cerr << "Ошибка открытия файла конфигурации!" << endl;
        return;
    }

    string line;
    while (getline(configFile, line)) {
        // Удаление пробелов до и после двоеточия
        line = regex_replace(line, regex("[: \t]+"), ":");

        // Разделение строки на ключ и значение
        istringstream iss(line);
        string key, value;
        getline(iss, key, ':');
        getline(iss, value);

        // Преобразование значения в соответствующий тип
        if (key == "width") {
            width = stoi(value);
        } else if (key == "height") {
            height = stoi(value);
        } else if (key == "sim_speed") {
            sim_speed = stoi(value);
        } else {
            cerr << "Неизвестный ключ в файле конфигурации: " << key << endl;
        }
    }
    BOARD_SIZE = WIDTH * HEIGHT;
    BOARD_SIZE_BYTES = BOARD_SIZE * sizeof(uint32_t);
    configFile.close();
}

// Функция для вычисления остатка по модулю с учетом отрицательных чисел
int mod(int a, int b) {
    int ret = a % b;
    if (ret < 0) {
        ret += b;
    }
    return ret;
}

// Функция для получения состояния клетки
uint32_t get_cell(const cells& generation, int x, int y) {
    return generation[y * WIDTH + x];
}

// Функция для установки состояния клетки
void set_cell(cells& generation, int x, int y, uint32_t value) {
    generation[y * WIDTH + x] = value;
}

// Функция для обработки клика мыши по игровому полю
void draw(cells& current_generation, int x, int y) {
    int scaled_x = x/SCALE;
    int scaled_y = y/SCALE;
    uint32_t cell_value = get_cell(current_generation, scaled_x, scaled_y);
    uint32_t next_state = cell_value ? DEAD : ALIVE;
    set_cell(current_generation, scaled_x, scaled_y, next_state);
}

// Функция для получения количества живых соседей клетки
uint32_t neighbors(const cells& current_generation, int x, int y) {
    // Проверка соседних клеток по часовой стрелке
    size_t count = 0; // Счетчик живых соседей

    // Проверка соседних клеток по часовой стрелке
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue; // Пропускаем текущую клетку
            int neighbor_x = mod((x + dx), WIDTH);
            int neighbor_y = mod((y + dy), HEIGHT);
            if (current_generation[neighbor_y * WIDTH + neighbor_x]) {
                count++;
            }
        }
    }

    return count;
}

// Функция для вычисления нового поколения клеток
void new_step_generation(cells& current_generation, cells& next_generation) {
    memcpy(next_generation.get(), current_generation.get(), BOARD_SIZE_BYTES);
    for (int y=0; y<HEIGHT; y++) {
        for (int x=0; x<WIDTH; x++) {
            uint32_t current_cell = current_generation[y * WIDTH + x]; // Состояние текущей клетки
            int live_neighbors = neighbors(current_generation, x, y); // Количество живых соседей

            // Правила игры "Жизнь"
            if (current_cell && !(live_neighbors == 2 || live_neighbors == 3)) {
                next_generation[y * WIDTH + x] = DEAD; // Клетка умирает
            }
            if (!current_cell && live_neighbors == 3) {
                next_generation[y * WIDTH + x] = ALIVE; // Клетка рождается
            }
        }
    }
    swap(current_generation, next_generation); // Обмен текущим и следующим поколениями
}

int main(int argc, char* argv[]) {
    readConfigFile(WIDTH, HEIGHT, SIM_SPEED);
    SDL_Init(SDL_INIT_VIDEO);

    // Создание окна
    SDL_Window *window = SDL_CreateWindow(
        "Игра 'Жизнь'",
        100,
        100,
        WIDTH * SCALE,
        HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1, // Индекс драйвера (-1 - автоматический выбор)
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    SDL_RenderClear(renderer);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888, // формат пикселей
        SDL_TEXTUREACCESS_STREAMING, // тип доступа к текстуре
        WIDTH,
        HEIGHT
    );

    unique_ptr<uint32_t[]> current_generation(new uint32_t[BOARD_SIZE]);
    unique_ptr<uint32_t[]> next_generation(new uint32_t[BOARD_SIZE]);

    // Очистка массивов
    memset(current_generation.get(), 0, BOARD_SIZE_BYTES);
    memset(next_generation.get(), 0, BOARD_SIZE_BYTES);

    // Флаги состояния игры
    bool quit = false;
    bool drawing = false;
    bool is_running = false; // Флаг состояния игры (запущена/пауза)

    SDL_Event event;

    while (!quit) {
        SDL_WaitEvent(&event);

        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN: // Нажатие кнопки мыши
                draw(current_generation, event.button.x, event.button.y);
                drawing = true;
                break;
            case SDL_MOUSEMOTION: // Перемещение мыши
                if (event.motion.state & SDL_BUTTON(1)) {
                    draw(current_generation, event.motion.x, event.motion.y);
                    drawing = true;
                }
                break;
            case SDL_MOUSEBUTTONUP: // Отпускание кнопки мыши
                drawing = false;
                break;
            case SDL_QUIT: // Закрытие окна
                quit = true;
                break;
            case SDL_KEYDOWN: // Нажатие клавиши
                if (event.key.keysym.sym == SDLK_q) {
                    quit = true;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (is_running) {
                        is_running = false;
                        drawing = true;
                    } else {
                        is_running = true;
                    }
                }
                break;
            default:
                break;
        }

        // Отрисовка игрового поля
        if (drawing) {
            SDL_UpdateTexture(texture, nullptr, current_generation.get(), WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            drawing = false;
        }

        // Вычисление нового поколения клеток
        if (is_running) {
            new_step_generation(current_generation, next_generation);

            // Задержка между генерациями
            SDL_Delay(SIM_SPEED);
            drawing = true;
        }
    }

    // Очистка ресурсов SDL
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}