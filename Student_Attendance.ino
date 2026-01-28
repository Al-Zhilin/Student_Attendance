#define ATOMIC_FS_UPDATE      // поддержка сжатых прошивок из чата

#include <FastBot.h>
#include <FileData.h>
#include <Stamp.h>
#include <FFat.h>
#include <ESP_Google_Sheet_Client.h>
#include <StringUtils.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>          // в какой-то момент без этого явного включения компилятор начал ругаться на отсутствие либы
#include <settings.h>
#include "types.h"

FastBot bot(BOT_TOKEN);
                                                   
float Version = 0.7;                                              // текущая версия прошивки

struct week_diapason {
  byte start = 0;
  byte end = 0;
}; 

struct Settings {
  week_diapason att_diapason;                                     // диапазон недель промежуточной аттестации
  uint8_t table_width[2] = {10, 10};                              // ширина таблицы (текущей и противоположной четности) в количестве столбцов (не считая столбец с фамилиями). После первого чтения новой таблицы обновиться до актуального значения
} settings;

struct fileData {                                                 // структуры настроек, записывамых в энергонезависимую память
  int32_t status_mess[sizeof(Admins)/sizeof(Admins[0])] = {};     // id статусного сообщения в каждом чате
  int32_t menu_id[sizeof(Admins)/sizeof(Admins[0])] = {};         // id меню в каждом чате
} chat_settings;

// номер текущей недели (считая от первой недели в таблице, не от первой недели в году!):
byte week_off = 1;  // НЕ ЗНАЕШЬ - НЕ МЕНЯЙ! О последствиях можно сильно пожалеть!!
FileData week_file(&FFat, "/weekdata.dat", 'Z', &week_off, sizeof(week_off));   // ЗДЕСЬ ТОЖЕ НЕ ТРОГАТЬ!!
FileData chat_file(&FFat, "/data.dat", 'Z', &chat_settings, sizeof(chat_settings));
FileData settings_file(&FFat, "/settings.dat", 'Z', &settings, sizeof(settings)); 

const String months[] PROGMEM = {               //сокращенные названия всех месяцев для отображения в меню
  "Янв",
  "Фев",
  "Мар",
  "Апр",
  "Май",
  "Июн",
  "Июл",
  "Авг",
  "Сен",
  "Окт",
  "Ноя",
  "Дек",
};

uint8_t getDayInMonth(uint8_t month, uint16_t year) {                     // year нужен для проверки високосности февраля. Нумерация месяцев: 0...11
  byte day_month[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 1) return day_month[month] + ((StampUtils::isLeap(year)) ? 1 : 0);         // учитываем возможные 29 дней февраля
  return day_month[month];
}

struct Date {
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;

  Date(uint8_t dday, uint8_t mmonth, uint16_t yyear) : day(dday), month(mmonth), year(yyear) {
    if (mmonth < 1 || mmonth > 12) {
      bot.sendMessage(F("InvalidMonthInDateConstructor!"), error_chat);
      month = 1;
    }
    if (yyear < 1 || yyear > 4096) {
      bot.sendMessage(F("InvalidYearInDateConstructor!"), error_chat);
      year = 1970;
    }
    if (dday < 1 || dday > getDayInMonth(mmonth, year)) {
      bot.sendMessage(F("InvalidDayInDateConstructor!"), error_chat);
      day = 1;
    }
  }

  Date() : day(1), month(1), year(1970) {};
  
  void operator= (const Date& other) {                // перегружаем присваивание
    this->day = other.day;
    this->month = other.month;
    this->year = other.year;
  }
};

void sumDate(Date *date, int day_offset);

String PROGMEM DaysOfWeek[] = {
  "Понедельник",
  "Вторник",
  "Среда",
  "Четверг",
  "Пятница",
  "Суббота",
  "Воскресенье",
};

struct SetInfo {    //структура с данными, нужными для выставления/изменения конкретной Н-ки и/или массива Нок. В обоих случаях используем эту структуру
  String surn;          //фамилия человека
  String nki;           //строка, в которой каждый индекс строки обозначает тип пропуска, соответственно каждой паре выбранного дня
  Date date;            //дата выставления Нки
  uint8_t dayWeek;      //день недели (1-7 / понедельник-воскресенье)
  String posC;          //символьная составлющая координаты ячейки
  uint16_t posI;             //численная составляющая координаты ячейки
  bool subgroup;        //подгруппа (false/true, 1/2 соответственно)
  bool parity;          //четность/нечетность (0/1 соответственно) недели, в которой ставим Нку
} nka;

struct WeekInfo {
  Date pon_date;                                     //дата понедельника этой недели
  uint8_t study_days = 0;                            //количество учебных дней в неделе  (week_info_c; week_info_i) после /
  uint8_t subj_num[7] = {};                          //кол-во пар в учебных днях (less_mun_c; less_num_i)......
  uint8_t less_nums[7][MAX_LESSONS_IN_DAY] = {};      //номера всех пар в дне
  bool parity;             //четная/нечетная (true/false соответственно) эта неделя  (week_info_c; week_info_i) перед /

} week_object[2];      //0 - неделя у 1 подгруппы, 1 - неделя 2 подгруппы

WeekInfo *week[2] = {&week_object[0], &week_object[1]};           //week[2] - массив указателей на обьекты структуры WeekInfo. 0 - настоящая, 1 - противоположная четность недели. Такое объявление нужно для удобного свайпа указателей при смене четности

byte CheckSurnameMatch(String s_input, String s_list, byte* syntax_errors, byte max_errors = SURNAME_ERRORS_NUM);

struct CountInfo {
  String surn;
  int surn_ind;
  int total;
  bool subgroup;
  String subject;
  byte mode;      //0 -  все предметы УП, 1 - все предметы неУП, 2 - по отдельным предметам неУП
} count;

struct timer_data {
  uint32_t start_millis = 0;
  int32_t message_id = 0;
  uint16_t period = 0;            //в секундах
  char chat_id[15] = "";
};

class DeleteTimer {
  private:
  byte timer_size = 0;
  timer_data *ptr = nullptr;

  public:
  ~DeleteTimer() {                      //мало ли
    if (ptr != nullptr) {
      free(ptr);
      ptr = nullptr;
    }
  }

  void add(int32_t message_id, uint16_t period, String StringChatId) {
    if (timer_size+1 > 255)  return;                   //проверка на переполнения счетчика сообщений, обрабатываемых таймером
    if (StringChatId.length() >= sizeof(ptr[timer_size-1].chat_id))  {
      bot.sendMessage(F("В массиве стуктыры обьекта для таймера не хватает места для записи этого chat_id!\nНе удалось добавить новый обьект!"), error_chat);
      return;
    }

    timer_data *temp = (timer_data *)realloc(ptr, (++timer_size)*sizeof(timer_data));           //выделяем память под данные нового таймера
    if (temp == nullptr)  return;                 //проверка на успешность перераспределения памяти
    ptr = temp;                                     //после проверки можно вернуть на место указатель на массив с данными
    
    ptr[timer_size-1].period = period;
    ptr[timer_size-1].start_millis = millis();
    ptr[timer_size-1].message_id = message_id;                                
    StringChatId.toCharArray(ptr[timer_size-1].chat_id, sizeof(ptr[timer_size-1].chat_id));
  }

  void tick() {
    static uint32_t tick_timer = millis(), tick_period = 200;         //период проверки сработки таймеров сделаем 200 мс
    
    if (millis() - tick_timer >= tick_period) {
      tick_timer = millis();
      bool need_delete = false;
      for (byte i = 0; i < timer_size; i++) {
        if (millis() - ptr[i].start_millis >= ptr[i].period*1000) {
          bot.deleteMessage(ptr[i].message_id, String(ptr[i].chat_id));
          ptr[i].message_id = -1;
          need_delete = true;
        }
      }
      if (need_delete)  MyRealloc();
    }
  }

