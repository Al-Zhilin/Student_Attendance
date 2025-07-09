uint8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  FB_Time realTime = bot.getTime(3);                            //структура реального времени

  //добавить в будущем проверку перехода через новый год и на разные даты последней недели в 2 листах, если нужно

  if (realTime.day == 0) {
    bot.sendMessage(F("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!"), Admins[0]);
    timer.add(bot.lastBotMsg(), 10);
  }

  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  byte pulled_day = week[0].pon_day + (realTime.dayWeek-1);         //далее сравниывем даты по дням недели. week[i].pon_day всегда дата понедельника, а прибавлением дня недели делаем дату, соответственно текущему дню недели. Упрощает дальнейшие расчеты
  byte weeksToBuild = 0;

  if (week[0].pon_month == realTime.month) {       //если месяцы одинаковые
    if (realTime.day - pulled_day == 0) {
      return 0;       //отлично, в таблице прописана актуальная неделя! Создание новой/-ых недели/недель не требуется!
    }

    else {
      weeksToBuild = (realTime.day - pulled_day) / 7;      //количество недель, которые нужно достроить
    }
  }

  else {                                                            //рассчитываем количество дней, а в последствии недель, которые нужно достроить в случае, когда месяца дат не равны
    int days_between = day_month[week[0].pon_month-1] - pulled_day;
    for (byte i = 1; i < realTime.month - week[i].pon_month; i++) {
      days_between += day_month[week[i].pon_month+i-1];
    }
    days_between += realTime.day;
    weeksToBuild = days_between / 7;
    bot.sendMessage(String(days_between), Admins[0]);
  }
  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  
  bot.sendMessage("Нужно достроить: " + String(weeksToBuild), Admins[0]);
  

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  byte tableLen[2] = {};        //длина таблицы для 2 четностей подгруппы, таблица в которой сейчас достраивается

  for (byte i = 0; i < 2; i++) {                          //цикл для листов 2 подгрупп

    if (!week[2+i].pon_day) {         //если структура не содержит данных - заполняем
      String range = "";
      if (!i) range += Sheet1;
      else range += Sheet2;
      range += weekInfo_c;
      range += (weekInfo_i + (offset[i]*(week_off-2)));
      range += ":";
      range += charOffset(String(weekInfo_c), 1);
      range += (weekInfo_i + (offset[i]*(week_off-2)));
      Text answer(list.getCells(range));
      list.getBriefCellData(&week[2+i], answer);
    }

    for (byte k = 0; k < 2; k++) {
      bool prev = false;
      for (int s = 0; s < 7; s++) {                 //ищем горизонтальную длину len строки, содержащей номера всех пар для обоих четностей недели подгруппы
        if (week[i+2*k].subj_num[s] == 0) continue; 
        if (prev) tableLen[k] += 1;
        tableLen[k] += week[i+2*k].subj_num[s];       //---------------------------------------------ПЕРЕПИСАТЬ БЕЗ СОЗДАНИЯ НОВЫХ WEEK!!!!------------------------------------------------------------
        prev = true;
      }
    }

    for (byte iter = 0; iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель

      FirebaseJsonArray requests;
      FirebaseJson request;

      bot.sendMessage("Начинаю сборку листа " + String(iter) + "/" + String(i+1) + ", HEAP: " + String(ESP.getFreeHeap()) + "/" + String(ESP.getHeapSize()), Admins[0]);

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

      /*if (!i)
        request.set("updateCells/range/sheetId", SHEET1_ID);
      else
        request.set("updateCells/range/sheetId", SHEET2_ID);
      
      request.set("updateCells/range/startRowIndex", );
      request.set("updateCells/range/endRowIndex");
      request.set("updateCells/range/startColumnIndex", );
      request.set("updateCells/range/endColumnIndex");
      request.set("updateCells/raws/");
      */

      bot.sendMessage("MIN FREE HEAP: " + String(ESP.getFreeHeap()) + "/" + String(ESP.getHeapSize()), Admins[0]);

      FirebaseJson response;
      bool success = GSheet.batchUpdate(&response, spreadsheetId, &requests, "false", "", "false");

      String responseStr;
      requests.toString(responseStr, true);
      bot.sendMessage(responseStr, Admins[0]);

      response.clear();
      requests.clear();
      
      if (iter) return 0;
    }
  }
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  EEPROM_PUT(0, week_off);
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


