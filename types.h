#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

class Time {
private:
    uint8_t hours;
    uint8_t minutes;

public:
    Time(uint8_t h, uint8_t m);
    Time();
    Time(const Time &object);

    void setHours(uint8_t hours);
    void setMinutes(uint8_t minutes);
    uint8_t getHours() const;
    uint8_t getMinutes() const;

    Time operator +(const Time &other) const;
    Time operator +(uint8_t minutes) const;
    Time operator -(const Time &other) const;
    Time operator -(uint8_t minutes) const;

    bool operator >(const Time &other) const;
    bool operator <(const Time &other) const;
    bool operator ==(const Time &other) const;
    bool operator >=(const Time &other) const;
    bool operator <=(const Time &other) const;
};

#endif