  void MyRealloc() {            //очищаем массив от данных обьектов, которые уже сработали
    byte delete_num = 0;

    if (ptr == nullptr)   return;           //пропишем и эту ситуацию на всякий

    for (byte i = 0; i < timer_size; i++) {                 //узнаем, сколько элементов надо удалить
      if (ptr[i].message_id == -1)  delete_num++;
    }

    if (delete_num == timer_size) {         //если нужно удалить все элементы - удаляем, делаем указатель nullprt и выходим, в таком случае больше ничего не надо делать
      free(ptr);
      ptr = nullptr;
      timer_size = 0;
      return;
    }

    byte supp = 0;
    timer_data *new_ptr = (timer_data*) calloc((timer_size-delete_num), sizeof(timer_data));

    if (new_ptr == nullptr)   return;             //не удалось выделить память под новый массив        

    for (byte i = 0; i < timer_size; i++) {
      if (ptr[i].message_id == -1)  supp++;
      else  new_ptr[i-supp] = ptr[i];
    }

    free(ptr);
    ptr = new_ptr;
    timer_size -= delete_num;
  }
} timer;

class ServiceMess {
  private:
    bool need_clear = false;
    uint32_t delete_period = 0;
    uint32_t start_millis = 0;

  public:
    void edit(String edit_text, uint32_t del_period = 0) {                  // функция редактирует сервисное сообщение, а при передаче дополнительного параметра - очищает его через timeout
      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        bot.editMessage(chat_settings.status_mess[i], "ИСиТенок v" + String(Version, 1) + ((edit_text != "") ? "\n\n" : "") + edit_text, Admins[i]);
      }

      if (del_period) {
        need_clear = true;
        delete_period = del_period;
        start_millis = millis();
      }
    }

    void tick() {
      if (!need_clear) return;

      if (millis() - start_millis >= delete_period) {
        this->edit("");
        need_clear = false;
      }
    }

} serviceMess;

FB_Time realTime;                            //структура реального времени

class Sheet {
  private:

  public:
    void begin() {
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
      GSheet.setPrerefreshSeconds(10 * 60);
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

      serviceMess.edit("Подключаюсь к Google Sheet API...");

      uint32_t reset_timer = millis();
      //digitalWrite(2, true);
      while (!(this->ready()))  {
        ArduinoOTA.handle();
        static uint8_t tryes_num = 0;
        if (millis() - reset_timer >= 60*100) {
          tryes_num++;

          if (tryes_num == GSHEET_CONNECT_ATT) {
            bot.sendMessage(F("Вышел таймаут ожидания подключения к GoogleSheet! Перезагружаюсь..."), error_chat);
            ESP.restart();
          }

          bot.sendMessage("Попытка подключения к GSheet " + String(tryes_num) + "/" + GSHEET_CONNECT_ATT, error_chat);
        }

      }
      //digitalWrite(2, false);

      serviceMess.edit("Google Sheet API успешно подключено!\nПолучаю информацию о текущей неделе...");

      for (byte i = 0; i < 2; i++) {             // парсим данные о текущей и предыдущей неделе
        String range = "";
        FirebaseJson returned_json;
        FirebaseJsonData cell;
        int8_t parity_iter = ((i == 1 && week_off == 1) ? -1 : 1);          // надобный сдвиг с обработкой чтения будущей недели при week_off = 1 вместо прошлой
      
        //------------Получаем четность с заглавной ячейки недели-------------
        range += SheetName;
        range += weekInfo_c;
        range += (weekInfo_i + (offset*(week_off-i-parity_iter)));
        range += ":";
        range += charOffset(String(weekInfo_c), 1);                    // тем самым однам запросом прихватываем и ячейку "Понедельник, 22.10" для следующего этапа
        range += (weekInfo_i + (offset*(week_off-i-parity_iter)));
        this->getCells(returned_json, range);

        returned_json.get(cell, "values/[0]/[0]");
        if (cell.success) {         // данные присутствуют
          if (cell.stringValue == WEEK_PARITY_NAME) week[i]->parity = false;
          else week[i]->parity = true;
        }

        else {
          bot.sendMessage(F("Ошибка парсинга заглавное ячейки недели!"), error_chat);
          CriticalError();
        }
        //------------Получаем четность с заглавной ячейки недели-------------


        //----------------------Дата понедельника этой недели---------------------------
        returned_json.get(cell, "values/[0]/[1]");
        if (cell.success) {                         // Дата присутствует в ячейке
          String firstDayName = cell.stringValue.substring(0, cell.stringValue.indexOf(","));      // имя первого дня этой недели (может быть не понедельник)
          String rawDate = cell.stringValue.substring(cell.stringValue.indexOf(",")+1);            // дата в сыром формате: в строке, возможны разные представления: 1.2, 12.2, 1.12, 12.11
          
          int8_t firstDot = rawDate.indexOf(".");
          int8_t secondDot = rawDate.lastIndexOf(".");

          if (firstDot != -1 && secondDot != -1 && firstDot != secondDot) {                        // условие корректности формата даты в ячейке (X.Y.Z)
            week[i]->pon_date.day = rawDate.substring(0, firstDot).toInt();
            week[i]->pon_date.month = rawDate.substring(firstDot + 1, secondDot).toInt();
            uint16_t parsedYear = rawDate.substring(secondDot + 1).toInt();

            if (parsedYear < 100) {                                                                // если в таблице год указан неполно (26 вместо 2026) - достараиваем недостающую часть
                week[i]->pon_date.year = (realTime.year / 100) * 100 + parsedYear;
            } else {
                week[i]->pon_date.year = parsedYear;
            }
          }

          else {
            bot.sendMessage(F("Структура заглавной ячейки недели некорректна!"), error_chat);
            CriticalError();
          }
          

          if (firstDayName != "понедельник" && firstDayName != "Понедельник") {                  //непонятно, нужна ли эта фигня №2       !!!Переделать с помощью enum дней недели!!!
            if (firstDayName == "вторник" || firstDayName == "Вторник")  week[i]->pon_date.day--;
            else if (firstDayName == "среда" || firstDayName == "Среда") week[i]->pon_date.day-=2;
            else if (firstDayName == "четверг" || firstDayName == "Четверг") week[i]->pon_date.day-=3;
            else if (firstDayName == "пятница" || firstDayName == "Пятница") week[i]->pon_date.day-=4;
            else if (firstDayName == "суббота"  || firstDayName == "Суббота") week[i]->pon_date.day-=5;
            else if (firstDayName == "воскресенье" || firstDayName == "Воскресенье") week[i]->pon_date.day-=6;
            else {
              bot.sendMessage("Неизвестное имя дня недели обнаружено в диапазоне данных первого учебного дня недели: \"" + firstDayName, error_chat);
            }
          }
        }

        else bot.sendMessage(F("Ошибка получения даты и имени первого учебного дня недели!"), error_chat);
        //----------------------Дата понедельника этой недели---------------------------


        //------------Получаем количество пар в каждый день и их номера, а так же количество учебных дней------------------
        // Формируем: week[i]->study_days                               кол-во учебных дней
        //            week[i]->subj_num[7]                              количество пар в каждый день
        //            week[i]->less_nums[7][MAX_LESSON_IN_DAY]          номера всех пар в каждый день
        uint8_t real_width = 0;
        range = SheetName;
        range += less_num_c;
        range += less_num_i;
        range += ":";
        range += charOffset(String(less_name_c), settings.table_width[i]+2);           // после первого чтения новой таблицы система запомнит ее ширину и будет гарантированно укладываться в один запрос
        range += less_name_i;                                                          // +2 нужно, чтобы понимать, что конец прочитанного диапазона - реально конец недели (ищем 2 пустые ячейки подряд)

        this->getCells(returned_json, range);                // получаем данные
        String adasd = "";
        returned_json.toString(adasd, true);
        
        char path[20];
        uint8_t current_day = 0, current_lesson = 0;         // значения текущих используемых индексов в массиве дней и занаятий, который сейчас заполняем
        uint8_t read_offset = 0;                             // переменная для сдвига диапазона читаемой таблицы
        bool empty_prev = false;                             // показывает, была ли предыдущая ячейка пустой

        for (int8_t path_iter = 0; path_iter < settings.table_width[i]; path_iter++) {             //+2 нужно чтобы корректно захватить 2 пустые строки после окончания недель
          snprintf(path, sizeof(path), "values/[0]/[%d]", path_iter);
          FirebaseJsonData cell;
          returned_json.get(cell, path);

          real_width++;
          
          if (cell.stringValue == "") {
            if (empty_prev) break;             // два пропуска подряд, значит дни закончились!

            empty_prev = true;
            week[i]->study_days++;
            if (++current_day > 6) {
              bot.sendMessage(F("Форматирование таблицы соответствует некорректному значению дней в неделе!"), error_chat);
              CriticalError();
            }
            current_lesson = 0;
            if (path_iter != settings.table_width[i]+1) continue;
          }

          else {
            empty_prev = false;                 // если нашли данные - сбрасываем флаг пустой ячейки

            week[i]->subj_num[current_day]++;                                       // кол-во пар в каждый день
            week[i]->less_nums[current_day][current_lesson] = cell.intValue;        // номера каждой пары в каждый день

            if (++current_lesson >= MAX_LESSONS_IN_DAY) {
              bot.sendMessage("Количество пар в " + DaysOfWeek[current_day] + " превышает установленный лимит! Измените настройки!", error_chat);
              CriticalError();
            }
          }
          if (path_iter == settings.table_width[i]+1) {                // если сработало это условие: мы гарантированно дошли до конца прочитанного обьема данных, но так и не нашли конец недели --> читаем еще пачку
            range = SheetName;
            range += charOffset(String(less_name_c), settings.table_width[i] + read_offset);
            range += less_num_i;
            range += ":";
            range += charOffset(String(less_name_c), settings.table_width[i] + (read_offset += settings.table_width[i]));
            range += less_name_i;

            path_iter = -1;                               // обновили переменную, чтобы начать новый массив данных С НАЧАЛА (-1 нужно чтобы скомпенсировать path_iter++, который цикл автоматически сделает перед следующей итерацией)
            this->getCells(returned_json, range);         // получили новые данные и продолжаем идти именно по ним
          }
        }
        real_width-=2;

        bot.sendMessage("Учебных дней: " + String(week[i]->study_days), error_chat);
        bot.sendMessage("Пары в каждый день:", error_chat);
        for (uint8_t days = 0; days < 7; days++) {
          bot.sendMessage("Всего пар в " + String(days+1) + " день: " + String(week[i]->subj_num[days]), error_chat);
          for (uint8_t p_days = 0; p_days < week[i]->subj_num[days]; p_days++) {
            bot.sendMessage("----- " + String(week[i]->less_nums[days][p_days]), error_chat);
          }
        }
        CriticalError();

        // читаем строку
        // идемм по ней с помощью функций хождения по json
        // по алгоритму заполняем нужные нам (указанные выше) данные
        // если json кончился, а предпосылки на данные есть - читаем еще кусок фиксированной длины
        // не забываем обновлять real_width, чтобы оптимизировать процесс поиска (причем для каждой четности неделя может быть разной длины -> учитываем)
        // как только встретили 2 пустые ячейки подряд - наша остановочка (конец недели)


        if (real_width != settings.table_width[i]) {
          settings.table_width[i] = real_width;
          settings_file.update();
        }
        //------------Получаем количество пар в каждый день и их номера, а так же количество учебных дней------------------
      }
      
      //checkTableWeek();                                                 //проверяем неделю на актуальность
    }


