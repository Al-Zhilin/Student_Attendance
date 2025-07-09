#define ATOMIC_FS_UPDATE

#include <FastBot.h>
#include <ESP_Google_Sheet_Client.h>
#include <StringUtils.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include "types.h"
#include <passwords.h>

//-------------------------------------НАСТРОЙКИ-------------------------------------------------------------------------------------------------------------------------------------------------------------
#define WIFI_RES_PERIOD 1 * 60 * 1000                                                            //период ожидания подключения к WiFi, по истечении - перезагрузка
#define START_MONTH 2                                                                            //месяц, с которого начинается учеба в семестре
#define START_DAY 3                                                                              //дата понедельника в первой учебной неделе (даже если учеба фактически началась не в понедельник)
#define Respect "✅"                                                                             //символ, соответствующий УП в меню
#define Disrep "❌"                                                                              //символ, соответствующий неУП в меню
#define GetTryNum 3                                                                              //количество попыток получить данные из таблицы, после них - вывод ошибки
#define SetTryNum 3                                                                              //количество попыток отправить данные в таблицу, после них - вывод ошибки
#define SerialDebug 0                                                                            //вкл/выкл (1/0 соответственно) отладка в Serial
#define SURNAME_ERRORS_NUM 2                                                                     //количество допускаемых ошибок в фамилии при сокращенном вводе (количество посимвольных отличий между вводимой фамилией и соответствующей фамилией из списка)
#define MINUTES_OFFSET 5                                                                         //Допустимый оффсет по времени, для определения, какая пара сейчас идет (позволяет определять, что сейчас идет N-ая пара, даже если сейчас часы_начала_пары:минуты_начала-<значение> или аналогично для конца пары)
//-------------------------------------НАСТРОЙКИ-------------------------------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------ОФФСЕТЫ--------------------------------------------------------------------------------------------------------------------------------------------------------------
byte offset[] = {23, 24};                                                                        //смещение (в количестве строк) между одними и теми же данными, в неделях, различающийся по номеру на 1, для каждого листа (подгруппы)
#define r_count 11                                                                               //количество " , до первого значения из ячейки в массиве мусора и угара от библиотеки 
#define r_offset 2                                                                               //сдвиг в количестве " в том же мусоре от библы, для получения следующего значения из массива
//--------------------------------------ОФФСЕТЫ--------------------------------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------РАЗНЫЕ КОНСТАНТЫ ТАБЛИЦЫ---------------------------------------------------------------------------------------------------------------------------------------------
#define Sheet1 "People1!"                                                                         //имя листа, с данными о людях первой подгруппы
#define Sheet2 "People2!"                                                                         //имя листа, с данными о людях второй подгруппы

#define weekInfo_c 'B'                                                                            //информация из заглавной ячейки недели (данные ячейки)
#define weekInfo_i 2

#define less_num_c 'C'                                                                            //номер первой пары первого дня (данные ячейки)
#define less_num_i 3

#define less_name_c 'C'                                                                           //имя первой пары первого дня (данные ячейки)
#define less_name_i 4

#define people_list_c 'B'                                                                         //начало списка людей подгруппы (данные ячейки)
#define people_list_i 6
//--------------------------------------РАЗНЫЕ КОНСТАНТЫ ТАБЛИЦЫ---------------------------------------------------------------------------------------------------------------------------------------------


FastBot bot(BOT_TOKEN);

byte week_off = 13;                                                                               //номер текущей недели (считая от первой недели в таблице, не от первой недели в году!)                                                   
bool semestr = true;                                                                              //осенний/летний семестр (false/true)
float Version = 0.5;                                                                              //текущая версия прошивки
byte people_in_subgr[2] = {};                                                                     //количество людей в каждой подгруппе

