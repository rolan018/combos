#include "speed_converter.hpp" // Подключаем объявление нашей функции
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream> // Для отладочных сообщений, если нужно

double convertSpeedToBps_NoSpace(const std::string& speedStr) {
    if (speedStr.empty()) {
        throw std::invalid_argument("Input string is empty.");
    }

    size_t split_pos = 0;
    
    // 1. Найти позицию, где заканчивается числовая часть
    // Ищем первый символ, который не является цифрой, точкой или знаком минуса
    for (size_t i = 0; i < speedStr.length(); ++i) {
        char c = speedStr[i];
        if (!std::isdigit(c) && c != '.' && c != '-' && c != '+') {
            split_pos = i;
            break;
        }
        // Если дошли до конца строки, значит, это просто число
        if (i == speedStr.length() - 1) {
             split_pos = speedStr.length();
        }
    }

    if (split_pos == 0 || split_pos == speedStr.length()) {
        // Если число не отделено от единицы, или это просто число
        // В данном случае мы предполагаем, что если число и текст слились,
        // ищем разделитель (например, 'G', 'M', 'K')
        // Для надежности, лучше всего использовать robust parsing
        // Но для простоты примера:
        
        // Если нашли символ, который не является частью числа, но мы не нашли пробела
        // Попробуем извлечь число до первого нецифрового символа, кроме '.'
        for (size_t i = 0; i < speedStr.length(); ++i) {
             char c = speedStr[i];
             if (!std::isdigit(c) && c != '.') {
                 split_pos = i;
                 break;
             }
        }
        
        if (split_pos == 0) {
             throw std::invalid_argument("Cannot separate numeric value and unit.");
        }
    }
    
    // 2. Извлечение числовой части и единицы измерения
    std::string valueStr = speedStr.substr(0, split_pos);
    std::string unit = speedStr.substr(split_pos);
    
    // 3. Преобразование числа
    double value;
    try {
        value = std::stod(valueStr);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Error converting numeric part: " + valueStr);
    }

    // 4. Нормализация и расчет (как и раньше)
    unit.erase(std::remove_if(unit.begin(), unit.end(), ::isspace), unit.end());
    std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);

    double multiplier = 1.0;
    
    // Ищем только префиксы (G, M, K)
    if (!unit.empty()) {
        char prefix = unit[0];
        if (prefix == 'G') {
            multiplier = 1000000000.0;
        } else if (prefix == 'M') {
            multiplier = 1000000.0;
        } else if (prefix == 'K') {
            multiplier = 1000.0;
        } else if (prefix == 'T') {
            multiplier = 1000000000000.0;
        } else if (unit == "BPS" || unit == "B/S") {
            multiplier = 1.0;
        }
        else {
             throw std::invalid_argument("Unknown unit prefix or suffix: " + unit);
        }
    }
    
    return value * multiplier;
}