      void getCells(FirebaseJson &answ, const String &range) {                 // функция получения Нок из таблицы (чтобы в меню отображать)
        byte tries = 0;
        answ.clear();
        while (!GSheet.values.get(&answ, spreadsheetId, range) && tries < GetTryNum) {
          tries++;
      }

      if (tries == GetTryNum) bot.sendMessage("getError", error_chat);
    }

    void SetN(const String &range) {                       // базовая функция постановки Нок для одного человека в один день
      String answ = "";
      byte tries = 0;

      FirebaseJson valueRange;
      valueRange.add("range", range);
      valueRange.add("majorDimension", "ROWS");

      for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) {
        String address = "values/[0]/[", data = "";
        address += i;
        address += "]";
        if (nka.nki[i] == ' ')  data = PRESENCE_SYMBOL;
        else if (nka.nki[i] == '+') data = RESPECT_SYMBOL;
        else data = DISREP_SYMBOL;
        valueRange.set(address, data);
      }
      
      while (!GSheet.values.update(&answ, spreadsheetId, range, &valueRange) && tries < SetTryNum) {
        tries++;
      }

      if (tries == SetTryNum) bot.sendMessage("updateError");
      valueRange.clear();
    }

    void Counting(byte start_week, byte end_week) {            // номера недель, ограничивающих область подсчета, нужно для подсчета только конкретного диапазона
      String formula = "", diapason;                                                  // строки для сборки формулы и диапазона
      byte table_len[2] = {};                                                         // горизонтальная длина таблицы

      for (byte parity_iter = 0; parity_iter < 2; parity_iter++) {                    // Высчитываем len (горизонталную длины недели в таблице) для обоих четностей недель у count.subgroup
        bool prev = false;
        for (int s = 0; s < 7; s++) {
          if (week[count.subgroup + 2*parity_iter]->subj_num[s] == 0) continue;
          if (prev) table_len[parity_iter] += 1;
          table_len[parity_iter] += week[count.subgroup + 2*parity_iter]->subj_num[s];
          prev = true;
        }
      }

      /*
      for (int s = 0; s < 7; s++) {                                         //ищем горизонтальную длину len строки, содержащей номера всех пар для обоих четностей недели подгруппы
        if (week[i+2*z]->subj_num[s] == 0) continue;
        if (prev) tableLen[z] += 1;
        tableLen[z] += week[i+2*z]->subj_num[s];
        prev = true;
      }*/
      
      // == Находим позицию вставки формулы в листе === (В данной версии пока так же одинаокова для любого варианта подсчета)
      String form_position = SheetName;
      form_position += charOffset(String(less_name_c), max(table_len[0], table_len[1]) + 5-1);
      form_position += people_list_i + offset * (end_week-1) + count.surn_ind;


      if (!count.mode || count.mode == 1)  {                                  // все предметы УП ИЛИ все предметы неУП
        // === Собираем диапазон ===
        diapason = less_name_c;                                                    // символьное начало диапазона
        diapason += people_list_i + offset * (start_week-1);        // численное начало диапазона
        diapason += ":";
        diapason += charOffset(String(less_name_c), max(table_len[0], table_len[1])-1);
        diapason += people_list_i + offset * (end_week-1) + sizeof(students)/sizeof(students[0]) - 1;

        // === Собираем формулу === (в данном случае конечный вид: =COUNTIF(FILTER(C581:U617,MOD(ROW(C581:U617)-588,23)=0),"D")
        formula.reserve(65);
        formula = "=COUNTIF(FILTER(";
        formula += diapason;
        formula += ";MOD(ROW(";
        formula += diapason;
        formula += ")-";
        formula += people_list_i + offset * (start_week-1) + count.surn_ind;
        formula += ";";
        formula += offset;
        formula += ")=0);\"";
        formula += (!count.mode) ? RESPECT_SYMBOL : DISREP_SYMBOL;                                // в зависимости от вида поиска ищем конкретный символ
        formula += "\")";
      }

      else if (count.mode == 2)   {        //по отдельным предметам неУП
        // === Собираем диапазон ===
        diapason = less_name_c;
        diapason += less_name_i + offset * (start_week-1);
        diapason += ":";
        diapason += charOffset(String(less_name_c), max(table_len[0], table_len[1])-1);
        diapason += people_list_i + offset * (end_week-1) + sizeof(students)/sizeof(students[0]) - 1;

        // === Собираем формулу ===, в данном случае ее конечный вид:
        // =COUNTIFS(FILTER(C244:U284; MOD(ROW(C244:U284)-244;24)=0); "Физ практикум (лб)"; FILTER(C244:U284; MOD(ROW(C244:U284)-244-2;24)=0); "D")
        formula.reserve(110 + count.subject.length());
        formula = "=COUNTIFS(FILTER(";
        formula += diapason;
        formula += ";MOD(ROW(";
        formula += diapason;
        formula += ")-";
        formula += less_name_i + offset * (start_week-1);
        formula += ";";
        formula += offset;
        formula += ")=0);\"";
        formula += count.subject;
        formula += "\";FILTER(";
        formula += diapason;
        formula += ";MOD(ROW(";
        formula += diapason;
        formula += ")-";
        formula += less_name_i + offset * (start_week-1);
        formula += "-";
        formula += 2+count.surn_ind;
        formula += ";";
        formula += offset;
        formula += ")=0);\"";
        formula += DISREP_SYMBOL;
        formula += "\")";
      }

      else bot.sendMessage("Неизвестный count.mode!", error_chat);
      // === Устанавливаем формулу в листе ===
      FirebaseJson response, valueRange;
      valueRange.add("range", form_position);
      valueRange.add("majorDimension", "ROWS");
      valueRange.set("values/[0]/[0]", formula);

      byte tries = 0;
      while (!GSheet.values.update(&response, spreadsheetId, form_position, &valueRange) && tries < SetTryNum) tries++;
      if (tries == SetTryNum) bot.sendMessage("updateError");
      valueRange.clear();
      /*
      String responseStr;
      response.toString(responseStr, true);                 //Вывод ответа от Google Sheets API для отладки
      bot.sendMessage(responseStr, error_chat);
      */
      response.clear();

      // === Получаем итоговую цифру подсчета ===
      tries = 0;
      FirebaseJsonData result_object;
      while (!GSheet.values.get(&response, spreadsheetId, form_position) && tries < GetTryNum) tries++;
      if (tries == GetTryNum) bot.sendMessage("getError", error_chat);

      /*String responseStr;
      response.toString(responseStr, true);                 //Вывод ответа от Google Sheets API для отладки
      bot.sendMessage(responseStr, error_chat);*/

      response.get(result_object, "values/[0]/[0]");
      response.clear();
      count.total = result_object.intValue;
      result_object.clear();
    }

    bool ready() {
      return GSheet.ready();
    }

} list;

