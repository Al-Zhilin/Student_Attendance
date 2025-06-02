#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h> // для byte

class Time {
  private:
    byte hours, minutes;

  public:
    Time(byte h, byte m);
    Time();
    Time(const Time &object);

    void setHours(byte hours);
    void setMinutes(byte minutes);
    byte getHours() const;
    byte getMinutes() const;

    Time operator +(const Time &other);
    Time operator +(byte &minutes);
    Time operator -(const Time &other);
    Time operator -(byte &minutes);

    bool operator >(const Time &other);
    bool operator <(const Time &other);
    bool operator ==(const Time &other);
    bool operator >=(const Time &other);
    bool operator <=(const Time &other);
};

#endif // TYPES_H
