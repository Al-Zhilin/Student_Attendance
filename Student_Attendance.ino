#define ATOMIC_FS_UPDATE      // поддержка сжатых прошивок из чата

#include <FastBot.h>
#include <FileData.h>
#include <FFat.h>
#include <ESP_Google_Sheet_Client.h>
#include <StringUtils.h>
#include <ArduinoOTA.h>
#include "types.h"
#include <settings.h>

FastBot bot(BOT_TOKEN);
                                                   
float Version = 0.5;                                                                              //текущая версия прошивки
byte people_in_subgr[2] = {};                                                                     //количество людей в каждой подгруппе

struct fileData {                                                 // структуры настроек, записывамых в энергонезависимую память
  byte week_off = 2;                                              // номер текущей недели (считая от первой недели в таблице, не от первой недели в году!)
  int32_t status_mess[sizeof(Admins)/sizeof(Admins[0])] = {};     // id статусного сообщеня в каждом чате
  int32_t menu_id[sizeof(Admins)/sizeof(Admins[0])] = {};         // id меню в каждом чате

} file_data;

FileData settings_file(&FFat, "/data.dat", 'B', &file_data, sizeof(file_data));

const String months[] = {               //сокращенные названия всех месяцев
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

byte day_month[] = {        //количество дней в каждом месяце года. Для високосного есть отдельная функция
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

struct Date {
  byte day = 0;
  byte month = 0;

  Date(byte dday, byte mmonth) : day(dday), month(mmonth) {
    if (mmonth < 1 || mmonth > 12) {
      bot.sendMessage(F("InvalidMonthInDateConstructor!"), error_chat);
      month = 0;
    }
    if (dday < 1 || dday > day_month[mmonth]) {
      bot.sendMessage(F("InvalidDayInDateConstructor!"), error_chat);
      day = 0;
    }    
  }

  Date() : day(0), month(0) {};
};

String PROGMEM DaysOfWeek[] = {
  "Понедельник",
  "Вторник",
  "Среда",
  "Четверг",
  "Пятница",
  "Суббота",
  "Воскресенье",
};

struct SetInfo {      //структура с данными, нужными для выставления/изменения конкретной Н-ки и/или массива Нок. В обоих случаях используем эту структуру
  String surn;          //фамилия человека
  String nki;           //строка, в которой каждый индекс строки обозначает тип пропуска, соответственно каждой паре выбранного дня
  Date date;            //день и месяц выставления Нки
  byte dayWeek;         //день недели (1-7 / понедельник-воскресенье)
  String year;          //год 
  String posC;          //символьная составлющая координаты ячейки
  int posI;             //численная составляющая координаты ячейки
  bool subgroup;        //подгруппа (false/true, 1/2 соответственно)
  bool parity;          //четность/нечетность (0/1 соответственно) недели, в которой ставим Нку
} nka;

struct WeekInfo {
  Date pon_date;                  //дата понедельника этой недели
  byte study_days = 0;            //количество учебных дней в неделе  (week_info_c; week_info_i) после /
  byte subj_num[7] = {};          //кол-во пар в учебных днях (less_mun_c; less_num_i)......
  byte *less_nums[7] = {};        //номера всех пар в дне
  bool parity;             //четная/нечетная (true/false соответственно) эта неделя  (week_info_c; week_info_i) перед /

} week_object[4];      //0 - неделя у 1 подгруппы, 1 - неделя 2 подгруппы

WeekInfo *week[4] = {&week_object[0], &week_object[1], &week_object[2], &week_object[3]};           //week[4] - массив указателей на обьекты структуры WeekInfo. 0 и 1 - для настоящей четности, а 2 и 3 - для противоположной у обоих подгрупп

struct CountInfo {
  String surn;
  int surn_ind;
  int total;
  bool subgroup;
  String subject;
  byte mode;      //0 -  все предметы УП, 1 - все предметы неУП, 2 - по отдельным предметам неУП
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
    void begin() {                                  // is_start обозначает, вызывается ли эта функция в начала работы программы или после очередной проверки актульность недели во время работы
      
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);
      GSheet.setPrerefreshSeconds(10 * 60);
      GSheet.begin(CLIENT_EMAIL, PROJECT_ID, PRIVATE_KEY);

      editServiceMess("Подключаюсь к Google Sheet API...");

      uint32_t reset_timer = millis();
      //digitalWrite(2, true);
      while (!(this->ready()))  {
        ArduinoOTA.handle();
        if (millis() - reset_timer >= 60*1000) {
          ESP.restart();
        }
      }
      //digitalWrite(2, false);

      editServiceMess("Google Sheet API успешно подключено!\nПолучаю информацию о текущей неделе...");

      for (byte i = 0; i < 4; i++) {
        String get_cell = "", range = "", returned_string;
        byte parity_offset = 1;                       //бывает 1 или 2, показывает, парсим данные из недели последней или предыдущей четности соответственно
        if (i > 1) parity_offset = 2;

        //------------Получаем краткую информацию с заглавной ячейки недели-------------
        if (i % 2 == 0) range += Sheet1;
        else range += Sheet2;
        range += weekInfo_c;
        range += (weekInfo_i + (offset[i % 2]*(file_data.week_off-parity_offset)));
        range += ":";
        range += charOffset(String(weekInfo_c), 1);
        range += (weekInfo_i + (offset[i % 2]*(file_data.week_off-parity_offset)));
        returned_string = this->getCells(range);
        Text answer(returned_string);
        Text ans = answer.getSub(r_count, "\"");

        for (byte iter = 0; iter < ans.count("/"); iter++) {
          ans.getSub(iter, "/").toString(get_cell);
          Text cell(get_cell);
          if (!iter)  {
            if (cell == "числитель" || cell == "Числитель") week[i]->parity = false;
            else week[i]->parity = true;
          }

          else if (iter == 1) {
            week[i]->study_days = cell.toInt();
          }

          else { 
            if (cell.toInt() > sizeof(lessons)/sizeof(lessons[0]))  bot.sendMessage("Не для всех пар в " + String(iter-1) + " день удается найти временные рамки! Недостаточно описанных временных рамок пар в структуре \"lessons\", чтобы обрабатывать сокращенный ввод в данный день!", error_chat);
            week[i]->subj_num[iter-2] = cell.toInt();
            if (!week[i]->subj_num[iter-2])  {                       //если в этот день пар нет, все равно выделяем 1 элемент, чтобы там был инициализирован 0. Возможно нужно в некоторых случаях, хезе кароч
              week[i]->less_nums[iter-2] = new byte[1]{};
            }
            else  week[i]->less_nums[iter-2] = new byte[week[i]->subj_num[iter-2]]{};                  //выделяем под каждый день с N парами в этот день ровно N ячеек (для хранения номеров каждой пары в каждый день)
          }
        }
        //------------Получаем краткую информацию с заглавной ячейки недели-------------


        //----------------------Дата понедельника этой недели---------------------------
        ans = answer.getSub(r_count+r_offset, "\"").getSub(1, ", ");
        
        String firstDayName = answer.getSub(r_count+r_offset, "\"").getSub(0, ", ");        //имя первого дня этой недели (может быть не понедельник)    непонятно, нужна ли эта фигня №1
        
        for (byte iter = 0; iter < ans.count("."); iter++)  {
          Text cell = ans.getSub(iter, ".");
          for (byte q = 0; q < cell.length(); q++) {
            if (iter == 0)  week[i]->pon_date.day = (week[i]->pon_date.day * 10 + cell[q] - '0');
            else if (iter == 1)  week[i]->pon_date.month = (week[i]->pon_date.month * 10 + cell[q] - '0');
          }
        }

        if (firstDayName != "понедельник" || firstDayName == "Понедельник") {                  //непонятно, нужна ли эта фигня №2       !!!Переделать с помощью enum дней недели!!!
          if (firstDayName == "вторник" || firstDayName == "Вторник")  week[i]->pon_date.day--;
          else if (firstDayName == "среда" || firstDayName == "Среда") week[i]->pon_date.day-=2;
          else if (firstDayName == "четверг" || firstDayName == "Четверг") week[i]->pon_date.day-=3;
          else if (firstDayName == "пятница" || firstDayName == "Пятница") week[i]->pon_date.day-=4;
          else if (firstDayName == "суббота"  || firstDayName == "Суббота") week[i]->pon_date.day-=5;
          else if (firstDayName == "воскресенье" || firstDayName == "Воскресенье") week[i]->pon_date.day-=6;
          else {
            bot.sendMessage("Неизвестное имя дня недели обнаружено в диапазоне данных первого учебного дня недели: \"" + firstDayName + "\"!\n\nОтвет от Sheet: \"" + answer.toString() + "\"", Admins[0]);
            ESP.restart();
          }
        }
        //----------------------Дата понедельника этой недели---------------------------


        //-----------------------Получение номеров всех пар-----------------------------
        range = "";
        if (i % 2 == 0) range += Sheet1;
        else range += Sheet2;
        range += less_num_c;
        range += (less_num_i + (offset[i % 2]*(file_data.week_off-parity_offset)));
        range += ":";

        byte len = 0;
        bool prev = false;

        for (int s = 0; s < 7; s++) {                 //ищем горизонтальную длину len строки, содержащей номера всех пар
          if (week[i]->subj_num[s] == 0) continue;
          if (prev) len += 1;
          len += week[i]->subj_num[s];
          prev = true;
        }

        range += charOffset(String(less_num_c), len-1);
        range += (less_num_i + (offset[i % 2]*(file_data.week_off-parity_offset)));
        returned_string = this->getCells(range);
        Text answa(returned_string);

        byte job_day = 0;                            //отображает дни недели 0...6 который сейчас заполняем, обеспечивает их "смену" в цикле
        byte lesson_in_day = 0;                        //отоюражает обрабатываемую пару в какой-либо день

        for (int s = 0; s < len; s++) {
          Text this_cell = answa.getSub(r_count + r_offset*s, "\"");

          if (String(this_cell) == "") {                //если попался разделитель между днями - переходим на следующий день
            job_day++;
            lesson_in_day = 0;
            continue;
          }

          byte lesson_count = week[i]->subj_num[job_day];

          while (!lesson_count) {               //пока пар в этот день нет
            job_day++;
            lesson_count = week[i]->subj_num[job_day];       //ищем день, в который они есть
          }

          //bot.sendMessage(String(this_cell) + " / " + String(lesson_count) + " / " + String(job_day),  error_chat);

          week[i]->less_nums[job_day][lesson_in_day] = this_cell.toInt();
          lesson_in_day++;
        }

        
        /*for (int b = 0; b < 7; b++) {                     //вывод, оставим на случай отладки
          byte ii = week[i]->subj_num[b];
          if (!ii)  ii++;
          for (int d = 0; d < ii; d++) {
            bot.sendMessage(String(week[i]->less_nums[b][d]), error_chat);
          }
          bot.sendMessage("----------", error_chat);
        }*/
        //-----------------------Получение номеров всех пар-----------------------------
      }
      checkTableWeek();                                                 //проверяем неделю на актуальность
    }


    String getCells(String range) {                 // функция получения Нок из таблицы (чтобы в меню отображать)
      byte tries = 0;
      String answ;
      while (!GSheet.values.get(&answ, spreadsheetId, range) && tries < GetTryNum) {
        tries++;
      }

      if (tries == GetTryNum) bot.sendMessage("getError", error_chat);

      return answ;
    }

    void SetN(String range) {                       // базовая функция постановки Нок для одного человека в один день
      String answ = "";
      byte tries = 0;

      FirebaseJson valueRange;
      valueRange.add("range", range);
      valueRange.add("majorDimension", "ROWS");

      for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) {
        String address = "values/[0]/[", data = "";
        address += i;
        address += "]";
        if (nka.nki[i] == ' ')  data = "";
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

    void Counting(byte start_week = 1, byte end_week = file_data.week_off) {            // номера недель, ограничивающих область подсчета, нужно для подсчета только конкретного диапазона
      if (!count.mode || count.mode == 1)  {                                  // все предметы УП ИЛИ все предметы неУП
        String formula = "", diapason = "";                                      // строка для сборки формулы имеет конечный вид =СЧЁТЕСЛИ(FILTER(C581:U617; ОСТАТ(СТРОКА(C581:C617)-588; 23)=0);"D")
        byte table_len[2] = {};                                                                       // горизонтальная длина таблицы
        bool prev = false;

        for (byte parity_iter = 0; parity_iter < 2; parity_iter++) {          // Высчитываем len (горизонталную длины недели в таблице)
          for (int s = 0; s < 7; s++) {
            if (week[count.subgroup + 2*parity_iter]->subj_num[s] == 0) continue;
            if (prev) table_len[parity_iter] += 1;
            table_len[parity_iter] += week[count.subgroup + 2*parity_iter]->subj_num[s];
            prev = true;
          }
        }

        // === Собираем диапазон ===
        diapason += less_name_c;                                        // символьное начало диапазона
        diapason += people_list_i + offset[count.subgroup] * (start_week-1);        // численное начало диапазона
        diapason += ":";
        diapason += charOffset(String(less_name_c), max(table_len[0], table_len[1])-1);
        diapason += people_list_i + offset[count.subgroup] * (end_week-1) + people_in_subgr[count.subgroup] - 1;
        // === Собираем диапазон ===


        // === Собираем саму формулу ===
        formula += "=СЧЁТЕСЛИ(FILTER(";
        formula += diapason;
        formula += "; ОСТАТ(СТРОКА(";
        formula += diapason;
        formula += ")-";
        formula += people_list_i + offset[count.subgroup] * (start_week-1) + count.surn_ind;
        formula += "; ";
        formula += offset[count.subgroup];
        formula += ")=0); \"";
        formula += (!count.mode) ? RESPECT_SYMBOL : DISREP_SYMBOL;                                // в зависимости от вида поиска ищем конкретный символ
        formula += "\")";

        
        // == Находим позицию вставки формулы в листе ===
        String form_position = (!count.subgroup) ? Sheet1 : Sheet2;
        form_position += charOffset(String(less_name_c), max(table_len[0], table_len[1]) + 4);
        form_position += people_list_i + offset[count.subgroup] * (end_week-1) + people_in_subgr[count.subgroup] - 1;

        
        // === Устанавливаем формулу в листе ===
        FirebaseJson response, valueRange;
        valueRange.add("range", form_position);
        valueRange.add("majorDimension", "ROWS");
        valueRange.set("values/[0]/[0]", formula);

        byte tries = 0;
        while (!GSheet.values.update(&response, spreadsheetId, form_position, &valueRange) && tries < SetTryNum) tries++;
        if (tries == SetTryNum) bot.sendMessage("updateError");
        valueRange.clear();

        /*String responseStr;
        response.toString(responseStr, true);                 //Вывод ответа от Google Sheets API для отладки
        bot.sendMessage(responseStr, error_chat);*/

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

      else if (count.mode == 2)   {        //по отдельным предметам неУП

      }

      else bot.sendMessage("Неизвестный count.mode!", error_chat);
    }

    bool ready() {
      return GSheet.ready();
    }

} list;

class Menu {
  private:
    bool ret_command = false, reading_flag = true;
    byte nka_ind = 0;
    String s_menu[2] = {"Редактировать", "Подсчитать"};
    String way = "10000";
    byte start_week_ind = 0, end_week_ind = 0, unknown_ind = 0;

  public:
    void start_page(bool mode, FDstat_t file_status = FD_NO_DIF) {              // file_status отображает статус работы с файлом настроек, нужен для понимания - отправлять или подтягивать сообщения у пользователей
      if (way == "10000") way = "0";

      if (!mode)  {
        for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
          if (file_status == FD_WRITE || file_status == FD_ADD) {
            bot.sendMessage("ИСиТенок v" + String(Version, 1), Admins[i]);
            file_data.status_mess[i] = bot.lastBotMsg();
          }
          else bot.editMessage(file_data.status_mess[i], "ИСиТенок v" + String(Version, 1), Admins[0]);
        }
        settings_file.update();
        return;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
        if (file_status == FD_WRITE || file_status == FD_ADD) {
          bot.inlineMenu("Выберите:", s_menu[0] + "\t" + s_menu[1], Admins[i]);
          file_data.menu_id[i] = bot.lastBotMsg();
        }
        else  bot.editMenu(file_data.menu_id[i], s_menu[0] + "\t" + s_menu[1], Admins[i]);
      }
      settings_file.update();
    }

    friend void editServiceMess(String edit_text);              //функция редактирования "статусного" сообщения

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

        if (ret_command)  {
          ret_command = false;
          start_page(1);
        }
        
        else  bot.sendMessage("err_menu", error_chat);
      }

      if (way.startsWith("01")) {                                                     // ветка редактирования 
        if (way == "01") {                                                            // отображается страница выбора фамилии
          nka.surn = "";
          nka.nki = "";
          nka.date.month = t.month;
          nka.date.day = t.day;
          nka.year = t.year;
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
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += '+';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Все неУП") {                                               // выбрал "поставить неУП на все пары в дне"
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += '-';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Нет пропусков") {                                          // выбрал "убрать пропуски на всех парах в дне"
            nka.nki = "";
            for (byte i = 0; i < week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]; i++) nka.nki += ' ';
            reading_flag = false;
            edit_page(1);
            return;
          }

          else if (comm == "Поставить") {                                              // поставить введенные Нки
            String range;
            getNIndex();                              //подумать, нужно ли оно тут
            if (!nka.subgroup) range += Sheet1;
            else range += Sheet2;
            range += nka.posC;
            range += nka.posI;
            range += ":";
            range += charOffset(nka.posC, week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[nka.dayWeek-1]-1);
            range += nka.posI;
            list.SetN(range);
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
          nka.year = t.year;
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
          for (int i = 1; i < day_month[nka.date.month-1]+1; i++) {
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

      if (way.startsWith("02")) {                                  // ветка подсчета
        if (way == "02") {
          nka.surn = "";
          nka.nki = "";
          nka.date.month = t.month;
          nka.date.day = t.day;
          nka.year = t.year;
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

          if (ret_command)  {
            ret_command = false;
            calculate_page(1);
          }

          return;
        }

        if (way == "0212") {                     // нажата кнопка на меню выбора диапазона подсчета          
          if (comm == "Готово") {
            list.Counting(start_week_ind, end_week_ind);
            calculate_page(5);
          }

          else if (ret_command)  {
            ret_command = false;
            calculate_page(3);
          }

          else {                                      // обрабатывааем нажатия на неделю
            // здесь надо суметь вычислить индекс в глобальном пространстве индексов недель [1; week_off] и засунуть его в unknown_ind
            // здесь имеем comm = ~ "с 23.03 по 30.03"

            int8_t c_index = comm.indexOf("с");                                   // в любой строке индекс начала значащей части (без значков и отступов)

            if (c_index == -1)   {                                                // на прям крайняк
              bot.sendMessage(F("invalidMenuTextInCount!"), error_chat);
              return;
            }

            Date startDate, endDate;
            startDate.day = (comm[c_index+3] - '0')*10 + (comm[c_index+4] - '0');
            startDate.month = (comm[c_index+6] - '0')*10 + (comm[c_index+7] - '0');

            bot.sendMessage(String(startDate.day) + "." + String(startDate.month), error_chat);

            calculate_page(4);                        // страница выбора статуса недели (Начало диапазона, конец или только эта неделя)
            way = "02121";
          }

          return;
        }

        else if (way == "02121") {                    // нажатия на странице выбора статуса недели (Начало диапазона, конец или только эта неделя)
          if (comm == "Начало") start_week_ind = unknown_ind;
          else if (comm == "Конец") end_week_ind = unknown_ind;
          else if (comm == "Только эта неделя") {
            start_week_ind = unknown_ind;
            end_week_ind = unknown_ind;
          }
        }

        else if (way == "0211") {                     // выбор предмета для подсчета
          for (byte i = 0; i < sizeof(subjects)/sizeof(subjects[0]); i++) {
            if (comm == subjects[i]) {
              count.subject = comm;
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
          if (nka.date.day < 10) mess += "0";
          mess += nka.date.day;
          mess += ".";
          if (nka.date.month < 10) mess += "0";
          mess += nka.date.month;
          mess += ".";
          mess += nka.year[2];
          mess += nka.year[3];
          mess += "\n";
          getNIndex();
          byte week_index = nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2);                               //индекс недели, складывается из подгруппы и сдвига на неделю, соответствующую выставляемым Нкам по четности
          if (week[week_index]->subj_num[nka.dayWeek-1])  {               //если в этот день пары есть (в день, соответственной Нке по четности, недели)
            if (reading_flag) {
              nka.nki = "";                                                 //разобраться, почему нужна эта заплатка и починить (если очень захочется :) )
              if (!nka.subgroup) range += Sheet1;
              else range += Sheet2;
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
              else mess += " ";
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
          reading_flag = true;
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
          else  nka.date.day = day_month[nka.date.month-1];
          getNIndex();
          post_offset = 7 - nka.dayWeek;
          nka.date.day = day_n;
          nka.dayWeek = dayWeek_n;

          mess += "-пн-\t-вт-\t-ср-\t-чт-\t-пт-\t-сб-\t-вс-\n";

          for (byte i = 0; i < pre_offset; i++) mess += " \t";

          for (byte i = k-1; i < day_month[nka.date.month-1]; i++) {
            mess += i+1;
            if ((i+pre_offset-k) % 7 == 5)  mess += "\n";
            else mess += "\t";
            if (nka.date.month == t.month && i+1 == t.day) break;
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
        bot.editMenu(file_data.menu_id[i], mess, Admins[i]);
      }
    }

    void calculate_page(byte calculate_depth) {
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
          start_week_ind = 1;
          end_week_ind = file_data.week_off;
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
          Date date_start(week[0]->pon_date.day, week[0]->pon_date.month), date_end(week[0]->pon_date.day, week[0]->pon_date.month);
          sumDate(&date_end, 6);

          for (byte i = 0; i < file_data.week_off; i++) {

            sumDate(&date_start, -7);                     // отодвигаем дату назад на неделю
            sumDate(&date_end, -7);
            
            if (start_week_ind == i+1)  {
              mess += START_SYMBOL;
              mess += " --- ";
            }

            else if (end_week_ind == i+1) {
              mess += END_SYMBOL;
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

            if (start_week_ind == i+1)  {
              mess += " --- ";
              mess += START_SYMBOL;
            }

            else if (end_week_ind == i+1) {
              mess += " --- ";
              mess += END_SYMBOL;
            }

            if (i != file_data.week_off-1) mess += "\n";
          }
          break;
        }

        case 4:
          mess = "Эта неделя ... диапазона:\nНачало\tКонец\tНачало и конец\nНа главную\tНазад";
          break;

        case 5:                                     // страница, отображающая итог подсчета
          mess = count.surn;
          mess += "\t";
          if (!count.mode)  mess += "УП\tВсего";

          else if (count.mode == 1) mess += "неУП\tВсего";
          else if (count.mode == 2) {
            mess += "неУп\tпо \"";
            if (count.subject != "") mess += count.subject;                             //хз, на всяяякийййй
            else mess += "unknown lesson";
            mess += "\"";
          }
          mess += "\n";
          mess += count.total;
          mess += "\nНа главную";
          break;
      }

      for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {                 // обновляем страницу у всех пользователей
        bot.editMenu(file_data.menu_id[i], mess, Admins[i]);
      }
    }
} menu;

void editServiceMess(String edit_text) {              // функция редактирования "статусного" сообщения
  for (byte i = 0; i < sizeof(Admins)/sizeof(Admins[0]); i++) {
    bot.editMessage(file_data.status_mess[i], "ИСиТенок v" + String(Version, 1) + "\n\n" + edit_text, Admins[i]);
  }
}

void setup() {
  Serial.begin(115200);                                                         // последовательный порт аааткрывать
  WiFi_Connect();                                                               // подключаемся к WiFi
  bot.attach(newMsg);                                                           // подключаем обработчик входящих сообщений
  bot.setPeriod(50);                                                            // период между проверками входящих сообщений

  if (!FFat.begin()) {                                                          // подключаем файловую систему
    bot.sendMessage(F("Ошибка инициализации файловой системы!"), error_chat);
  }
  settings_file.addWithoutWipe(true);
  FDstat_t file_stat = settings_file.read();                                    // читаем структуру из файла

  switch (file_stat) {
    case FD_FS_ERR: bot.sendMessage(F("FileSystemError!"), error_chat);
      break;
    case FD_FILE_ERR: bot.sendMessage(F("OpenFileError!"), error_chat);
      break;
    default:
      break;
  }

  bot.clearServiceMessages(true);                                             //автоматическое удаление всех "сервисных" сообщений по типу "... закрепил сообщение"
  ArduinoOTA.setHostname(OTA_NAME);                                           //имя для точки OTA обновления
  ArduinoOTA.setPassword(OTA_PASS);                                           //пароль
  ArduinoOTA.begin();

  for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) people_in_subgr[((!students[i].subgroup) ? 0 : 1)]++;       //считаем количество людей в каждой подгруппе самым изощренным способом

  menu.start_page(0, file_stat);       //чисто для обновления структуры FB_Time
  list.begin();
  menu.start_page(1, file_stat);       //вот тут уже отсылаем менюшку
  checkYear();              //проверяем год на високосность
  editServiceMess("");            //стираем все приколюхи в статусном сообщении после всех begin`ов
}

void loop() {
  static int old_year = 0;
  static byte old_day = 0;
  bot.tick();
  settings_file.tick();
  timer.tick();
  ArduinoOTA.handle();
  FB_Time t = bot.getTime(3);

  if (!old_year && t.year)  old_year = t.year;        //Запоминаем год при запуске только после того, как время синхронизировано. Возможно в будущем заменим записью в EEPROM 
  else if (old_year != t.year)  {                     //Если год сменился - опа, произошел новый год, то проверяем на високосность
    checkYear();
    old_year = t.year;
  }
  if (!old_day && t.day)  old_day = t.day;
  else if (old_day != t.day) {                        //если сменился день - повод проверить актуальность недели
    checkTableWeek();
    old_day = t.day;
  }

}