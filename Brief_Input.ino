void briefInput(Text message, String chat) {
  byte input_found = 0;           // 0 - нет ввода, 1 - есть, без условия, 2 - есть, с условием
  byte found_less = 0, found_month = 0, found_day = 0, faza = 0, syntax_errors = 0;
  const String ignored_symbols = ",. ";    //символы, которые пользователь в теории может запихать между значащими частями в сокращенном вводе
  int32_t m_id = 0;                        //Храним id сообщения, которое будет информировать пользователя о состоянии введенного им сокращенного ввода (принят/не принят, правильно введен/неправильно)
  String supp = "";
  FB_Time real_time = bot.getTime(3);

  for (int i = 0; i <= message.count("\n"); i++) {                  //цикл, каждый раз берем часть сообщения до перевода строки
    Text dataa = message.getSub(i, "\n");                       //тут как раз и берем

    for (int j = 0; j < sizeof(students)/sizeof(students[0]); j++) {                //выискиваем среди всех фамилий нашу
      syntax_errors = 0;
      if (CheckSurnameMatch(dataa.toString(), students[j].surname, &syntax_errors)) {
        if (i == 0) input_found = 1;                      //если первая строка - фамилия = это сокращенный ввод без условия
        else  input_found = 2;                            //иначе - это сокращенный ввод с условием
        break;
      }
    }

    if (input_found)  break;
  }

  if (input_found == 2 && !isDigit((message.getSub(0, "\n").toString())[0]))  input_found = 1;        //если первая строка не фамилия, но и не условие - значит сильно опечатанная фамилия. Воспринимаем как сокр ввод без условия

  if (!input_found) return;                               //если не нашли никакого ввода - выходим сразу, тут больше нечего ловить

  bot.sendMessage("Сокращенный ввод " + String((input_found == 1) ? "без условия" : "с условием") + " принят!\nОбрабатываю список...", chat);
  timer.add(bot.lastBotMsg(), 15, chat);
  timer.add(bot.lastUsrMsg(), 15, chat);
  m_id = bot.lastBotMsg();

  if (input_found == 2) {                                      //рассматриваем условие при сокращенном вводе
    String condition = message.getSub(0, "\n").toString();
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
        //----------------------------------добавить проверку адекватности введенной пары----------------------------------
        if (isDigit(symbol[0])) found_less = found_less*10 + (symbol[0] - '0');         //собираем номер пары, смеха ради поддерживаем даже двузначные и более номера
        else if (found_less) faza++;
      }

      if (faza == 1) {    //ищем слово "пара"
        if (ignored_symbols.indexOf(symbol) == -1)  supp += symbol;       //нашли какой то значащий текст? Собираем в Строку, если получиться 'пара' - то пользователь пока не накосячил
        if (supp == "пара") faza++;
      }

      if (faza == 2) {    //ищем день
        if (unique_end) break;
        if (symbol[0] == '.') faza++;       //нашли разделитель дня и месяца (точку) - переходим к извлечению месяца
        else if (isDigit(symbol[0])) {
          found_day = found_day*10 + (symbol[0] - '0');
          if (found_day > day_month[found_month])  {
            bot.editMessage(m_id, "Значение дня в сокращенном вводе некорректно: \"" + String(found_day) + "\"!", chat);
            return;
          }
        }
      }

      if (faza == 3) {    //ищем месяц
        if (isDigit(symbol[0])) {
          found_month = found_month*10 + (symbol[0] - '0');
          if (found_month > 12)  {
            bot.editMessage(m_id, "Значение месяца в сокращенном вводе некорректно: \"" + String(found_month) + "\"!", chat);
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
      bot.editMessage(m_id, "Неправильный ввод условия при сокращенном вводе! Образец: \"1 пара 02.03\"\nУсловие некорректно из-за некорректной записи слова \"пара\"!", chat);
      return;
    }

    //------------------------------ Перебираем, на какой фазе остановился цикл ------------------------------



    bot.sendMessage("пара: " + String(found_less) + "\nДата: " + String(found_day) + "." + String(found_month), error_chat);
    timer.add(bot.lastBotMsg(), 15, chat);
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
      bot.editMessage(m_id, "Не удалось получить информацию о паре, которая идет прямо сейчас в сокращенном вводе без условия! Проверьте MINUTES_OFFSET в настройках программы, заданы ли временные рамки для данной пары в структуре или укажите условие вручную!");
      return;
    }
  }

  FirebaseJson nki_array[2];                                      //будем хранить будущие обьекты для запроса для обоих подгрупп

  for (int i = input_found-1; i < message.count("\n"); i++) {                   //обрабатываем фамилии
    Text dataa = message.getSub(i, "\n");
    bool surname_found = false;
    byte min_syntax_errors = 250;
    String assumed_surname = "";

    for (int ind = 0; ind < sizeof(students)/sizeof(students[0]); ind++) {
      syntax_errors = 0;
      byte func_res = CheckSurnameMatch(dataa.toString(), students[ind].surname, &syntax_errors);

      if (func_res == 1) {       //если фамилия безошибочно найдена в списке фамилий
        //------------------Здесь вызываем функцию постановки Нки-----------------------------
        surname_found = true;
        break;
      }

      if (func_res == 2 && syntax_errors <= min_syntax_errors) {
        if (syntax_errors == min_syntax_errors) {
          bot.sendMessage("Невозможно однозначно определить, какая это фамилия: " + dataa.toString(), error_chat);
          timer.add(bot.lastBotMsg(), 10, error_chat);
          break;
        }
        min_syntax_errors = syntax_errors;
        assumed_surname = students[ind].surname;
      }

      if (min_syntax_errors < 250 && ind == sizeof(students)/sizeof(students[0])-1)  {
        bot.sendMessage("Фамилия \"" + dataa.toString() + "\" воспринята как \"" + assumed_surname + "\"", error_chat);
        timer.add(bot.lastBotMsg(), 10, error_chat);
        //------------------Здесь вызываем функцию постановки Нки-----------------------------                (Фамилия найдена с ошибками и воспринята как одна из списка)
        surname_found = true;
      }
    }
    if (!surname_found) {
      bot.sendMessage("Неизвестная фамилия: " + String(dataa) + "!", chat);
      timer.add(bot.lastBotMsg(), 10, chat);
    }
  }

  bot.editMessage(m_id, F("Сокращенный ввод обработан!"));
}