class Menu {
  private:
    bool ret_command = false, reading_flag = true;
    byte nka_ind = 0;
    const String s_menu[4] = {"Редактировать", "Подсчитать", "Статистика", "Настройки"};
    String way = "0";
    byte unknown_ind = 0;
    week_diapason local_diapason;         // границы недель диапазона подсчета

  public:
    void start_page(bool mode, FDstat_t file_status = FD_NO_DIF) {        // функция показа стартовой страницы
      // file_status отображает статус работы с файлом настроек, нужен (в данной функции) для понимания - отправлять или подтягивать сообщения у пользователей
      way.reserve(7);

      bot.notify(false);
      if (!mode)  {
        for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
          if (file_status == FD_WRITE || file_status == FD_ADD) {
            bot.sendMessage("ИСиТенок v" + String(Version, 1), Admins[i]);
            chat_settings.status_mess[i] = bot.lastBotMsg();
          }
          else bot.editMessage(chat_settings.status_mess[i], "ИСиТенок v" + String(Version, 1), Admins[0]);
        }
        chat_file.update();
        return;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        if (file_status == FD_WRITE || file_status == FD_ADD) {
          bot.inlineMenu("Выберите:", s_menu[0] + "\t" + s_menu[1] + "\t" + s_menu[2] + "\n" + s_menu[3], Admins[i]);
          chat_settings.menu_id[i] = bot.lastBotMsg();
        }
        else  bot.editMenu(chat_settings.menu_id[i], s_menu[0] + "\t" + s_menu[1] + "\t" + s_menu[2] + "\n" + s_menu[3], Admins[i]);
      }

