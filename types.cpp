#include "types.h"

  Time::Time(byte h, byte m) : hours(h), minutes(m) {};
  Time::Time() : Time(0, 0) {};
  Time::Time(const Time &object) : hours(object.getHours()), minutes(object.getMinutes()) {};

  void Time::setHours(byte hours) {this->hours = hours;}
  void Time::setMinutes(byte minutes) {this->minutes = minutes;}
  byte Time::getHours() const {return hours;}
  byte Time::getMinutes() const {return minutes;}

  Time Time::operator +(const Time &other)  {
    int total_min = (this->hours + other.getHours())*60 + (this->minutes + other.getMinutes());
    Time ret((total_min / 60) % 24, total_min % 60);
    return ret;
  }

  Time Time::operator +(byte &minutes) {
    Time ret(this->hours + (this->minutes + minutes)/60, (this->minutes + minutes) % 60);
    return ret;
  }

  Time  Time::operator -(const Time &other) {
    int total_min1, total_min2;
    total_min1 = this->minutes + 60*this->hours;
    total_min2 = other.getMinutes() + 60*other.getHours();

    if (total_min1 - total_min2 < 0)  total_min1 += 86400;
    Time ret((total_min1 - total_min2) / 60, (total_min1 - total_min2) % 60);
    return ret;
  }

  Time Time::operator -(byte &minutes) {
    int total_mins = this->minutes + this->hours*60 - minutes;
    if (total_mins < 0) total_mins += 86400;
    Time ret(total_mins / 60, total_mins % 60);
    return ret;
  }

  bool Time::operator >(const Time &other) {
    if (this->hours > other.getHours())  return true;
    if (this->hours < other.getHours())  return false;
    return this->minutes > other.getMinutes();
  }

  bool Time::operator <(const Time &other)  {return !(*this > other);}
  bool Time::operator ==(const Time &other) {return (this->hours == other.getHours() || this->minutes == other.getMinutes());}
  bool Time::operator >=(const Time &other)  {return !(*this < other);}
  bool Time::operator <=(const Time &other) {return !(*this > other);}
