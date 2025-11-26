#include "types.h"  // Заголовок, где объявлен класс Time
#define MIN_FREE_HEAP 50                                                                       //минимальный объем свободной оперативной памяти во время выполнения программы (порог нужен для оценки при мониторинге), кБ

// Конструкторы
Time::Time(byte h, byte m) : hours(h), minutes(m) {}

Time::Time() : Time(0, 0) {}

Time::Time(const Time &object)
    : hours(object.getHours()), minutes(object.getMinutes()) {}

// Сеттеры
void Time::setHours(byte hours) {
    this->hours = hours;
}

void Time::setMinutes(byte minutes) {
    this->minutes = minutes;
}

// Геттеры
byte Time::getHours() const {
    return hours;
}

byte Time::getMinutes() const {
    return minutes;
}

// Арифметические операторы
Time Time::operator +(const Time &other) const {
    int total_min = (this->hours + other.getHours()) * 60 + (this->minutes + other.getMinutes());
    return Time((total_min / 60) % 24, total_min % 60);
}

Time Time::operator +(byte minutes) const {
    int total_minutes = this->hours * 60 + this->minutes + minutes;
    return Time((total_minutes / 60) % 24, total_minutes % 60);
}

Time Time::operator -(const Time &other) const {
    int total_min1 = this->hours * 60 + this->minutes;
    int total_min2 = other.getHours() * 60 + other.getMinutes();

    int diff = total_min1 - total_min2;
    if (diff < 0) diff += 24 * 60;

    return Time((diff / 60) % 24, diff % 60);
}

Time Time::operator -(byte minutes) const {
    int total_mins = this->hours * 60 + this->minutes - minutes;
    if (total_mins < 0) total_mins += 24 * 60;

    return Time((total_mins / 60) % 24, total_mins % 60);
}

// Операторы сравнения
bool Time::operator >(const Time &other) const {
    if (this->hours > other.getHours()) return true;
    if (this->hours < other.getHours()) return false;
    return this->minutes > other.getMinutes();
}

bool Time::operator <(const Time &other) const {
    return !(*this > other) && !(*this == other);
}

bool Time::operator ==(const Time &other) const {
    return (this->hours == other.getHours()) && (this->minutes == other.getMinutes());
}

bool Time::operator >=(const Time &other) const {
    return (*this > other) || (*this == other);
}

bool Time::operator <=(const Time &other) const {
    return !(*this > other);
}

bool Time::operator !=(const Time &other) const {
    return !(*this == other);
}


MemoryControl::MemoryControl() {
    _start_heap = ESP.getFreeHeap();
    _keep_heap = 0;
}

bool MemoryControl::check() {                    // проверяем свободное количество
    _keep_heap = max(_keep_heap, ESP.getFreeHeap());            // и обновляем максимальное занимаемой пространство памяти
    if (ESP.getFreeHeap() < MIN_FREE_HEAP * 1024)  return false;
    return true;
}

uint32_t MemoryControl::getHeap(bool mode) {     // false - start_heap; true - current heap
    if (!mode) return _start_heap;
    return _keep_heap;
}

uint32_t MemoryControl::totalHeap() {            // общий размер памяти
    return ESP.getHeapSize();
}