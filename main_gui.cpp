#include "simple_window.h"
#include <iostream>
#include <exception>

int main() {
    try {
        SimpleWindow window;
        
        if (!window.initialize()) {
            std::cerr << "Ошибка инициализации графического интерфейса" << std::endl;
            std::cerr << "Убедитесь, что у вас установлен системный шрифт Arial" << std::endl;
            return 1;
        }
        
        window.run();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Неизвестная критическая ошибка" << std::endl;
        return 1;
    }
}