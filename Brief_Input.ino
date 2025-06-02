void briefInput(Text message, String chat) {
  byte input_found = 0;           // 0 - нет ввода, 1 - есть, без условия, 2 - есть, с условием
  byte less_number = 0, found_month = 0, found_day = 0, faza = 0, syntax_errors = 0;
  String supp = "";
  int32_t m_id = 0;         //Храним id сообщения, которое будет информировать пользователя о состоянии введенного им сокращенного ввода (принят/не принят, правильно введен/неправильно)

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
    for (int i = 0; i < condition.length(); ) {
      byte c = condition[i], charLen = 1;

      if ((c & 0x80) == 0x00) charLen = 1; // ASCII
      else if ((c & 0xE0) == 0xC0) charLen = 2; // 2-byte UTF-8
      else if ((c & 0xF0) == 0xE0) charLen = 3; // 3-byte UTF-8
      String symbol = condition.substring(i, i + charLen);

      //-------------------Код разбора условия----------------------

      i += charLen; // увеличиваем i на длину символа
    }

    if (faza != 2 && faza != 4) {
      bot.editMessage(m_id, "Условие введено некорректно!");
      return;    //некорректный ввод условия, выходим
    }
  }

  else {                                 //Присваиваем данные текущего дня и пары, которая идет именно сейчас

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