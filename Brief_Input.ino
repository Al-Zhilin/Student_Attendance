void briefInput(Text message, String chat) {
  byte input_found = 0;           // 0 - нет ввода, 1 - есть, без условия, 2 - есть, с условием
  byte found_less = 0, found_month = 0, found_day = 0, faza = 0, syntax_errors = 0;
  const String ignored_symbols = ",. ";    //символы, которые пользователь в теории может запихать межде значащими частями в сокращенном вводе
  int32_t m_id = 0;         //Храним id сообщения, которое будет информировать пользователя о состоянии введенного им сокращенного ввода (принят/не принят, правильно введен/неправильно)
  String supp = "";
  FB_Time real_time = bot.getTime(3);

  Text dataa = message.getSub(0, "\n");
  for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) {             //ПЕРВАЯ строка сообщения - фамилия (сокращенный ввод без условия)
    syntax_errors = 0;
    if (CheckSurnameMatch(dataa.toString(), students[i].surname, &syntax_errors)) {
      input_found = 1;
      break;
    }
  }

  if (!input_found) {
    dataa = message.getSub(1, "\n");
    for (byte i = 0; i < sizeof(students)/sizeof(students[0]); i++) {           //ВТОРАЯ строка сообщения - фамилия (сокращенный с условием)
      syntax_errors = 0;
      if (CheckSurnameMatch(dataa.toString(), students[i].surname, &syntax_errors)) {
        input_found = 2;
        break;
      }
    }
  }

  if (input_found == 0)  return;         //если не нашли никакого ввода - выходим сразу, тут больше нечего ловить

  bot.sendMessage("Сокращенный ввод " + String((input_found == 1) ? "без условия" : "с условием") + " принят!\nОбрабатываю список...", chat);
  m_id = bot.lastBotMsg();

  if (input_found == 2) {                                      //рассматриваем условие при сокращенном вводе
    String condition = message.getSub(0, "\n").toString();
    condition.trim();                                          //убираем лишние пробелы
    bool unique_end = false;
    if (condition.endsWith("вчера") || condition.endsWith("позавчера")) unique_end = true;
    for (int i = 0; i < condition.length(); ) {
      byte c = condition[i], charLen = 1;

      if ((c & 0x80) == 0x00) charLen = 1; // ASCII
      else if ((c & 0xE0) == 0xC0) charLen = 2; // 2-byte UTF-8
      else if ((c & 0xF0) == 0xE0) charLen = 3; // 3-byte UTF-8 (на всяяякииийй)
      String symbol = condition.substring(i, i + charLen);

      if (faza == 0) {    //ищем номер пары
        if (isDigit(symbol[0])) found_less = found_less*10 + (symbol[0] - '0');         //собираем номер пары, смеха ради поддерживаем даже двузначные и более номера
        else faza++;
      }

      if (faza == 1) {    //ищем слово "пара"
        if (ignored_symbols.indexOf(symbol) == -1)  supp += symbol;       //нашли какой то значащий текст? Собираем в Строку, если получиться 'пара' - то пользователь пока не накосячил
        if (supp == "пара") faza++;
      }

      if (faza == 2) {    //ищем день
        if (unique_end) break;
        if (symbol[0] == '.') faza++;       //нашли разделитель дня и месяца (точку) - переходим к извлечению месяца
        else if (isDigit(symbol[0])) found_day = found_day*10 + (symbol[0] - '0');
      }

      if (faza == 3) {
        if (isDigit(symbol[0])) {
          found_month = found_month*10 + (symbol[0] - '0');
          faza = 4;
        }
      }

      i += charLen; // увеличиваем i на длину символа
    }

    if (found_month > 12 || found_month < 1)  bot.editMessage(m_id, "Значение месяца в сокращенном вводе некорректно!");
    if (found_day > day_month[found_month] || found_day < 1)  bot.editMessage(m_id, "Значение дня в сокращенном вводе некорректно!");
    //----------------------------------добавить проверку адекватности введенной пары----------------------------------

    if (faza == 2 && unique_end) {
      if (condition.endsWith("вчера"))  found_day = real_time.day-1;
      else if (condition.endsWith("позавчера")) found_day = real_time.day-2;
      found_month = real_time.month;
    } 

    else if (faza != 4) {
      bot.editMessage(m_id, "Неправильный ввод условия при сокращенном вводе! Образец: \"1 пара 24.01\"");
      return;
    }
  }

  else {                                 //Присваиваем данные текущего дня и пары, которая идет именно сейчас, если пользователь не указал эти данные явно
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
      bot.editMessage(m_id, "Не удалось получить информацию о паре, которая идет прямо сейчас в сокращенном вводе без условия! Проверьте MINUTES_OFFSET в настройках программы или укажите условие вручную!");
      return;
    }
  }

  for (int i = input_found-1; i < message.count("\n"); i++) {                   //обрабатываем сокращенный ввод
    Text dataa = message.getSub(i, "\n");
    bool surname_found = false;
    byte min_syntax_errors = 250;
    String assumed_surname = "";

    for (int ind = 0; ind < sizeof(students)/sizeof(students[0]); ind++) {
      syntax_errors = 0;
      byte func_res = CheckSurnameMatch(dataa.toString(), students[ind].surname, &syntax_errors);

      if (func_res == 1) {
        //------------------Здесь вызываем функцию постановки Нки-----------------------------
        surname_found = true;
        break;
      }

      if (func_res == 2 && syntax_errors <= min_syntax_errors) {
        if (syntax_errors == min_syntax_errors) {
          bot.sendMessage("Невозможно однозначно определить, какая это фамилия: " + dataa.toString());
          break;
        }
        min_syntax_errors = syntax_errors;
        assumed_surname = students[ind].surname;
      }

      if (min_syntax_errors < 250 && ind == sizeof(students)/sizeof(students[0])-1)  {
        bot.sendMessage("Фамилия \"" + dataa.toString() + "\" воспринята как \"" + assumed_surname + "\"");
        //------------------Здесь вызываем функцию постановки Нки-----------------------------
        surname_found = true;
      }
    }
    if (!surname_found) bot.sendMessage("Неизвестная фамилия: " + String(dataa) + "!", chat);
  }

  bot.editMessage(m_id, "Сокращенный ввод обработан!");
}