uint8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  FB_Time realTime = bot.getTime(3);                            //структура реального времени

  String DaysOfWeek[7] = {
    "Понедельник",
    "Вторник",
    "Среда",
    "Четверг",
    "Пятница",
    "Суббота",
    "Воскресенье",
  };

  //добавить в будущем проверку перехода через новый год и на разные даты последней недели в 2 листах, если нужно

  if (realTime.day == 0) {
    bot.sendMessage(F("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!"), Admins[0]);
    timer.add(bot.lastBotMsg(), 10);
  }

  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  byte pulled_day = week[0].pon_day + (realTime.dayWeek-1);         //далее сравниывем даты по дням недели. week[i].pon_day всегда дата понедельника, а прибавлением дня недели делаем дату, соответственно текущему дню недели. Упрощает дальнейшие расчеты
  byte pulled_month = week[0].pon_month;

  if (pulled_day > day_month[pulled_month-1]) {
    pulled_day -= day_month[pulled_month-1];
    pulled_month++;
  }

  byte weeksToBuild = 0;

  if (pulled_month == realTime.month) {       //если месяцы одинаковые
    if (realTime.day == pulled_day) {
      return 0;       //отлично, в таблице прописана актуальная неделя! Создание новой/-ых недели/недель не требуется!
    }

    else {
      weeksToBuild = (realTime.day - pulled_day) / 7;      //количество недель, которые нужно достроить
    }
  }

  else {
    int days_between = day_month[pulled_month - 1] - pulled_day;

    for (byte i = pulled_month; i < realTime.month - 1; i++) {
      days_between += day_month[i];
    }

    days_between += realTime.day;
    weeksToBuild = days_between / 7;
    if (days_between % 7 != 0)  bot.sendMessage(F("WARNING! days_between % 7 != 0!"), Admins[0]);
  }
  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  
  menu.editServiceMess("Нужно достроить недель: " + String(weeksToBuild));

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  byte tableLen[2] = {};        //длина таблицы для 2 четностей подгруппы, таблица в которой сейчас достраивается
  byte subj_num[7] = {};
  
  for (byte i = 0; i < 2; i++) {                          //цикл для листов 2 подгрупп

    //-------Получаем данные о парах кахдого дня недели противоположной настоящей четности для каждой подгруппы (нужно для tableLen и дальнейшего заполнения)
    String range = "";
    if (!i) range += Sheet1;
    else range += Sheet2;
    range += weekInfo_c;
    range += (weekInfo_i + (offset[i]*(week_off-2)));
    range += ":";
    range += charOffset(String(weekInfo_c), 1);
    range += (weekInfo_i + (offset[i]*(week_off-2)));
    Text answer(list.getCells(range));
    list.BriefCellToArray(subj_num, sizeof(subj_num)/sizeof(subj_num[0]), answer);          //функция заполняет массив subj_num нужными данными из ячейки
    

    for (byte k = 0; k < 2; k++) {              //ищем 2 длины - для каждой четности недели у подгруппы i
      bool prev = false;
      for (int s = 0; s < 7; s++) {                                         //ищем горизонтальную длину len строки, содержащей номера всех пар для обоих четностей недели подгруппы
        if (((!k) ? week[i].subj_num[s] : subj_num[s]) == 0) continue;
        if (prev) tableLen[k] += 1;
        tableLen[k] += ((!k) ? week[i].subj_num[s] : subj_num[s]);
        prev = true;
      }
    }

    for (byte iter = 0; iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель

      FirebaseJsonArray requests;
      FirebaseJson request;
      FirebaseJson rows;

      menu.editServiceMess("Начинаю сборку листа " + String(iter) + "/" + String(weeksToBuild) + ", HEAP: " + String(ESP.getFreeHeap()) + "/" + String(ESP.getHeapSize()));

      if (!i)
        request.set("copyPaste/source/sheetId", SHEET1_ID);
      else
        request.set("copyPaste/source/sheetId", SHEET2_ID);

      request.set("copyPaste/source/startRowIndex", (weekInfo_i + (offset[i] * (week_off - 2 + iter))) - 1);
      request.set("copyPaste/source/endRowIndex", (people_list_i + (offset[i] * (week_off - 2 + iter)) + people_in_subgr[i] - 2));
      request.set("copyPaste/source/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
      request.set("copyPaste/source/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      if (!i)
        request.set("copyPaste/destination/sheetId", SHEET1_ID);
      else
        request.set("copyPaste/destination/sheetId", SHEET2_ID);

      request.set("copyPaste/destination/startRowIndex", (weekInfo_i + (offset[i] * (week_off + iter)) - 1));
      request.set("copyPaste/destination/endRowIndex", (people_list_i + (offset[i] * (week_off + iter)) + people_in_subgr[i] - 2));
      request.set("copyPaste/destination/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
      request.set("copyPaste/destination/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      request.set("copyPaste/pasteType", "PASTE_NORMAL");

      requests.add(request);
      request.clear();

      if (!i)
        request.set("repeatCell/range/sheetId", SHEET1_ID);
      else
        request.set("repeatCell/range/sheetId", SHEET2_ID);

      request.set("repeatCell/range/startRowIndex", (people_list_i + (offset[i] * (week_off + iter))) - 1);
      request.set("repeatCell/range/endRowIndex", (people_list_i + (offset[i] * (week_off + iter)) + people_in_subgr[i] - 2));
      request.set("repeatCell/range/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), 1)));
      request.set("repeatCell/range/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      request.set("repeatCell/cell/userEnteredValue/stringValue", "");
      request.set("repeatCell/fields", "userEnteredValue");

      requests.add(request);
      request.clear();

      if (!i)
        request.set("updateCells/range/sheetId", SHEET1_ID);
      else
        request.set("updateCells/range/sheetId", SHEET2_ID);
      /*
      request.set("updateCells/range/startRowIndex", );
      request.set("updateCells/range/endRowIndex");
      request.set("updateCells/range/startColumnIndex", );
      request.set("updateCells/range/endColumnIndex", );
      */

      String Value = "";
      bool prev = false;
      for (byte j = 0; j < 7; j++) {
        byte numSubjects = (iter % 2) ? week[i].subj_num[j] : subj_num[j];        //введем для читаемости

        if (!numSubjects) continue;             //если пар в этот день нет - пропускаем
        if (prev) request.set("updateCells/rows/[0]/values/[0]/userEnteredValue/stringValue", "");
        prev = true;
        Value = DaysOfWeek[j];                                               //день недели
        Value += ", ";
        Date dateToWeek;
        dateToWeek.day = week[0].pon_day;
        dateToWeek.month = week[0].pon_month;
        sumDate(&dateToWeek, 7*(iter+1));
        for (byte n = 0; n < numSubjects; n++) {
          if (!n) request.set("updateCells/rows/[0]/values/[0]/userEnteredValue/stringValue", Value);
          else request.set("updateCells/rows/[0]/values/[0]/userEnteredValue/stringValue", "");
        }
      }

      request.set("updateCells/rows", rows);
      request.set("updateCells/fields", "userEnteredValue");

      menu.editServiceMess("MIN FREE HEAP: " + String(ESP.getFreeHeap()) + "/" + String(ESP.getHeapSize()));

      FirebaseJson response;
      bool success = GSheet.batchUpdate(&response, spreadsheetId, &requests, "false", "", "false");

      /*
      String responseStr;
      requests.toString(responseStr, true);                 //Вывод для отладки
      bot.sendMessage(responseStr, Admins[0]);
      */

      response.clear();
      requests.clear();
      
      if (iter) break;
    }
  }
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  //EEPROM_PUT(0, week_off);      Расскоментить когда доделаем функцию
  menu.editServiceMess("");
  return weeksToBuild;
}

uint16_t columnLetterToIndex(const String& col) {         //конвертация буквенной части адреса ячейки в абсолютное числовое значение (такой формат требует batchUpdate)
  uint16_t result = 0;
  for (uint16_t i = 0; i < col.length(); ++i) {
    char c = toupper(col[i]);
    if (c < 'A' || c > 'Z') break;
    result = result * 26 + (c - 'A' + 1);
  }
  return result - 1;
}

void sumDate(Date *date, byte day_offset) {
  int total_day = date->day + day_offset;

  while (total_day > day_month[(date->month - 1) % 12]) {           //даже с проверкой перехода нового года
    total_day -= day_month[(date->month - 1) % 12];
    date->month++;
    if (date->month > 12) date->month = 1;
  }
}