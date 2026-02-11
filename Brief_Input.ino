void briefInput(Text message, String chat) {
  byte input_found = 0;           // 0 - нет ввода, 1 - есть, без условия, 2 - есть, с условием
  byte found_less[MAX_LESSONS] = {}, faza = 0, syntax_errors = 0, tries = 0, lessons_found = 0;
  const String ignored_symbols = ",. ";    // символы, которые пользователь в теории может запихать между значащими частями в сокращенном вводе
  String supp = "", post_symbol = "", temp_dataa = "";
  byte presence_mode = 0;                  // режим выставления пропусков наоборот. Указанные фамилии будут восприниматься как присутствующие, а не по стандарту, как отсутствующие
  MemoryControl MemControl;
  Date found_date(0, 0, realTime.year);

  post_symbol.reserve(10);

  for (uint8_t i = 0; i <= message.count("\n"); i++) {                  // цикл, каждый раз берем часть сообщения до перевода строки
    SpaceStringParse(message.getSub(i, "\n"), temp_dataa, post_symbol);         // см. описание ниже
    Text dataa(temp_dataa);

    for (uint8_t j = 0; j < sizeof(students)/sizeof(students[0]); j++) {                // выискиваем среди всех фамилий нашу
      syntax_errors = 0;
      if (CheckSurnameMatch(dataa.toString(), students[j].surname, &syntax_errors)) {       // нашли в строке фамилию из списка
        if (!i) input_found = 1;     // фамилия найдена сразу же в первой строке ввода
        else {
          if (CheckSurnameMatch(message.getSub(i-1, "\n"), PRESENCE_STRING, &syntax_errors, (String(PRESENCE_STRING).length() > 4 ? 0 : SURNAME_ERRORS_NUM))) {       // ищем на предыдущей строке указатель для presence_mode ввода
            presence_mode = 1;
          }
          if (i > presence_mode && isDigit((message.getSub(i-1-presence_mode, "\n").toString())[0])) input_found = 2;         // есть предпосылки полагать, что есть условие для ввода
          else input_found = 1;
        }
        break;
      }
    }
    if (input_found)  break;
  }

  //bot.sendMessage(String(presence_mode) + "/" + String(input_found), error_chat);         // отладочка

  //if (input_found == 2 && !isDigit((message.getSub(presence_mode, "\n").toString())[0]))  input_found = 1;        //если первая строка не фамилия, но и не условие - значит сильно опечатанная фамилия. Воспринимаем как сокр ввод без условия

  if (!input_found) return;                               //если не нашли никакого ввода - выходим сразу, тут больше нечего ловить

  serviceMess.edit("Сокращенный ввод " + String((input_found == 1) ? "без условия" : "с условием") + " принят!\nОбрабатываю список...");
  timer.add(bot.lastUsrMsg(), 15, chat);

  if (input_found == 2) {                                      //рассматриваем условие при сокращенном вводе
    String condition = message.getSub(0, "\n").toString(), symbol = "";
    condition.trim();                                          //убираем лишние пробелы
    bool unique_end = false;
    if (condition.endsWith("вчера") || condition.endsWith("позавчера") || condition.endsWith("сегодня")) unique_end = true;
    for (int i = 0; i < condition.length();) {                        //хитрая инкрементация цикла для посимвольной обработки возможного русского текста (символов различного байтового объема)
      byte c = condition[i], charLen = 1;

      if ((c & 0x80) == 0x00) charLen = 1; // ASCII
      else if ((c & 0xE0) == 0xC0) charLen = 2; // 2-byte UTF-8
      else if ((c & 0xF0) == 0xE0) charLen = 3; // 3-byte UTF-8 (на всяяякииийй)
      symbol = condition.substring(i, i + charLen);
      i += charLen; // увеличиваем i на длину символа

      if (faza == 0) {    //ищем номер пары
        if (isDigit(symbol[0])) found_less[lessons_found] = found_less[lessons_found]*10 + (symbol[0] - '0');         //собираем номер пары, на всякий случай поддерживаем даже двузначные и более номера
        else if (symbol.startsWith(","))  lessons_found++;
        else if (ignored_symbols.indexOf(symbol) == -1) {faza++; lessons_found++;}       //специально проваливаемся сразу, чтобы не упустить ни буквы следующего ввода
      }

      if (faza == 1) {    //ищем слово "пара"
        if (ignored_symbols.indexOf(symbol) == -1)  supp += symbol;       //нашли какой то значащий текст? Собираем в Строку, если получиться 'пара' - то пользователь пока не накосячил
        if (supp == "пара") faza++;
      }

      if (faza == 2) {    //ищем день
        if (unique_end) break;
        if (symbol[0] == '.') faza++;       //нашли разделитель дня и месяца .(точку) - переходим к извлечению месяца
        
        else if (isDigit(symbol[0])) {
          found_date.day = found_date.day*10 + (symbol[0] - '0');
          if (found_date.day > 31)  {
            serviceMess.edit("Значение дня в сокращенном вводе некорректно: \"" + String(found_date.day) + "\"!");
            return;
          }
        }
      }

      if (faza == 3) {    //ищем месяц
        if (isDigit(symbol[0])) {
          found_date.month = found_date.month*10 + (symbol[0] - '0');
          if (found_date.month > 12)  {
            serviceMess.edit("Значение месяца в сокращенном вводе некорректно: \"" + String(found_date.month) + "\"!");
            return;
          }
        }

        if (i == condition.length() && found_date.month) faza = 4;
      }
    }


    //------------------------------ Перебираем, на какой фазе остановился цикл ------------------------------
    if (faza == 2 && found_date.day) faza = 3;                 //фиксит случай "1 пара 20" (без точки на конце) - здесь надо сделать фазу = 3, т.к. не хватает только месяца

    if (faza == 2) {                                                        //указан только номер пары - значит Нка ставится сегодня
      if (unique_end) {                                                         // если имеет на конце одно из этих слов - значит дата в них указана в неявном виде
        Date todayWithOffset(realTime.day, realTime.month, realTime.year);
        if (condition.endsWith("позавчера"))  sumDate(&todayWithOffset, -2);              // Важно! Сначала проверяем это
        else if (condition.endsWith("вчера")) sumDate(&todayWithOffset, -1);              // только потом это, не наоборот!
        else if (!condition.endsWith("сегодня"))  {
          bot.sendMessage("Неизвестное окончание в условии сокращенного ввода!", chat);
          return;
        }
        found_date = todayWithOffset;
      }

      else {                                                                    // не имеет на конце специальных слов
        found_date.day = realTime.day;
        found_date.month = realTime.month;
      }
    }

    else if (faza == 3) {    //если указан только день - месяц воспринимаем как текущий
      found_date.month = realTime.month;
    }

    else if (faza == 1) {
      serviceMess.edit("Неправильный ввод условия при сокращенном вводе! Образец: \"1 пара 02.03\"\nУсловие некорректно из-за некорректной записи слова \"пара\"!", 7000);
      return;
    }
    //------------------------------ Перебираем, на какой фазе остановился цикл ------------------------------
  }

  else {                                 //Присваиваем данные текущего дня и пары, которая идет именно сейчас, если пользователь не указал эти данные явно (ввод без условия)
    found_date.day = realTime.day;
    found_date.month = realTime.month;
    Time now_time(realTime.hour, realTime.minute);
    
    for (byte i = 0; i < (sizeof(lessons)/sizeof(lessons[0])); i++) {
      Time support_time(0, MINUTES_OFFSET);
      if (now_time >= (lessons[i].start - support_time) && now_time <= (lessons[i].end + support_time)) {
        found_less[lessons_found] = i+1;
        break;
      }
    }
    if (!found_less[lessons_found++]) {                                     // инкрементируем здесь, чтобы далее переменная имела корректное значение
      serviceMess.edit("Убедитесь в корректности текущей пары!", 5000);
      return;
    }
  }

  serviceMess.edit("Сокращенный ввод " + String((input_found == 1) ? "без условия" : "с условием") + " принят!\nПолучаю данные из таблицы...");

  // будем хранить будущие обьекты для запроса
  // [массив Нок для каждой пары, которые уже были выставлены в Таблице]
  FirebaseJson nki_array[lessons_found];

  uint8_t table_indexes[lessons_found] = {};                        // индексы в таблице для каждой выставляемой пары (относительные) для сопоставление теоретического номера пары с фактическими номерами столбцов
  bool targetSubgroup[2] = {true, true};                            // "Для какой подгруппы введенные пары могут быть выставлены (корректны)?" [0] - корректны ли для 1 подгруппы, [1] - корректны ли для второй

  nka.surn = "";                                                    // позволяет в алгоритме функции получить не у конкретной, а у первой в списке фамилии posI
  nka.date.day = found_day;
  nka.date.month = found_month;

  bool week_index = (week[0]->parity == nka.parity);        // индекс недели, получается из соответсивия/несоответствия четности текущей недели и той, на которой будут выставляться пропуски

  // ------------------------------ Валидация введенных пользователем пар ------------------------------
  for (uint8_t less = 0; less < lessons_found; less++) {            // перебираем все введенные пары, чтобы 
    bool is_found = false;
    for (uint8_t todays_less = 0; todays_less < week[week_index]->days[nka.dayWeek-1].subj_num; todays_less++) {          // цикл по всем парам нужного дня
      if (week[week_index]->days[nka.dayweek-1].less_info[todays_less].number == found_less[less]) {
        is_found = true;
        uint8_t sub_have = week[week_index]->days[nka.dayweek-1].less_info[todays_less].in_subgroup;
        if (!sub_have)  targetSubgroup[1] = false;        // пары нет у второй подгруппы точно
        if (sub_have == 1)  targetSubgroup[0] = false;      // пары нет у первой подгруппы точно
      }
      else table_indexes[less]++;
    }
    if (!is_found)  {                                                     // если какой-либо введенной пары нет ни у первой, ни у второй подгруппы в расписании
      serviceMess.edit("Пара #" + String(found_less[less]) + " не существует в введенный день ни у одной подгруппы!", 10000);
      return;
    }

    if (!targetSubgroup[0] && !targetSubgroup[1]) {                       // сработает, если введено > 1 пары, и список введенных пар не присутствует в полной мере ни в расписании первой, ни в расписании второй подгруппы
      serviceMess.edit(F("Введенные пары нельзя в полной мере выставить ни первой, ни второй подгруппе!"), 10000);
      return;
    }
  }
  // ------------------------------ Валидация введенных пользователем пар ------------------------------
  


  // ------------------------------ Получение существующих пропусков ------------------------------
  for (byte less = 0; less < lessons_found; less++) {                             // получаем раздельно, нету особо смысла объединять диапазон в единый, т.к. пары могут быть не в смежных столбцах
    String range = SheetName;                                                                                                           // может быть позже сделаем оптимизацию случая их смежности
    range += charOffset(String(nka.posC), table_indexes[less]);                   // собираем полный вид диапазона для чтения/записи
    range += nka.posI;
    range += ":";
    range += charOffset(String(nka.posC), table_indexes[less]);
    range += nka.posI + sizeof(students)/sizeof(students[0]) - 1;

    tries = 0;
    while (!GSheet.values.get(&nki_array[less], spreadsheetId, range) && tries < GetTryNum) tries++;
    if (tries == SetTryNum) {
      bot.sendMessage(F("ErrorGetRequest! (Получение существующих пропусков из Sheet)\nОбработка сокращенного ввода досрочно прервана!"), chat);

    }
  
    for (byte j = 0; j < sizeof(students)/sizeof(students[0]); j++) {
      String address = "values/[";
      address += j;
      address += "]/[0]";
      if (getJsonData(nki_array[less], address, false) == "invalidPath") {
        if (!presence_mode) nki_array[less].set(address, "");
        else nki_array[less].set(address, "D");
      }
    }

    if (!MemControl.check()) {
      serviceMess.edit(F("Нехватка RAM! (Этап формирование массивов)"), 7000);
      for (byte lesss = 0; lesss < lessons_found; lesss++)  nki_array[lesss].clear();            // очищаем массивы вручную
      return;
    }
  }
  // ------------------------------ Получение существующих пропусков ------------------------------



  // ------------------------------ Обработка введенных фамилий ------------------------------
  serviceMess.edit("Обрабатываю введенные фамилии...");

  for (int i = input_found-1 + presence_mode; i < message.count("\n"); i++) { 
    SpaceStringParse(message.getSub(i, "\n"), temp_dataa, post_symbol);         // см. описание ниже
    Text dataa(temp_dataa);
    byte surname_length[2] = {};                                                //количество фамилий этой подгруппы перед найденной. Нужно для вставки фамилии в документе на правильное место

    bool surname_found = false;
    byte min_syntax_errors = 250;
    person assumed_people;                                                      //если фамилия с опечаткой - здесь будем хранить человека, наиболее подходящего
    byte assumed_length = 0; 
    String address = "values/[";

    for (int ind = 0; ind < sizeof(students)/sizeof(students[0]); ind++) {      //цикл перебирает все фамилии по списку и сравнивает с введенной
      syntax_errors = 0;
      byte func_res = CheckSurnameMatch(dataa.toString(), students[ind].surname, &syntax_errors);    // 1 - безошибочно, 2 - с допустимым кол-вом ошибок

      if (func_res == 1) {       //если фамилия безошибочно найдена в списке фамилий
        //------------------Здесь ставим Нку нужному человеку-----------------------------
        
        address += surname_length[students[ind].subgroup];
        address += "]/[0]";

        for (byte less = 0; less < lessons_found; less++) {
          nki_array[students[ind].subgroup][less].set(address, UpdateArrayCell(presence_mode, post_symbol, getJsonData(nki_array[students[ind].subgroup][less], address, true)));
          need_post[students[ind].subgroup] = true;
        }
      
        surname_found = true;
        break;
      }

      if (func_res == 2 && syntax_errors <= min_syntax_errors) {
        if (syntax_errors == min_syntax_errors) {
          bot.sendMessage(String(ALERT_SYMBOL) + " Невозможно однозначно определить, какая это фамилия: " + dataa.toString() + String(ALERT_SYMBOL), chat);
          timer.add(bot.lastBotMsg(), 20, chat);
          break;
        }

        min_syntax_errors = syntax_errors;
        assumed_people.surname = students[ind].surname;
        assumed_people.subgroup = students[ind].subgroup;
        assumed_length = surname_length[students[ind].subgroup];
      }

      if (min_syntax_errors < 250 && ind == sizeof(students)/sizeof(students[0])-1)  {
        if (NOTIFY_ERRORS_FIND) {
          bot.sendMessage("Фамилия \"" + dataa.toString() + "\" воспринята как \"" + assumed_people.surname + "\"", chat);
          timer.add(bot.lastBotMsg(), 10, chat);
        }
        //------------------Здесь ставим Нку нужному человеку-----------------------------                (Фамилия найдена с ошибками и воспринята как одна из списка)
        address += assumed_length;
        address += "]/[0]";

        for (byte less = 0; less < lessons_found; less++) {
          nki_array[assumed_people.subgroup][less].set(address, UpdateArrayCell(presence_mode, post_symbol, getJsonData(nki_array[assumed_people.subgroup][less], address, true)));
          need_post[assumed_people.subgroup] = true;
        }

        surname_found = true;
      }


      surname_length[students[ind].subgroup]++;         //см. описание к переменной выше
    }

    if (!surname_found) {
      bot.sendMessage("Неизвестная фамилия: " + String(dataa) + "!", chat);
      timer.add(bot.lastBotMsg(), 10, chat);
    }

    if (!MemControl.check()) {
        serviceMess.edit(F("Нехватка RAM! (Заполнение массивов пропусками)"), 7000);
        for (byte sub = 0; sub < 2; sub++)  for (byte lesss = 0; lesss < lessons_found; lesss++)  nki_array[sub][lesss].clear();            // очищаем массивы вручную
        return;
    }
  }
  // ------------------------------ Обработка введенных фамилий ------------------------------



  // ------------------------------ Выставление новых пропусков ------------------------------
  serviceMess.edit("Выставляю пропуски...");

  for (byte i = 0; i < 2; i++) {
    if (!need_post[i] && !presence_mode)  {
      for (byte less = 0; less < lessons_found; less++) nki_array[i][less].clear();
      continue;
    }

    String answ = "";
    tries = 0;

    for (byte less = 0; less < lessons_found; less++) {
      String range = ((!i) ? Sheet1 : Sheet2);
      range += charOffset(String(nka.posC), table_indexes[i][less]);                   // собираем полный вид диапазона для чтения/записи
      range += nka.posI;
      range += ":";
      range += charOffset(String(nka.posC), table_indexes[i][less]);
      range += nka.posI + people_in_subgr[i] - 1;

      while (!GSheet.values.update(&answ, spreadsheetId, range, &nki_array[i][less]) && tries < SetTryNum) {
        tries++;
      }
      nki_array[i][less].clear();
      if (tries == SetTryNum) bot.sendMessage(F("ErrorSendRequest! (Отправка пропусков в Sheet)"), chat);
    }
  }
  // ------------------------------ Выставление новых пропусков ------------------------------


  serviceMess.edit("Сокращенный ввод обработан!\nRAM занято: " + String(MemControl.getDiff()/1024) + " кБ.", 5000);
}

