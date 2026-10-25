#include "BMPwork.h"
#include "Filters.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int ParseInt(const std::string& text, const std::string& parameter) {
    std::size_t pos = 0;
    int value = 0;
    try {
        value = std::stoi(text, &pos);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Параметр " + parameter + " должен быть целым числом: '" + text + "'");
    } catch (const std::out_of_range&) {
        throw std::out_of_range("Параметр " + parameter + " выходит за допустимый диапазон int: '" + text + "'");
    }
    if (pos != text.size()) {
        throw std::invalid_argument("Параметр " + parameter + " должен быть целым числом: '" + text + "'");
    }
    return value;
}

long double ParseLd(const std::string& text, const std::string& parameter) {
    std::size_t pos = 0;
    long double value = 0;
    try {
        value = std::stold(text, &pos);
    } catch (const std::invalid_argument&) {
        throw std::invalid_argument("Параметр " + parameter + " должен быть действительным числом: '" + text + "'");
    } catch (const std::out_of_range&) {
        throw std::out_of_range("Параметр " + parameter + " выходит за допустимый диапазон действительных чисел: '" +
                                text + "'");
    }
    if (pos != text.size()) {
        throw std::invalid_argument("Параметр " + parameter + " должен быть действительным числом: '" + text + "'");
    }
    return value;
}

int main(int argc, char* argv[]) {
    try {
        if (argc <= 1) {
            std::cerr << "Использование: image_processor <входной.bmp> <выходной.bmp> [фильтры]\n"
                         "Формат: 24-битный BMP без сжатия.\n\n"
                         "Фильтры применяются слева направо:\n"
                         "  -crop width height  Обрезать верхнюю левую часть; размеры должны быть положительными.\n"
                         "  -gs                 Перевести в оттенки серого.\n"
                         "  -neg                Применить негатив.\n"
                         "  -sharp              Повысить резкость.\n"
                         "  -edge threshold     Выделить границы с заданным порогом.\n"
                         "  -blur sigma         Размыть по Гауссу; sigma должна быть положительной.\n"
                         "  -crystal            Применить кристаллизацию.\n\n"
                         "Без фильтров сохраняется исходное изображение.\n\n"
                         "Пример:\n"
                         "  image_processor input.bmp output.bmp -crop 800 600 -gs -blur 0.5"
                      << std::endl;
            return 0;
        }
        if (argc == 2) {
            std::cerr << "Нет выходного файла" << std::endl;
            return 1;
        }
        Image inp = ReadBMP(argv[1]);
        int u = 3;
        while (u < argc) {
            std::string arg = argv[u];
            if (arg == "-neg") {
                Negative ng;
                inp = ng.filter(inp);
            } else if (arg == "-gs") {
                Grayscale gs;
                inp = gs.filter(inp);
            } else if (arg == "-sharp") {
                Sharpening sharp;
                inp = sharp.filter(inp);
            } else if (arg == "-crystal") {
                Crystalize beauty;
                inp = beauty.filter(inp);
            } else if (arg == "-crop") {
                if (u + 2 >= argc) {
                    throw std::out_of_range("В crop должно быть ровно 2 аргумента");
                }
                int width = ParseInt(argv[++u], "-crop width");
                int height = ParseInt(argv[++u], "-crop height");
                Crop crop(width, height);
                inp = crop.filter(inp);
            } else if (arg == "-edge") {
                if (u + 1 >= argc) {
                    throw std::out_of_range("В edge должен быть ровно 1 аргумент");
                }
                long double threshold = ParseLd(argv[++u], "-edge threshold");
                EdgeDetection edg(threshold);
                inp = edg.filter(inp);
            } else if (arg == "-blur") {
                if (u + 1 >= argc) {
                    throw std::out_of_range("В blur должен быть ровно 1 аргумент");
                }
                long double sigma = ParseLd(argv[++u], "-blur sigma");
                GaussianBlur bl(sigma);
                inp = bl.filter(inp);
            } else {
                throw std::invalid_argument("Много аргументов или неверный ввод");
            }
            ++u;
        }
        WriteBMP(argv[2], inp);
    } catch (const std::exception& error) {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}