const String months[] = {
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

byte day_month[] = {
  31,
  28,
  31,
  30, 
  31,
  30,
  31,
  31,
  30,
  31,
  30,
  31,
};

struct SetInfo {      //структура с данными, нужными для выставления/изменения конкретной Н-ки
  String surn;          //фамилия человека
  String nki;           //строка, в которой каждый символ это либо " " либо "Н", соответственно каждой паре выбранного дня
  byte month;           //номер месяца
  byte day;             //день в месяце
  byte dayWeek;         //день недели (1-7 / понедельник-воскресенье)
  String year;          //год 
  String posC;          //символьная составлющая координаты ячейки
  int posI;             //численная составляющая координаты ячейки
  bool subgroup;        //подгруппа (false/true, 1/2 соответственно)
  String c;             //символьная составляющая ячейки с первой нкой
  int i;                //численная составляющая ячейки с первой нкой
  bool parity;          //четность/нечетность (0/1 соответственно) недели, в которой ставим Нку

} nka;

struct WeekInfo {
  byte pon_day = 0;               //число понедельника этой недели (week_info_c + 1; week_info_i) [0:1]
  byte pon_month = 0;             //месяц понедельника этой недели  (week_info_c + 1; week_info_i) [3:4]
  byte study_days = 0;            //количество учебных дней в неделе  (week_info_c; week_info_i) после /
  byte subj_num[7] = {};          //кол-во пар в учебных днях (less_mun_c; less_num_i)......
  byte *less_nums[7] = {};        //номера всех пар в дне
  bool parity;             //четная/нечетная (true/false соответственно) эта неделя  (week_info_c; week_info_i) перед /

} week[2];      //0 - неделя у 1 подгруппы, 1 - неделя 2 подгруппы

struct CoutntInfo {
  String surn;
  int surn_ind;
  int total;
  bool subgroup;
  String subject;
  byte mode;      //0 -  все предметы УП, 1 - все предметы все Нки, 2 - по отдельным предметам
} count;

void checkYear() {
  FB_Time t = bot.getTime(3);
  if (t.year % 4 == 0)  {
    if (t.year % 100 == 0)  {
      if (t.year % 400 == 0) day_month[1] = 29;
      else day_month[1] = 28;
    }
    else day_month[1] = 29;
  }
  else day_month[1] = 28;
}

struct timer_data {
  uint32_t start_millis = 0;
  int32_t message_id = 0;
  uint16_t period = 0;            //в секундах
};

class DeleteTimer {
  private:
  byte timer_size = 0;
  timer_data *ptr = nullptr;

  public:
  void add(int32_t message_id, uint16_t period) {
    if (timer_size+1 > 255)  return;                   //проверка на переполнения счетчика сообщений, обрабатываемых тайммером

    timer_data *temp = (timer_data *)realloc(ptr, (++timer_size)*sizeof(timer_data));           //выделяем память под данные нового таймера
    if (temp == nullptr)  return;                 //проверка на успешность перераспределения памяти
    ptr = temp;                                     //после проверки можно вернуть на место указатель на массив с данными
    
    ptr[timer_size-1].period = period;
    ptr[timer_size-1].start_millis = millis();
    ptr[timer_size-1].message_id = message_id; 
  }

  void tick() {
    static uint32_t tick_timer = millis(), tick_period = 200;         //период проверки сработки таймеров сделаем 200 мс
    
    if (millis() - tick_timer >= tick_period) {
      tick_timer = millis();
      bool need_delete = false;
      for (byte i = 0; i < timer_size; i++) {
        if (millis() - ptr[i].start_millis >= ptr[i].period*1000) {
          bot.deleteMessage(ptr[i].message_id);
          ptr[i].message_id = -1;
          need_delete = true;
        }
      }
      if (need_delete)  MyRealloc();
    }
  }

  void MyRealloc() {            //очищаем массив от данных обьектов, которые уже сработали
    byte delete_num = 0;

    if (ptr == nullptr)   return;           //невозможная ситуация, но пропишем и ее на всякий

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

class Sheet {
  private:

  public:
    void begin() {
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
      GSheet.setPrerefreshSeconds(10 * 60);
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

      uint32_t reset_timer = millis();
      digitalWrite(2, true);
      while (!(this->ready()))  {
        ArduinoOTA.handle();
        if (millis() - reset_timer >= 60*1000) {
          ESP.restart();
        }
      } 
      digitalWrite(2, false);

      
      for (byte i = 0; i < 2; i++) {
        String get_cell = "", range = "";
        
        //------------Получаем краткую информацию с заглавной ячейки недели-------------
        if (!i) range += Sheet1;
        else range += Sheet2;
        range += weekInfo_c;
        range += (weekInfo_i + (offset[i]*(week_off-1)));
        range += ":";
        range += charOffset(String(weekInfo_c), 1);
        range += (weekInfo_i + (offset[i]*(week_off-1)));
        Text answer(this->getCells(range));
        this->getBriefCellData(&week[i], answer);              //упоковал эту процедуру в функцию - для сокращения кода здесь и возможности повторного использования где-нибудь потом
        //------------Получаем краткую информацию с заглавной ячейки недели-------------
        

        //----------------------Дата понедельника этой недели---------------------------
        Text ans = answer.getSub(r_count+r_offset, "\"").getSub(1, ", ");
        
        String firstDayName = answer.getSub(r_count+r_offset, "\"").getSub(0, ", ");        //имя первого дня этой недели (может быть не понедельник)    непонятно, нужна ли эта фигня №1
        
        for (byte iter = 0; iter < ans.count("."); iter++)  {
          Text cell = ans.getSub(iter, ".");
          for (byte q = 0; q < cell.length(); q++) {
            if (iter == 0)  week[i].pon_day = (week[i].pon_day * 10 + cell[q] - '0');
            else if (iter == 1)  week[i].pon_month = (week[i].pon_month * 10 + cell[q] - '0');
          }
        }
        bot.sendMessage(String(week[i].pon_month), Admins[0]);
        if (firstDayName != "понедельник" || firstDayName == "Понедельник") {                  //непонятно, нужна ли эта фигня №2
          if (firstDayName == "вторник" || firstDayName == "Вторник")  week[i].pon_day--;
          else if (firstDayName == "среда" || firstDayName == "Среда") week[i].pon_day-=2;
          else if (firstDayName == "четверг" || firstDayName == "Четверг") week[i].pon_day-=3;
          else if (firstDayName == "пятница" || firstDayName == "Пятница") week[i].pon_day-=4;
          else if (firstDayName == "суббота"  || firstDayName == "Суббота") week[i].pon_day-=5;
          else if (firstDayName == "воскресенье" || firstDayName == "Воскресенье") week[i].pon_day-=6;
          else bot.sendMessage(F("[!CRITICAL] Неизвестное имя дня недели обнаружено в диапазоне данных первого учебного дня недели!"), Admins[0]);
        }
        //----------------------Дата понедельника этой недели---------------------------


        //-----------------------Получение номеров всех пар-----------------------------
        range = "";
        if (!i) range += Sheet1;
        else range += Sheet2;
        range += less_num_c;
        range += (less_num_i + (offset[i]*(week_off-1)));
        range += ":";
        byte len = 0;
        bool prev = false;

        for (int s = 0; s < 7; s++) {                 //ищем горизонтальную длину len строки, содержащей номера всех пар
          if (week[i].subj_num[s] == 0) continue;
          if (prev) len += 1;
          len += week[i].subj_num[s];
          prev = true;
        }

        range += charOffset(String(less_num_c), len);
        range += (less_num_i + (offset[i]*(week_off-1)));
        Text answa(this->getCells(range));
        byte faza = 0, supp = 0, iteration = 0;
        for (int s = 0; s < len; s++) {
          Text this_cell = answa.getSub(r_count + r_offset*s, "\"");
          byte s_num = week[i].subj_num[faza];
          if (!s_num) s_num++;

          if (supp != s_num) {
            week[i].less_nums[faza][iteration] = this_cell.toInt();
            supp++;
            iteration++;
          }

          else {
            supp = 0;
            faza++;
            iteration = 0;
          }
        }

        /*for (int b = 0; b < 7; b++) {                     //вывод, оставим на случай отладки
          byte ii = week[i].subj_num[b];
          if (!ii)  ii++;
          for (int d = 0; d < ii; d++) {
            bot.sendMessage(String(week[i].less_nums[b][d]));
          }
          bot.sendMessage("----------");
        }*/
        //-----------------------Получение номеров всех пар-----------------------------
      }

      checkTableWeek();        //после вытягивания всех данных проверяем их на валидность текущей дате
    }

    void getBriefCellData(WeekInfo *some_week, Text answer) {           //разбирает ячейку с сокращенной информацией о неделе в структуру WeekInfo
      Text ans = answer.getSub(r_count, "\"");
      String get_cell = "";
      for (byte iter = 0; iter < ans.count("/"); iter++) {
        ans.getSub(iter, "/").toString(get_cell);
        get_cell.toLowerCase();
        Text cell(get_cell);
        if (!iter)  {
          if (cell == "числитель") some_week->parity = false;
          else some_week->parity = true;
        }

        else if (iter == 1) {
          some_week->study_days = cell.toInt();
        }

        else { 
          if (cell.toInt() > sizeof(lessons)/sizeof(lessons[0]))  bot.sendMessage("Не для всех пар в " + String(iter-1) + " день удается найти временные рамки! Недостаточно описанных временных рамок пар в структуре \"lessons\", чтобы обрабатывать сокращенный ввод в данный день!", error_chat);
          some_week->subj_num[iter-2] = cell.toInt();
          some_week->less_nums[iter-2] = new byte[some_week->subj_num[iter-2]]{};                  //выделяем под каждый день с N парами в этот день ровно N ячеек (для хранения номеров каждой пары в каждый день)
        }
      }
    }

    String getCells(String range) {
      byte tries = 0;
      String answ;
      while (!GSheet.values.get(&answ, spreadsheetId, range) && tries < GetTryNum) {
        tries++;
      }

      if (tries == GetTryNum) bot.sendMessage("getError", error_chat);

      return answ;
    }

    void SetN(String range) {
      String answ = "";
      byte tries = 0;

      FirebaseJson valueRange;
      valueRange.add("range", range);
      valueRange.add("majorDimension", "ROWS");

      for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) {
        String address = "values/[0]/[", data = "";
        address += i;
        address += "]";
        if (nka.nki[i] == ' ')  data = "";
        else if (nka.nki[i] == '+') data = "R";
        else data = "D";
        valueRange.set(address, data);
      }
      
      while (!GSheet.values.update(&answ, spreadsheetId, range, &valueRange) && tries < SetTryNum) {
        tries++;
      }

      if (tries == SetTryNum) bot.sendMessage("updateError");
      valueRange.clear();
    }

    void Counting() {
      if (!count.mode || count.mode == 1)  {         //все предметы УП (R) ИЛИ все предметы все Н
        for (int i = 1; i < week_off+1; i++) {
          String range = "";
          if (!count.subgroup)  range += Sheet1;
          else range += Sheet2;
          range += charOffset(String(people_list_c), 1);
          range += people_list_i + count.surn_ind + offset[count.subgroup]*(i-1);
          range += ":";
          byte plus = 0;

          for (byte j = 0; j < 7; j++) {
            plus += week[count.subgroup].subj_num[j] + 1;
          }

          range += charOffset(String(people_list_c), 1+plus);
          range += people_list_i + count.surn_ind + offset[count.subgroup]*(i-1);
          Serial.println(range);
        }
      }

      if (count.mode == 2)   {        //по отдельным предметам

      }
    }

    bool ready() {
      return GSheet.ready();
    }

} list;

class Menu {
  private:
    int32_t menu_id[sizeof(Admins)/sizeof(Admins[0])] = {};
    bool ret_command = false, reading_flag = true;
    byte nka_ind = 0;
    String s_menu[2] = {"Редактировать", "Подсчитать"};
    String way = "10000";

  public:
    void start_page(bool mode) {
      if (way == "10000") way = "0";

      if (!mode)  {
        for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
          bot.sendMessage("______________ИСиТенок_v" + String(Version, 1) + "_____________", Admins[i]);
        }
        return;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        if (menu_id[i]) bot.editMenu(menu_id[i], s_menu[0] + "\t" + s_menu[1], Admins[i]);
        else {
          bot.inlineMenu("Выберите:", s_menu[0] + "\t" + s_menu[1], Admins[i]);
          menu_id[i] = bot.lastBotMsg();
        }
      }
    }

    void menuEdit (String comm, String user) {
      FB_Time t = bot.getTime(3);

      if (comm == "На главную") {
        way = "0";
        ret_command = true;
      }
      if (comm == "Назад")  {
        way.remove(way.length()-1);
        ret_command = true;
      }

      if (way == "0") {                   //отображается стартовая страница
        if (comm == s_menu[0]) {          //нажали кнопку "Редактирование"
          way = "01";
          edit_page(0);
          return;
        }

        if (comm == s_menu[1]) {      //нажали кнопку "Подсчет"
          way = "02";
          calculate_page(0);
          return;
        }

        if (ret_command)  {
          ret_command = false;
          start_page(1);
        }
        
        else  bot.sendMessage("err_menu", error_chat);
      }

      if (way.startsWith("01")) {                                                     //ветка редактирования 
        if (way == "01") {                                                            //отображается страница выбора фамилии
          nka.surn = "";
          nka.nki = "";
          nka.month = t.month;
          nka.day = t.day;
          nka.year = t.year;
          nka.dayWeek = t.dayWeek;
          nka.c = 'A';
          nka.i = 0;
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

        else if (way == "011") {                                                           //возможность поставить Н, или перейти к выбору другой даты
          if (comm.startsWith("Дата:")) {
            way = "0111";
            edit_page(2);
            return;
          }

          else if (comm.startsWith("(")) {
            for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) {
              if (String(comm[1]) == String(week[nka.subgroup].less_nums[nka.dayWeek-1][i])) {
                edit_page(4);
                way = "011111";
                nka_ind = i;
                return;
              }
            }
          }

          else if (comm == "Все УП") {
            for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) nka.nki[i] = '+';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Все неУП") {
            for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) nka.nki[i] = '-';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Нет пропусков") {
            for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) nka.nki[i] = ' ';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Поставить") {
            String range;
            getNIndex(nka.subgroup);
            if (!nka.subgroup) range += Sheet1;
            else range += Sheet2;
            range += nka.posC;
            range += nka.posI;
            range += ":";
            range += charOffset(nka.posC, week[nka.subgroup].subj_num[nka.dayWeek-1]-1);
            range += nka.posI;
            list.SetN(range);
            way = "01";
            edit_page(0);
            return;
          }

          else if (comm.startsWith("В этот")) {
            bot.sendMessage("Чо жмешь? Сказали же, пар в выбранный день нет!", user);
            timer.add(bot.lastBotMsg(), 7);
            return;
          }

          else if (ret_command)  {
            ret_command = false;
            edit_page(1);
          }
        }

        else if (way == "0111") {                                                          //выбор месяца
          nka.month = t.month;
          nka.day = t.day;
          nka.year = t.year;
          nka.dayWeek = t.dayWeek;
          for (int i = 0; i < 12; i++) {
            if (comm == months[i]) {
              nka.month = i+1;
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

        else if (way == "01111") {                                                         //выбор дня в месяце
          for (int i = 1; i < day_month[nka.month-1]+1; i++) {
            if (comm == String(i)) {
              nka.day = i;
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

        else if (way == "011111") {                                                       //выбор варианта Нки
          if (comm != "Вернуться") {
            if (comm == "по УП") nka.nki[nka_ind] = '+';
            else if (comm == "по неУП") nka.nki[nka_ind] = '-';
            else nka.nki[nka_ind] = ' ';
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

      if (way.startsWith("02")) {         //ветка подсчета
        if (way == "02") {
          nka.surn = "";
          nka.nki = "";
          nka.month = t.month;
          nka.day = t.day;
          nka.year = t.year;
          nka.dayWeek = t.dayWeek;
          nka.c = 'A';
          nka.i = 0;
          byte l = 0;
          for (byte i = 0; i < sizeof(students)/sizeof(students[0]); ++i) {
            if (comm == students[i].surname)  {
              count.surn = students[i].surname;
              count.subgroup = students[i].subgroup;
              if (!students[i].subgroup) count.surn_ind = l;
              else count.surn_ind = i-l+1;
              way = "021";
              calculate_page(1);
              return;
            }
            if (!students[i].subgroup) l++;
          }

          if (ret_command)  {
            ret_command = false;
            calculate_page(0);
          }
        }

        if (way == "021") {
          if (comm == "Общее УП") count.mode = 0;
          else if (comm == "Общее все Н") count.mode = 1;
          else if (comm == "По предметам") {
            count.mode = 2;
            way = "02111";
            calculate_page(2);
            return;
          }
          list.Counting();
          way = "0211";
          calculate_page(3);
          return;

          if (ret_command)  {
            ret_command = false;
            calculate_page(1);
          }
        }

        if (way == "02111") {
          for (byte i = 0; i < sizeof(subjects)/sizeof(subjects[0]); i++) {
            if (comm == subjects[i]) {
              count.subject = comm;
              list.Counting();
              calculate_page(3);
              way = "0211";
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
    }

    void edit_page(byte edit_depth) {
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
          String range = "", answ;
          mess = nka.surn;
          mess += "\t";
          mess += nka.subgroup+1;
          mess += " подгруппа";
          mess +=  "\tДата: ";
          if (nka.day < 10) mess += "0";
          mess += nka.day;
          mess += ".";
          if (nka.month < 10) mess += "0";
          mess += nka.month;
          mess += ".";
          mess += nka.year[2];
          mess += nka.year[3];
          mess += "\n";
          getNIndex(nka.subgroup);
          if (week[nka.subgroup].subj_num[nka.dayWeek-1])  {
            if (reading_flag) {
              nka.nki = "";                                                 //разобраться, почему нужна эта заплатка и починить (если очень захочется :) )
              if (!nka.subgroup) range += Sheet1;
              else range += Sheet2;
              range += nka.posC;
              range += nka.posI;
              range += ":";
              range += charOffset(nka.posC, week[nka.subgroup].subj_num[nka.dayWeek-1]-1);
              range += nka.posI;
              answ = list.getCells(range);
              Text answer(answ);
              for (byte i = 0; i < (week[nka.subgroup].subj_num[nka.dayWeek-1]); i++) {
                String a = "";
                answer.getSub(r_count + r_offset*i, "\"").toString(a);
                if (a == "R")  nka.nki += "+";
                else if (a == "D") nka.nki += "-";
                else nka.nki += " ";
              }
            }

            for (byte i = 0; i < week[nka.subgroup].subj_num[nka.dayWeek-1]; i++) {
              mess += "(";
              mess += week[nka.subgroup].less_nums[nka.dayWeek-1][i];
              mess += ") ";
              if (nka.nki[i] == '-')  mess += Disrep;
              else if (nka.nki[i] == '+') mess += Respect;
              else mess += " ";
              if (i != week[nka.subgroup].subj_num[nka.dayWeek-1]-1) mess += "\t";
              else mess += "\n";
            }

            mess += "Все УП\tВсе неУП\tНет пропусков\n";
            mess += "Назад\tНа главную\tПоставить";
          }
          else {
            mess += "В этот день пар нет!\n";
            mess += "Назад\tНа главную";
          }
          reading_flag = true;
        }
        break;

        case 2: {
          mess = "";
          if (semestr) {
            for (int i = START_MONTH; i < t.month+1; i++) {
              mess += months[i-1];
              if (i % 3 == 1 || i == t.month) mess += "\n";
              else mess += "\t";
            }
          }

          if (!semestr) {
            for (int i = START_MONTH; i < t.month+1; i++) {
              mess += months[i-1];
              if (i % 3 == 1 || i == t.month) mess += "\n";
              else mess += "\t";
            }
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
          if (nka.month == START_MONTH) k = START_DAY;
          byte day_n = nka.day;                   
          byte dayWeek_n = nka.dayWeek;
          nka.day = k;
          getNIndex(nka.subgroup);
          byte pre_offset = nka.dayWeek-1;
          byte post_offset;
          if (nka.month == t.month) nka.day = day_n;
          else  nka.day = day_month[nka.month-1];
          getNIndex(nka.subgroup);
          post_offset = 7 - nka.dayWeek;
          nka.day = day_n;
          nka.dayWeek = dayWeek_n;

          mess += "-пн-\t-вт-\t-ср-\t-чт-\t-пт-\t-сб-\t-вс-\n";

          for (byte i = 0; i < pre_offset; i++) mess += " \t";

          for (byte i = k-1; i < day_month[nka.month-1]; i++) {
            mess += i+1;
            if ((i+pre_offset-k) % 7 == 5)  mess += "\n";
            else mess += "\t";
            if (nka.month == t.month && i+1 == t.day) break;
          }

          for (byte i = 0; i < post_offset; i++)  {
            if (i != post_offset-1) mess += " \t";
            else mess += " \n";
          }
          
          mess += "Назад\tНа главную";
        }
        break;

        case 4: {
          mess = "Пропуск:\nпо УП\tпо неУП\tПрисутствие\nВернуться";
        }
        break;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        bot.editMenu(menu_id[i], mess, Admins[i]);
      }
    }

    void calculate_page(byte calculate_depth) {
      String mess = "";
      switch (calculate_depth) {
        case 0:
            mess = "";
            for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
              mess += students[i].surname;
              if (i % 3 == 2 || i == (sizeof(students)/sizeof(students[0]))-1) mess += "\n";
              else mess += "\t";
            }
            mess += "На главную";
        break;

        case 1:
          mess = "";
          mess += count.surn;
          mess += "\n";
          mess += "Общее УП\tОбщее все Н\tПо предметам\n";
          mess += "Назад\tНа главную";
        break;

        case 2:
          mess = "Введите предмет:\n";
          for (byte i = 0; i < sizeof(subjects)/sizeof(subjects[0]); i++) {
              mess += subjects[i];
              if (i % 3 == 2 || i == (sizeof(subjects)/sizeof(subjects[0]))-1) mess += "\n";
              else mess += "\t";
          }
          mess += "Назад\tНа главную";
        break;

        case 3:
          mess = "Итог подсчета:\n";
          mess += count.total;
          mess += "\nНа главную";
        break;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        bot.editMenu(menu_id[i], mess, Admins[i]);
      }
    }
} menu;

void setup() {
  Serial.begin(115200);                                                       //последовательный порт аааткрывать
  WiFi_Connect();                                                             //подключаемся к WiFi
  bot.attach(newMsg);                                                         //подключаем обработчик входящих сообщений
  bot.setPeriod(50);                                                          //период между проверками входящих сообщений
  pinMode(2, OUTPUT);

  bot.clearServiceMessages(true);
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.begin();

  for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) people_in_subgr[((!students[i].subgroup) ? 0 : 1)]++;       //считаем количество людей в каждой подгруппе самым изощренным способом

  menu.start_page(0);       //чисто для обновления времени
  list.begin();
  menu.start_page(1);       //вот тут уже достраиваем стартовую страницу окончательно
  checkYear();
}

void loop() {
  static int old_year = 0;
  bot.tick();
  timer.tick();
  ArduinoOTA.handle();
  FB_Time t = bot.getTime(3);

  if (t.year && !old_year)  old_year = t.year;        //Запоминаем год при запуске только после того, как время синхронизировано. Возможно в будущем заменим записью в EEPROM 
  else if (old_year != t.year)  checkYear();          //Если год сменился - опа, произошел новый год, то проверяем на високосность

}