String UpdateArrayCell(byte presence_m, String post_symbol, String old_nka) {
  if (presence_m)  return " ";

  else {
    if (post_symbol == "" && old_nka != "R") return DISREP_SYMBOL;          // если доп указаний нет и в таблице уже не был высталвен пропуск по УП
    if (post_symbol == "уп" || post_symbol == "Уп" || post_symbol == "УП")  return RESPECT_SYMBOL;
    if (post_symbol == "неуп" || post_symbol == "неУп" || post_symbol == "неУП")  return DISREP_SYMBOL;
    if (post_symbol == "тут" || post_symbol == "Тут" || post_symbol == "ТУТ") return PRESENCE_SYMBOL;
    else {
      bot.sendMessage("Некорректный post_symbol обнаружен в сокращенном вводе!\nСделаем вид, что его вообще не было: \"" + String(post_symbol) + "\"", error_chat);
      return DISREP_SYMBOL;
    }
  }
  return "";
}

String getJsonData (FirebaseJson &object, String &addr, bool show_error) {
  FirebaseJsonData data;
  if (object.get(data, addr)) return data.stringValue;
  if (show_error) bot.sendMessage(F("invalidPath! Тщательно проверьте результат работы последнего действия!"), error_chat);
  return "invalidPath";
}

void SpaceStringParse(const Text& mess, String& dataa, String& post_symbol) {       // может распарсить строку формата "Ололоев уп" на значащие составные части. Поддерживает простые строки без post_symbol
  byte space_count = mess.count(" ");

  if (space_count-1) {
    dataa = mess.getSub(0, " ");
    post_symbol = mess.getSub(space_count-1, " ");
  }

  else {
    dataa = mess;
    post_symbol = "";
  }
}