      bot.notify(false);
      chat_file.update();
    }

    void menuEdit(String comm, String user) {        // обработка нажатий в меню
      FB_Time t = bot.getTime(3);
      static bool N_edited = false;

      if (comm == "На главную" && way != "0") {
        way = "0";
        ret_command = true;
      }
      if (comm == "Назад" && way != "0")  {
        way.remove(way.length()-1);
        ret_command = true;
      }

      if (way == "0") {                   // отображается стартовая страница
        if (comm == s_menu[0]) {          // нажали кнопку "Редактирование"
          way = "01";
          edit_page(0);
          return;
        }

        if (comm == s_menu[1]) {          // нажали кнопку "Подсчет"
          way = "02";
          calculate_page(0);
          return;
        }

        if (comm == s_menu[2]) {          // нажали кнопку "Статистика"
          way = "03";
          stat_page(0);
          return;
        }

        if (comm == s_menu[3]) {          // нажали кнопку "Настройки"
          way = "04";
          settings_page(0);
          return;
        }

        if (ret_command)  {
          ret_command = false;
          start_page(1);
        }
        
        else  bot.sendMessage("err_menu", error_chat);
      }

      if (way.startsWith("01")) {                                                     // "бэкенд" ветки редактирования 
        if (way == "01") {                                                            // отображается страница выбора фамилии
          nka.surn = "";
          nka.nki = "";
          nka.date.month = t.month;
          nka.date.day = t.day;
          nka.date.year = t.year;
          nka.dayWeek = t.dayWeek;
          nka.posC = 'A';
          nka.posI = 0;
          for (byte i = 0; i < sizeof(students)/sizeof(students[0]); ++i) {
            if (comm == students[i].surname)  {
              nka.surn = students[i].surname;
              nka.subgroup = students[i].subgroup;
              way = "011";
              edit_page(1);
              return;
            }
          }

          if (ret_command)  {
            ret_command = false;
            edit_page(0);
          }
        }

        else if (way == "011") {                                                       // возможность поставить Н, или перейти к выбору другой даты
          if (comm.startsWith("Дата:")) {
            way = "0111";
            edit_page(2);
            return;
          }

          else if (comm.startsWith("(")) {                                             // если пользователь нажал на конкретную пару в дне
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) {
              if (String(comm[1]) == String(week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->less_nums[nka.dayWeek-1][i])) {
                edit_page(4);
                way = "011111";
                nka_ind = i;
                return;
              }
            }
          }

          else if (comm == "Все УП") {                                                 // выбрал "поставить УП на все пары в дне"
            N_edited = true;
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += '+';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Все неУП") {                                               // выбрал "поставить неУП на все пары в дне"
            N_edited = true;
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += '-';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Нет пропусков") {                                          // выбрал "убрать пропуски на всех парах в дне"
            N_edited = true;
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += ' ';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Поставить") {                                              // поставить введенные Нки
            String range;
            getNIndex();                              //подумать, нужно ли оно тут
            range += SheetName;
            range += nka.posC;
            range += nka.posI;
            range += ":";
            range += charOffset(nka.posC, week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]-1);
            range += nka.posI;
            if (N_edited) {
              list.SetN(range);
              N_edited = false;
            }
            way = "01";
            edit_page(0);
            return;
          }

          else if (comm.startsWith("В этот")) {                                        // нажал на плашку "В этот день пар нет" (любопытный тестировщик)
            bot.sendMessage("Чо жмешь? Сказали же, пар в выбранный день нет!", user);
            timer.add(bot.lastBotMsg(), 7, user);
            return;
          }

          else if (ret_command)  {
            ret_command = false;
            edit_page(1);
          }
        }

        else if (way == "0111") {                                                          // выбор месяца
          nka.date.month = t.month;
          nka.date.day = t.day;
          nka.date.year = t.year;
          nka.dayWeek = t.dayWeek;
          for (int i = 0; i < 12; i++) {
            if (comm == months[i]) {
              nka.date.month = i+1;
              way = "01111";
              edit_page(3);
              return;
            }
          }
          
          if (ret_command)  {
            ret_command = false;
            edit_page(2);
          }
        }

        else if (way == "01111") {                                                         // выбор дня в месяце
          for (int i = 1; i < getDayInMonth(nka.date.month-1, nka.date.year)+1; i++) {
            if (comm == String(i)) {
              nka.date.day = i;
              way = "011";
              edit_page(1);
              return;
            }
          }

          if (ret_command)  {
            ret_command = false;
            edit_page(3);
          }
        }

        else if (way == "011111") {                                                       // выбор варианта Нки
          if (comm != "Вернуться") {
            if (comm == "УП") nka.nki[nka_ind] = '+';
            else if (comm == "неУП") nka.nki[nka_ind] = '-';
            else nka.nki[nka_ind] = ' ';
            N_edited = true;
          }
          way = "011";
          reading_flag = false;
          edit_page(1);
          return;
        }

        else if (ret_command)  {
          ret_command = false;
          edit_page(0);
        }

        else  bot.sendMessage("err_menu", error_chat);
      }

      else if (way.startsWith("02")) {                                  // "бэкенд" ветки подсчета
        if (way == "02") {
          nka.surn = "";
          nka.nki = "";
          nka.date.month = t.month;
          nka.date.day = t.day;
          nka.date.year = t.year;
          nka.dayWeek = t.dayWeek;
          nka.posC = 'A';
          nka.posI = 0;
          byte len[2] = {};
          for (byte i = 0; i < sizeof(students)/sizeof(students[0]); ++i) {
            if (comm == students[i].surname)  {
              count.surn = students[i].surname;
              count.subgroup = students[i].subgroup;
              count.surn_ind = len[count.subgroup];
              way = "021";
              calculate_page(1);
              return;
            }
            len[students[i].subgroup]++;
          }

          if (ret_command)  {
            ret_command = false;
            calculate_page(0);
          }
        }

        if (way == "021") {
          if (ret_command)  {
            ret_command = false;
            calculate_page(1);
          }
          
          else {
            if (comm == "Общее УП") count.mode = 0;
            else if (comm == "Общее неУП") count.mode = 1;
            else if (comm == "По предметам (неУП)") {
              count.mode = 2;
              way = "0211";
              calculate_page(2);
              return;
            }

            calculate_page(3);
            way = "0212";
          }
          return;
        }

        if (way == "0212") {                     // нажата кнопка на меню выбора диапазона подсчета          
          if (comm == "Готово") {
            list.Counting(local_diapason.end, local_diapason.start);
            calculate_page(5);
          }

          else if (ret_command)  {
            ret_command = false;
            calculate_page(3);
          }

          else {                                      // обрабатывааем нажатия на неделю
            // здесь надо суметь вычислить индекс в глобальном пространстве индексов недель [1; week_off] и засунуть его в unknown_ind
            // здесь имеем comm = ~ "с 23.03 по 30.03"

            if (comm.indexOf("Эта неделя") != -1) unknown_ind = week_off;
            else if (comm.indexOf("Предыдущая") != -1)  unknown_ind = week_off-1;

            else {
              int8_t c_index = comm.indexOf("с");                                   // в любой строке индекс начала значащей части (без значков и отступов)

              if (c_index == -1)   {                                                // на прям крайняк
                bot.sendMessage(F("invalidMenuTextInCount!"), user);
                return;
              }

              Date startDate;
              startDate.day = (comm[c_index+3] - '0')*10 + (comm[c_index+4] - '0');
              startDate.month = (comm[c_index+6] - '0')*10 + (comm[c_index+7] - '0');

              bool found = false;
              for (byte i = 0; i < week_off; i++) {                     // вычисляем, на расстоянии скольки недель от текущей находится нажатая, путем сравнения дат начала и увеличения даты нажатой каждую итерацию на 7 дней
                if (startDate.day == week[0]->pon_date.day && startDate.month == week[0]->pon_date.month)  {
                  unknown_ind = week_off-i;
                  found = true;
                  break;
                }

                sumDate(&startDate, 7);
              }

              if (!found) {
                bot.sendMessage(F("Не удалось найти индекс выбранной недели!"), user);
              }
            }

            calculate_page(4);                        // страница выбора статуса недели (Начало диапазона, конец или только эта неделя)
            way = "02121";
          }

          return;
        }

        else if (way == "02121") {                    // нажатия на странице выбора статуса недели (Начало диапазона, конец или только эта неделя)
          if (comm == "Начало") local_diapason.start = unknown_ind;
          else if (comm == "Конец") local_diapason.end = unknown_ind;
          else if (comm == "Начало и конец") {
            local_diapason.start = unknown_ind;
            local_diapason.end = unknown_ind;
          }

          way = "0212";
          calculate_page(3);
          return;
        }

        else if (way == "0211") {                     // выбор предмета для подсчета
          for (byte i = 0; i < sizeof(subjects)/sizeof(subjects[0]); i++) {
            if (comm == subjects[i]) {
              count.subject = comm;
              calculate_page(3);
              way = "0212";
              return;
            }
          }

          if (ret_command)  {
            ret_command = false;
            calculate_page(2);
          }
        }

        else bot.sendMessage("err_menu2");
      }

      else if (way.startsWith("03")) {                    // "бэкенд" ветки статистики
        if (way == "03") {                    // стартовая страница
          if (comm == "Общее неУП") {
            way = "031";
            stat_page(1);
          }
        }

        else if (way == "031") {
          if (comm == "Готово") {
            byte len[2] = {};
            count.mode = 1;
            bot.sendMessage("Подсчитываю пропуски...", user);
            uint32_t mess_id = bot.lastBotMsg();
            unsigned int max_len = 0;
            way = "0";
            start_page(1);

            for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) max_len = max(max_len, students[i].surname.length());
            String total_list = "```\nФио:";
            for (byte i = 0; i < max_len+7-6; i++)  total_list += " ";
            total_list += "Нки:\n";

            for (byte i = 0; i < sizeof(students)/sizeof(students[0]); ++i) {
              count.surn = students[i].surname;
              count.subgroup = students[i].subgroup;
              count.surn_ind = len[count.subgroup];
              len[students[i].subgroup]++;
              list.Counting(local_diapason.end, local_diapason.start);

              total_list += students[i].surname;
              byte lim = max_len-students[i].surname.length() - (max_len-students[i].surname.length())/2;
              lim += 7;
              for (byte j = 0; j < lim; j++) total_list += (j != 0 && j != lim-1) ? "-" : " ";
              total_list += count.total;

              if (i != sizeof(students)/sizeof(students[0])-1) total_list += "\n";
            }
            total_list += "```\n";

            bot.setTextMode(FB_MARKDOWN);                       // для красивой таблички
            bot.editMessage(mess_id, total_list, user);
            bot.setTextMode(FB_TEXT);
          }

          else if (ret_command)  {
            ret_command = false;
            stat_page(1);
          }

          else {                                      // обрабатывааем нажатия на неделю
            // здесь надо суметь вычислить индекс в глобальном пространстве индексов недель [1; week_off] и засунуть его в unknown_ind
            // здесь имеем comm = ~ "с 23.03 по 30.03"

            if (comm.indexOf("Эта неделя") != -1) unknown_ind = week_off;
            else if (comm.indexOf("Предыдущая") != -1)  unknown_ind = week_off-1;

            else {
              int8_t c_index = comm.indexOf("с");                                   // в любой строке индекс начала значащей части (без значков и отступов)

              if (c_index == -1)   {                                                // на прям крайняк
                bot.sendMessage(F("invalidMenuTextInCount!"), user);
                return;
              }

              Date startDate;
              startDate.day = (comm[c_index+3] - '0')*10 + (comm[c_index+4] - '0');
              startDate.month = (comm[c_index+6] - '0')*10 + (comm[c_index+7] - '0');

              bool found = false;
              for (byte i = 0; i < week_off; i++) {                     // вычисляем, на расстоянии скольки недель от текущей находится нажатая, путем сравнения дат начала и увеличения даты нажатой каждую итерацию на 7 дней
                if (startDate.day == week[0]->pon_date.day && startDate.month == week[0]->pon_date.month)  {
                  unknown_ind = week_off-i;
                  found = true;
                  break;
                }

                sumDate(&startDate, 7);
              }

              if (!found) {
                bot.sendMessage(F("Не удалось найти индекс выбранной недели!"), user);
              }
            }

            stat_page(2);                        // страница выбора статуса недели (Начало диапазона, конец или только эта неделя)
            way = "032";
          }
        }
        
        else if (way == "032") {
          if (comm == "Начало") local_diapason.start = unknown_ind;
          else if (comm == "Конец") local_diapason.end = unknown_ind;
          else if (comm == "Начало и конец") {
            local_diapason.start = unknown_ind;
            local_diapason.end = unknown_ind;
          }
          way = "031";
          stat_page(1);
        }
      }

      else if (way.startsWith("04")) {     // "бэкенд" ветки настроек
        if (way == "04") {                                  // главная страница с выбором настройки
          if (ret_command)  {
            ret_command = false;
            settings_page(0);
          }

          else if (comm == "Сроки промежуточной аттестации") {
            settings_page(1);
            way = "0411";
            return;
          }
        }

        if (way == "0411") {            // обработка нажатий на неделю в показанном списке
          if (ret_command)  {
            ret_command = false;
            calculate_page(3);
          }
          
          else {
            int8_t c_index = comm.indexOf("с");                                   // в любой строке индекс начала значащей части (без значков и отступов)

            if (c_index == -1)   {                                                // на прям крайняк
              bot.sendMessage(F("invalidMenuTextInCount!"), user);
              return;
            }

            Date parsedDate;
            parsedDate.day = (comm[c_index+3] - '0')*10 + (comm[c_index+4] - '0') + (realTime.dayWeek-1);         // не только парсим день, но еще и сравниваем его по дню недели с текущим днем недели, упрощает расчеты ввиду получения кратности разницы
            parsedDate.month = (comm[c_index+6] - '0')*10 + (comm[c_index+7] - '0');
            // ДОБАВИТЬ parsedDate.year!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            byte week_diff = 0;

            
            int days_between;

            if (parsedDate.month <= realTime.month) {                             // если выбранная неделя в этом, или в одном из прошлый месяцев
              days_between = getDayInMonth(parsedDate.month - 1, parsedDate.year) - parsedDate.day;

              for (byte i = parsedDate.month; i < realTime.month - 1; i++) {
                days_between += getDayInMonth(i, realTime.year);                      // ЗДЕСЬ ПОКА ЗАГЛУШКА!!!! ПЕРЕДЕЛАТЬ ЧЕРЕЗ КОЛ-ВО ДНЕЙ ОТ 01.01.2000
              }

              days_between += realTime.day;
            }
            
            else {                                                               // если выбранная неделя в будущем месяце (месяцах)
              days_between = getDayInMonth(realTime.month-1, realTime.year) - realTime.day;

              for (byte i = realTime.month; i < parsedDate.month - 1; i++) {
                days_between += getDayInMonth(i, realTime.year);                       // ЗДЕСЬ ПОКА ЗАГЛУШКА!!!! ПЕРЕДЕЛАТЬ ЧЕРЕЗ КОЛ-ВО ДНЕЙ ОТ 01.01.2000
              }

              days_between += parsedDate.day;
              days_between = -days_between;
            }

            if (abs(days_between) % 7 != 0)  {
              bot.sendMessage(F("WARNING! Возможна ошибка с расчетом количества недель!\nКритично! (settings page)"), error_chat);
              bot.sendMessage(String(days_between), error_chat);
              return;
            }
            
            unknown_ind = week_off - days_between;

            bot.sendMessage(String(unknown_ind) + "/" + String(week_off), error_chat);
            
          }
        }
      }
    }

    void edit_page(byte edit_depth) {               // "фронтенд" страниц подменю "Редактировать"
      FB_Time t = bot.getTime(3);
      String mess = "";
      switch (edit_depth) {
        case 0:
          mess = "";
          for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
            mess += students[i].surname;
            if (i % 3 == 2 || i == (sizeof(students)/sizeof(students[0]))-1) mess += "\n";
            else mess += "\t";
          }
          mess += "На главную";
        break;

        case 1: {
          /*String range = "", answ;
          mess = nka.surn;
          mess += "\t";
          mess += nka.subgroup+1;
          mess += " подгруппа";
          mess +=  "\tДата: ";
          if (nka.date.day < 10) mess += "0";
          mess += nka.date.day;
          mess += ".";
          if (nka.date.month < 10) mess += "0";
          mess += nka.date.month;
          mess += ".";
          mess += nka.date.year % 100;
          mess += "\n";
          getNIndex();
          byte week_index = nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2);                               //индекс недели, складывается из подгруппы и сдвига на неделю, соответствующую выставляемым Нкам по четности
          if (week[week_index]->subj_num[nka.dayWeek-1])  {               //если в этот день пары есть (в день, соответственной Нке по четности, недели)
            if (reading_flag) {
              nka.nki = "";                                                 //разобраться, почему нужна эта заплатка и починить (если очень захочется :) )
              range += SheetName;
              range += nka.posC;
              range += nka.posI;
              range += ":";
              range += charOffset(nka.posC, week[week_index]->subj_num[nka.dayWeek-1]-1);
              range += nka.posI;
              answ = list.getCells(range);
              Text answer(answ);
              for (byte i = 0; i < (week[week_index]->subj_num[nka.dayWeek-1]); i++) {
                String a = "";
                answer.getSub(r_count + r_offset*i, "\"").toString(a);
                if (a == RESPECT_SYMBOL)  nka.nki += "+";
                else if (a == DISREP_SYMBOL) nka.nki += "-";
                else nka.nki += " ";
              }
            }

            for (byte i = 0; i < week[week_index]->subj_num[nka.dayWeek-1]; i++) {             //отображать будем пары, которые есть в день, когда Нки будем ставить
              mess += "(";
              mess += week[week_index]->less_nums[nka.dayWeek-1][i];
              mess += ") ";
              if (nka.nki[i] == '-')  mess += Disrep;
              else if (nka.nki[i] == '+') mess += Respect;
              else mess += Presence_menu;
              if (i != week[week_index]->subj_num[nka.dayWeek-1]-1) mess += "\t";
              else mess += "\n";
            }

            mess += "Все УП\tВсе неУП\tНет пропусков\n";
            mess += "Назад\tНа главную\tПоставить";
          }
          else {
            mess += "В этот день пар нет!\n";
            mess += "Назад\tНа главную";
          }
          reading_flag = true;*/
        }
        break;

        case 2: {
          mess = "";

          for (int i = START_MONTH; i < t.month+1; i++) {
            mess += months[i-1];
            if (i % 3 == 1 || i == t.month) mess += "\n";
            else mess += "\t";
          }
          
          mess += "Назад\tНа главную"; 
        
        }
        break;

        case 3: {                     //переделать полностью (на 3 этапа)
          /*
          int k = 1;
          if (nka.month == START_MONTH) k = START_DAY;
          byte day_n = nka.day;
          nka.day = k;
          byte dayWeek_n = nka.dayWeek;
          getNIndex(nka.subgroup);
          byte offset_days = nka.dayWeek;
          nka.day = day_n;
          nka.dayWeek = dayWeek_n;

          for (int i = k; i < day_month[nka.month-1]+offset_days; i++) {
      
            if (nka.month == t.month && i == t.day+1) {
              mess += "\n";
              break; 
            }

            if (i < offset_days) mess += " ";
            else mess += i-offset_days+1;
            if ((i-k-1) % 7 == 5 || i == day_month[nka.month-1]+offset-1) mess += "\n";
            else mess += "\t";
          }*/

          mess = "";

          //этап 1 (сдвиг начала месяца)
          int k = 1;
          if (nka.date.month == START_MONTH) k = START_DAY;
          byte day_n = nka.date.day;                   
          byte dayWeek_n = nka.dayWeek;
          nka.date.day = k;
          getNIndex();
          byte pre_offset = nka.dayWeek-1;
          byte post_offset;
          if (nka.date.month == t.month) nka.date.day = day_n;
          else  nka.date.day = getDayInMonth(nka.date.month-1, nka.date.year);
          getNIndex();
          post_offset = 7 - nka.dayWeek;
          nka.date.day = day_n;
          nka.dayWeek = dayWeek_n;

          mess += "-пн-\t-вт-\t-ср-\t-чт-\t-пт-\t-сб-\t-вс-\n";

          for (byte i = 0; i < pre_offset; i++) mess += " \t";

          for (byte i = k-1; i < getDayInMonth(nka.date.month-1, nka.date.year); i++) {
            mess += i+1;
            if ((i+pre_offset-k) % 7 == 5)  mess += "\n";
            else mess += "\t";
            if (nka.date.month == t.month && i+1 == week[0]->pon_date.day+6) break;
          }

          for (byte i = 0; i < post_offset; i++)  {
            if (i != post_offset-1) mess += " \t";
            else mess += " \n";
          }
          
          mess += "Назад\tНа главную";
        }
        break;

        case 4: {
          mess = "Пропуск:\nУП\tнеУП\tПрисутствие\nВернуться";
        }
        break;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        bot.editMenu(chat_settings.menu_id[i], mess, Admins[i]);
      }
    }

    void calculate_page(byte calculate_depth) {     // "фронтенд" страниц подменю "Подсчитать"
      String mess = "";
      mess.reserve(1024);                             // должно чуточку ускорить работу со стрингами, уберегая от реаллокаций и иных плохостей
      switch (calculate_depth) {
        case 0:                                       // страница выбора фамилии
            mess = "";
            for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
              mess += students[i].surname;
              if (i % 3 == 2 || i == (sizeof(students)/sizeof(students[0]))-1) mess += "\n";
              else mess += "\t";
            }
            mess += "На главную";
        break;

        case 1:                                      // страница выбора варианта подсчета
          mess = "";
          mess += count.surn;
          mess += "\n";
          mess += "Общее УП\tОбщее неУП\tПо предметам (неУП)\n";
          mess += "Назад\tНа главную";
          local_diapason.start = week_off;
          local_diapason.end = 1;
        break;

        case 2:                                     // страница выбора предмета (если выбран варинат подсчета по предмету)
          mess = "Выберите предмет:\n";
          for (byte i = 0; i < sizeof(subjects)/sizeof(subjects[0]); i++) {
              mess += subjects[i];
              if (i % 3 == 2 || i == (sizeof(subjects)/sizeof(subjects[0]))-1) mess += "\n";
              else mess += "\t";
          }
          mess += "Назад\tНа главную";
        break;

        case 3: {                                                                      // страница, предлагающая выбор диапазона недель для подсчета
          mess = "Нажмите для обозначения границ:\n";
          mess += "Назад\tГотово\tНа главную\n";  
          Date date_start(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year), date_end(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year);
          sumDate(&date_end, 6);

          for (byte i = 0; i < week_off; i++) {
            
            if (local_diapason.start == week_off-i || local_diapason.end == week_off-i) {
              if (local_diapason.start == week_off-i && local_diapason.end == week_off-i) mess += STARTEND_SYMBOL;
              else if (local_diapason.start == week_off-i) mess += START_SYMBOL;
              else mess += END_SYMBOL;
              mess += " --- ";
            }

            if (!i) mess += "Эта неделя";

            else if (i == 1) mess += "Предыдущая";

            else {
              mess += "с ";
              if (date_start.day < 10) mess += "0";
              mess += date_start.day;
              mess += ".";
              if (date_start.month < 10) mess += "0";
              mess += date_start.month;
              mess += " по ";
              if (date_end.day < 10) mess += "0"; 
              mess += date_end.day;
              mess += ".";
              if (date_end.month < 10) mess += "0";
              mess += date_end.month;
            }

            sumDate(&date_start, -7);                     // отодвигаем дату назад на неделю
            sumDate(&date_end, -7);

            if (local_diapason.start == week_off-i || local_diapason.end == week_off-i) {
              mess += " --- ";
              if (local_diapason.start == week_off-i && local_diapason.end == week_off-i) mess += STARTEND_SYMBOL;
              else if (local_diapason.start == week_off-i) mess += START_SYMBOL;
              else mess += END_SYMBOL;
            }

            if (i != week_off-1) mess += "\n";
          }
          break;
        }

        case 4:
          mess = "Эта неделя ... диапазона:\nНачало\tКонец\tНачало и конец\nНа главную\tНазад";
          break;

        case 5:                                     // страница, отображающая итог подсчета
          mess = count.surn;
          mess += "\t";
          if (!count.mode)  mess += "УП\t";
          else mess += "неУП\t";

          if (local_diapason.start == week_off && local_diapason.end == 1)  mess += "Всего";
          else mess += "В диапазоне";

          if (count.mode == 2)  {
            mess += "\nПредмет: ";
            mess += count.subject;
          }

          mess += "\n";
          mess += count.total;
          mess += "\nНа главную";
          break;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {                 // обновляем страницу у всех пользователей
        bot.editMenu(chat_settings.menu_id[i], mess, Admins[i]);
      }
    }

    void stat_page(byte stat_depth) {               // "фронтенд" страниц подменю "Статистика"
      String mess = "";
      switch (stat_depth) {
        case 0: {
          mess += "Общее УП\tОбщее неУП\tПо предметам (неУП)\n";
          mess += "Назад\tНа главную";
          local_diapason.start = week_off;
          local_diapason.end = 9;                     // ВРЕМЕННО!!
          break;
        }

        case 1: {
          mess = "Нажмите для обозначения границ:\n";
          mess += "Назад\tГотово\tНа главную\n";  
          Date date_start(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year), date_end(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year);
          sumDate(&date_end, 6);

          for (byte i = 0; i < week_off; i++) {
            
            if (local_diapason.start == week_off-i || local_diapason.end == week_off-i) {
              if (local_diapason.start == week_off-i && local_diapason.end == week_off-i) mess += STARTEND_SYMBOL;
              else if (local_diapason.start == week_off-i) mess += START_SYMBOL;
              else mess += END_SYMBOL;
              mess += " --- ";
            }

            if (!i) mess += "Эта неделя";

            else if (i == 1) mess += "Предыдущая";

            else {
              mess += "с ";
              if (date_start.day < 10) mess += "0";
              mess += date_start.day;
              mess += ".";
              if (date_start.month < 10) mess += "0";
              mess += date_start.month;
              mess += " по ";
              if (date_end.day < 10) mess += "0"; 
              mess += date_end.day;
              mess += ".";
              if (date_end.month < 10) mess += "0";
              mess += date_end.month;
            }

            sumDate(&date_start, -7);                     // отодвигаем дату назад на неделю
            sumDate(&date_end, -7);

            if (local_diapason.start == week_off-i || local_diapason.end == week_off-i) {
              mess += " --- ";
              if (local_diapason.start == week_off-i && local_diapason.end == week_off-i) mess += STARTEND_SYMBOL;
              else if (local_diapason.start == week_off-i) mess += START_SYMBOL;
              else mess += END_SYMBOL;
            }

            if (i != week_off-1) mess += "\n";
          }
          break;
        }

        case 2: {
          mess = "Эта неделя ... диапазона:\nНачало\tКонец\tНачало и конец\nНа главную\tНазад";
          break;
        }
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {                 // обновляем страницу у всех пользователей
        bot.editMenu(chat_settings.menu_id[i], mess, Admins[i]);
      }
    }

    void settings_page(byte sett_depth) {           // "фронтенд" страниц подменю "Настройки"
      String mess = "";
      switch (sett_depth) {
        case 0: {
          mess = "Сроки промежуточной аттестации\nНа главную";
          break;
        }

        case 1: {
          mess = "Нажмите для обожначения границ:\nНазад\tГотово\tНа главную\n";
          week_diapason centre;
          centre.start = (!settings.att_diapason.start) ? week_off+2 : settings.att_diapason.start;
          centre.end = (!settings.att_diapason.end) ? week_off-2 : settings.att_diapason.end;

          Date date_start(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year), date_end(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year);
          sumDate(&date_end, 6);

          // нужно подвинуть даты начала и конца недели до валидных значений (до недели начала промежутки (или до текущей недели) - минус 4 недели (или меньше) назад, именно с таких дат начинаем список)
          byte prev_weeks = ((week_off > 4) ? 4 : week_off-1);

          sumDate(&date_start, 7 * (centre.start - week_off + prev_weeks));
          sumDate(&date_end, 7 * (centre.start - week_off + prev_weeks));


          for (byte i = 0; i < 5 + prev_weeks + abs(centre.start - centre.end); i++) {         // отображаем 4 недели до, 4 после, и все недели, входящие в промежутку. Иначе - то же самое, но вместо недель промежутки - актуальная неделя
            byte start_offset = week_off - i + prev_weeks + ((centre.start - centre.end > 1) ? centre.start - centre.end+1: centre.start - centre.end)/2;
            if (settings.att_diapason.start && (centre.start == start_offset || centre.end == start_offset)) {     // если надо - в начале ячейки недели ставим спецсимвол
              if (centre.start == start_offset && centre.end == start_offset) mess += STARTEND_SYMBOL;
              else if (centre.start == start_offset) mess += START_SYMBOL;
              else mess += END_SYMBOL;
              mess += " --- ";
            }

            mess += "с ";
            if (date_start.day < 10) mess += "0";
            mess += date_start.day;
            mess += ".";
            if (date_start.month < 10) mess += "0";
            mess += date_start.month;
            mess += " по ";
            if (date_end.day < 10) mess += "0"; 
            mess += date_end.day;
            mess += ".";
            if (date_end.month < 10) mess += "0";
            mess += date_end.month;

            sumDate(&date_start, -7);                     // отодвигаем дату назад на неделю
            sumDate(&date_end, -7);

            if (settings.att_diapason.start && (centre.start == start_offset || centre.end == start_offset)) {     // если надо - в конце ячейки недели тоже ставим спецсимвол
              mess += " --- ";
              if (centre.start == start_offset && centre.end == start_offset) mess += STARTEND_SYMBOL;
              else if (centre.start == start_offset) mess += START_SYMBOL;
              else mess += END_SYMBOL;
            }

            if (i != 4 + prev_weeks + abs(centre.start - centre.end))  mess += "\n";
          }

          break;
        }
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {                 // обновляем страницу у всех пользователей
        bot.editMenu(chat_settings.menu_id[i], mess, Admins[i]);
      }
    }

} menu;

void setup() {
  Serial.begin(115200);                                                         // последовательный порт аааткрывать
  WiFi_Connect();                                                               // подключаемся к WiFi
  bot.attach(newMsg);                                                           // подключаем обработчик входящих сообщений
  bot.setPeriod(50);                                                            // период между проверками входящих сообщений

  ArduinoOTA.setHostname(OTA_NAME);                                           //имя для точки OTA обновления
  ArduinoOTA.setPassword(OTA_PASS);                                           //пароль
  ArduinoOTA.begin(); 

  if (!FFat.begin()) {                                                          // подключаем файловую систему
    bot.sendMessage(F("Ошибка инициализации файловой системы!"), error_chat);
  }
  chat_file.addWithoutWipe(true);
  FDstat_t file_stat;

  for (byte files = 0; files < 3; files++) {
    if (!files)  file_stat = chat_file.read();
    else if (files == 1) file_stat = week_file.read();
    else file_stat = settings_file.read();

    switch (file_stat) {
      case FD_FS_ERR: bot.sendMessage(F("FileSystemError!"), error_chat);
        break;
      case FD_FILE_ERR: bot.sendMessage("OpenFileError!");
        break;
      default:
        break;
    }
  }

  if (week_off < 1) {
    bot.sendMessage("Переменная week_off в структуре file_data должная иметь значение > 1!\nИзмените параметр, прежде чем продолжить работу!", error_chat);
    for (;;) {
      ArduinoOTA.handle();
    }
  }

  bot.clearServiceMessages(true);                                             //автоматическое удаление всех "сервисных" сообщений по типу "... закрепил сообщение"

  menu.start_page(0, file_stat);       // чисто для обновления структуры FB_Time
  list.begin();
  menu.start_page(1, file_stat);       // вот тут уже отсылаем менюшку
  bot.sendMessage("Запускаюсь!!", error_chat);
}

void loop() {
  static uint8_t old_day = 0;
  static uint32_t heap_timeout = millis();
  MemoryControl MemControl;

  bot.tick();
  chat_file.tick();
  week_file.tick();
  settings_file.tick();
  timer.tick();
  serviceMess.tick();
  ArduinoOTA.handle();

  realTime = bot.getTime(3);

  if (!old_day && realTime.day)  old_day = realTime.day;      // если структура realTime обновилась = запоминаем день
  else if (old_day != realTime.day) {                         // если сменился день - повод проверить актуальность недели
    //checkTableWeek();
    old_day = realTime.day;
  }

  if (millis() - heap_timeout >= HEAP_CHECK_TIMEOUT) {
    heap_timeout = millis();
    if (!MemControl.check())  bot.sendMessage(F("Объем оперативной памяти критически мал! (main)"), error_chat);
  }

}