void briefInput(Text message, String chat) {
  byte input_found = 0;           // 0 - нет ввода, 1 - есть, без условия, 2 - есть, с условием
  byte found_less = 0, found_month = 0, found_day = 0, faza = 0, syntax_errors = 0, tries = 0;
  const String ignored_symbols = ",. ";    // символы, которые пользователь в теории может запихать между значащими частями в сокращенном вводе
  String supp = "", post_symbol = "", temp_dataa = "";
  byte presence_mode = 0;                  // режим выставления пропусков наоборот. Указанные фамилии будут восприниматься как присутствующие, а не наоборот
  FB_Time real_time = bot.getTime(3);

  post_symbol.reserve(10);

  for (int i = 0; i <= message.count("\n"); i++) {                  // цикл, каждый раз берем часть сообщения до перевода строки
    SpaceStringParse(message.getSub(i, "\n"), temp_dataa, post_symbol);         // см. описание ниже
    Text dataa(temp_dataa);

    for (int j = 0; j < sizeof(students)/sizeof(students[0]); j++) {                // выискиваем среди всех фамилий нашу
      syntax_errors = 0;
      if (CheckSurnameMatch(dataa.toString(), students[j].surname, &syntax_errors)) {
        if (!i) input_found = 1;            // без условия
        else {
           if (CheckSurnameMatch(message.getSub(i-1, "\n"), PRESENCE_STRING, &syntax_errors, (String(PRESENCE_STRING).length() > 4 ? 0 : SURNAME_ERRORS_NUM))) {
             presence_mode = 1;              // есть ключевое слово - воспринимаем введенные фамилии как присутствующих
             if (i == 1) input_found = 1;
           }
           if (!input_found)  input_found = 2;
        }
        break;
      }
    }
    if (input_found)  break;
  }

  //if (input_found == 2 && !isDigit((message.getSub(presence_mode, "\n").toString())[0]))  input_found = 1;        //если первая строка не фамилия, но и не условие - значит сильно опечатанная фамилия. Воспринимаем как сокр ввод без условия

  if (!input_found) return;                               //если не нашли никакого ввода - выходим сразу, тут больше нечего ловить

  bot.sendMessage("Presence: " + String(presence_mode) + "\nInput: " + String(input_found), error_chat);

  return;

  serviceMess.edit("Сокращенный ввод " + String((input_found == 1) ? "без условия" : "с условием") + " принят!\nОбрабатываю список...");
  timer.add(bot.lastUsrMsg(), 15, chat);

  if (input_found == 2) {                                      //рассматриваем условие при сокращенном вводе
    String condition = message.getSub(presence_mode, "\n").toString();
    condition.trim();                                          //убираем лишние пробелы
    bool unique_end = false;
    if (condition.endsWith("вчера") || condition.endsWith("позавчера") || condition.endsWith("сегодня")) unique_end = true;
    for (int i = 0; i < condition.length(); /*этот пункт прописан отдельно дальше*/) {                        //хитрая инкрементация цикла для посимвольной обработки возможного русского текста
      byte c = condition[i], charLen = 1;

      if ((c & 0x80) == 0x00) charLen = 1; // ASCII
      else if ((c & 0xE0) == 0xC0) charLen = 2; // 2-byte UTF-8
      else if ((c & 0xF0) == 0xE0) charLen = 3; // 3-byte UTF-8 (на всяяякииийй)
      String symbol = condition.substring(i, i + charLen);
      i += charLen; // увеличиваем i на длину символа

      if (faza == 0) {    //ищем номер пары
        if (isDigit(symbol[0])) found_less = found_less*10 + (symbol[0] - '0');         //собираем номер пары, смеха ради поддерживаем даже двузначные и более номера
        //-------------------------Здесь добавить условие проверки нескольких пар для ввода---------------------------------------------
        else if (found_less) faza++;
      }

      if (faza == 1) {    //ищем слово "пара"
        if (ignored_symbols.indexOf(symbol) == -1)  supp += symbol;       //нашли какой то значащий текст? Собираем в Строку, если получиться 'пара' - то пользователь пока не накосячил
        if (supp == "пара") faza++;
      }

      if (faza == 2) {    //ищем день
        if (unique_end) break;
        if (symbol[0] == '.') faza++;       //нашли разделитель дня и месяца .(точку) - переходим к извлечению месяца
        
        else if (isDigit(symbol[0])) {
          found_day = found_day*10 + (symbol[0] - '0');
          if (found_day > 31)  {
            serviceMess.edit("Значение дня в сокращенном вводе некорректно: \"" + String(found_day) + "\"!");
            return;
          }
        }
      }

      if (faza == 3) {    //ищем месяц
        if (isDigit(symbol[0])) {
          found_month = found_month*10 + (symbol[0] - '0');
          if (found_month > 12)  {
            serviceMess.edit("Значение месяца в сокращенном вводе некорректно: \"" + String(found_month) + "\"!");
            return;
          }
        }

        if (i == condition.length() && found_month) faza = 4;
      }
    }



    //------------------------------ Перебираем, на какой фазе остановился цикл ------------------------------
    if (faza == 2 && found_day) faza = 3;                 //фиксит случай "1 пара 20" (без точки на конце) - здесь надо сделать фазу = 3, т.к. не хватает только месяца

    if (faza == 2) {                                                        //указан только номер пары - значит Нка ставится сегодня
      if (unique_end) {                                                             //если имеет на конце одно из этих слов - значит дата в них завуалирована
        if (condition.endsWith("позавчера"))  found_day = real_time.day-2;              //Важно! Сначала проверяем это
        else if (condition.endsWith("вчера")) found_day = real_time.day-1;              //только потом это, не наоборот!
        else if (condition.endsWith("сегодня")) found_day = real_time.day;
        found_month = real_time.month;
      }

      else {                                                                        //не имеет на конце специальных слов
        found_day = real_time.day;
        found_month = real_time.month;
      }
    }

    else if (faza == 3) {    //если указан только день - месяц воспринимаем как текущий
      found_month = real_time.month;
    }

    else if (faza == 1) {
      serviceMess.edit("Неправильный ввод условия при сокращенном вводе! Образец: \"1 пара 02.03\"\nУсловие некорректно из-за некорректной записи слова \"пара\"!");
      return;
    }

    //------------------------------ Перебираем, на какой фазе остановился цикл ------------------------------

  }

  else {                                 //Присваиваем данные текущего дня и пары, которая идет именно сейчас, если пользователь не указал эти данные явно (ввод без условия)
    found_day = real_time.day;
    found_month = real_time.month;
    Time now_time(real_time.hour, real_time.minute);
    
    for (byte i = 0; i < (sizeof(lessons)/sizeof(lessons[0])); i++) {
      Time support_time(0, MINUTES_OFFSET);
      if (now_time >= (lessons[i].start - support_time) && now_time <= (lessons[i].end + support_time)) {
        found_less = i+1;
        break;
      }
    }
    if (!found_less) {
      serviceMess.edit("Убедитесь в корректности текущей пары!", 5000);
      return;
    }
  }

  uint32_t Heap = ESP.getFreeHeap();
  FirebaseJson nki_array[2];                                      //будем хранить будущие обьекты для запроса для обеих подгрупп

  bool need_post[2] = {false, false};                             //есть ли пропуски у людей этой продгруппы. Если нет - то и смысла отправлять запрос в будущем нету
  bool valid_lesson[2] = {false, false};                          //есть ли вообще в этот день у данной подгруппы эта пара? (да, мне показалось здесь самое время это проверить :) )
  byte lesson_length[2] = {};                                     //отображает, какая пара для выставления по счету в это день. (Счет всегда с 1, вот номер пары может быть 1)
  nka.surn = "";
  nka.date.day = found_day;
  nka.date.month = found_month;
  String range[2] = {Sheet1, Sheet2};

  for (byte i = 0; i < 2; i++) {                                  //заполняем оба обьекта "", по количеству людей в подгруппе. В дальнейшем будем заменять некоторые позиции на фамилии. Гарантирует 'неразрывность' JSON документа
    nka.subgroup = i;
    getNIndex();
    byte week_index = nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2);        //индекс недели, складывается из подгруппы и сдвига на неделю, соответствующую выставляемым Нкам по четности
    
    for (byte day_iter = 0; day_iter < week[week_index]->subj_num[nka.dayWeek-1]; day_iter++) {
      if (week[week_index]->less_nums[nka.dayWeek-1][day_iter] == found_less)  {
        valid_lesson[i] = true;
        break;
      }
      lesson_length[i]++;
    }

    range[i] += charOffset(String(nka.posC), lesson_length[i]);                   // собираем полный вид диапазона для чтения/записи
    range[i] += nka.posI;
    range[i] += ":";
    range[i] += charOffset(String(nka.posC), lesson_length[i]);
    range[i] += nka.posI + people_in_subgr[i] - 1;

    tries = 0;
    while (!GSheet.values.get(&nki_array[i], spreadsheetId, range[i]) && tries < GetTryNum) tries++;
    if (tries == SetTryNum) bot.sendMessage("ErrorGetRequest!", chat);

    for (byte j = 0; j < people_in_subgr[i]; j++) {
      String address = "values/[";
      address += j;
      address += "]/[0]";
      if (getJsonData(nki_array[i], address, false) == "invalidPath") {
        if (!presence_mode) nki_array[i].set(address, "");
        else nki_array[i].set(address, "D");
      }
    }
  }

  if (!valid_lesson[0] || !valid_lesson[1]) {
    bot.sendMessage("В данный день у " + String((!valid_lesson[0]) ? "1" : "") + String((!valid_lesson[0] && !valid_lesson[1]) ? " и " : "") + String((!valid_lesson[1]) ? "2" : "") + String((!valid_lesson[0] && !valid_lesson[1]) ? " подгрупп" : " подгруппы") + " нет пары под номером " + String(found_less) + "!\nВыставление пропусков соответствующим студентам невозможно!", chat);
    timer.add(bot.lastBotMsg(), 20, chat);
  }

  for (int i = input_found-1 + presence_mode; i < message.count("\n"); i++) {                   //обрабатываем фамилии
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
      byte func_res = CheckSurnameMatch(dataa.toString(), students[ind].surname, &syntax_errors);

      if (func_res == 1) {       //если фамилия безошибочно найдена в списке фамилий
        //------------------Здесь ставим Нку нужному человеку-----------------------------
        if (valid_lesson[students[ind].subgroup]) {
          address += surname_length[students[ind].subgroup];
          address += "]/[0]";

          if (presence_mode) {
            nki_array[students[ind].subgroup].set(address, " "); 
            need_post[students[ind].subgroup] = true;                               //есть фамилии в этой подгруппе для выставлния, значит будем вызывать функцию отправки запроса
          }

          else {
            if (post_symbol == "" && getJsonData(nki_array[students[ind].subgroup], address, true) != "R") {          // если доп указания отсутствуют
              nki_array[students[ind].subgroup].set(address, DISREP_SYMBOL); 
              need_post[students[ind].subgroup] = true;
            }

            else if (post_symbol == "уп" || post_symbol == "Уп" || post_symbol == "УП") {                             // если нужно отметить пропуск как УП
              nki_array[students[ind].subgroup].set(address, RESPECT_SYMBOL);
              need_post[students[ind].subgroup] = true;
            }

            else if (post_symbol == "неуп" || post_symbol == "неУП" || post_symbol == "неУп") {                       // если понадобилось отметить пропуск как неУП (например, когда ранее он был отмечен УП)
              nki_array[students[ind].subgroup].set(address, DISREP_SYMBOL);
              need_post[students[ind].subgroup] = true;
            }

            else if (post_symbol == "тут" || post_symbol == "Тут") {                                                  // когда нужно отметить присутствие человека
              nki_array[students[ind].subgroup].set(address, PRESENCE_SYMBOL);
              need_post[students[ind].subgroup] = true;
            }

            else bot.sendMessage("Неизвестное дополнительное указание к фамилии \"" + students[ind].surname + "\": \"" + post_symbol + "\"!", chat);
          }
        }
        surname_found = true;
        break;
      }

      if (func_res == 2 && syntax_errors <= min_syntax_errors) {
        if (syntax_errors == min_syntax_errors) {
          bot.sendMessage("Невозможно однозначно определить, какая это фамилия: " + dataa.toString(), chat);
          timer.add(bot.lastBotMsg(), 10, error_chat);
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
        if (valid_lesson[assumed_people.subgroup]) {
          address += surname_length[assumed_people.subgroup];
          address += "]/[0]";

          if (presence_mode) {
            nki_array[assumed_people.subgroup].set(address, " "); 
            need_post[assumed_people.subgroup] = true;                               //есть фамилии в этой подгруппе для выставлния, значит будем вызывать функцию отправки запроса
          }

          else {
            if (post_symbol == "" && getJsonData(nki_array[assumed_people.subgroup], address, true) != "R") {          // если доп указания отсутствуют
              nki_array[assumed_people.subgroup].set(address, DISREP_SYMBOL); 
              need_post[assumed_people.subgroup] = true;
            }

            else if (post_symbol == "уп" || post_symbol == "Уп" || post_symbol == "УП") {                             // если нужно отметить пропуск как УП
              nki_array[assumed_people.subgroup].set(address, RESPECT_SYMBOL);
              need_post[assumed_people.subgroup] = true;
            }

            else if (post_symbol == "неуп" || post_symbol == "неУП" || post_symbol == "неУп") {                       // если понадобилось отметить пропуск как неУП (например, когда ранее он был отмечен УП)
              nki_array[assumed_people.subgroup].set(address, DISREP_SYMBOL);
              need_post[assumed_people.subgroup] = true;
            }

            else if (post_symbol == "тут" || post_symbol == "Тут") {                                                  // когда нужно отметить присутствие человека
              nki_array[students[ind].subgroup].set(address, PRESENCE_SYMBOL);
              need_post[students[ind].subgroup] = true;
            }

            else bot.sendMessage("Неизвестное дополнительное указание к фамилии \"" + students[ind].surname + "\": \"" + post_symbol + "\"!", chat);
          }
        }
        surname_found = true;
      }

      surname_length[students[ind].subgroup]++;         //см. описание к переменной выше
    }
    if (!surname_found) {
      bot.sendMessage("Неизвестная фамилия: " + String(dataa) + "!", chat);
      timer.add(bot.lastBotMsg(), 10, chat);
    }
  }

  Heap -= ESP.getFreeHeap();

  for (byte i = 0; i < 2; i++) {
    if (!need_post[i] && !presence_mode || valid_lesson[i])  {
      nki_array[i].clear();
      continue;
    }

    String answ = "";
    tries = 0;

    while (!GSheet.values.update(&answ, spreadsheetId, range[i], &nki_array[i]) && tries < SetTryNum) {
      tries++;
    }

    nki_array[i].clear();
    if (tries == SetTryNum) bot.sendMessage("ErrorSendRequest!", chat);
  }

  serviceMess.edit("Сокращенный ввод обработан!\nRAM занятно: " + String(Heap/1024) + " кБ.", 5000);
}

String getJsonData (FirebaseJson &object, String &addr, bool show_error) {
  FirebaseJsonData data;
  if (object.get(data, addr)) return data.stringValue;
  if (show_error) bot.sendMessage("invalidPath", error_chat);
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
