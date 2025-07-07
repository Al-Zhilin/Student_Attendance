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
  }
  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  
  byte tableLen[4] = {};        //длина таблицы для 2 подгрупп для 2 вариантов четности

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

    for (byte iter = 0; iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель

      int srcColEnd = 3;           //столбец конца диапазона копирования
      int dstRowStart = 0, dstColStart = 5;         //строка и столбец ячейки вставки скопированного диапазона

      int clearRowStart = 2, clearRowEnd = 4;       //строки начала и конца диапазона очистки
      int clearColStart = 5, clearColEnd = 8;       //столбцы начала и конца диапазона очистки

      FirebaseJsonArray requests;
      FirebaseJson request;
      
      if (!i) request.set("copyPaste/source/range/sheetId", SHEET1_ID);
      else  request.set("copyPaste/source/range/sheetId", SHEET2_ID);

      request.set("copyPaste/source/range/startRowIndex", (weekInfo_i + (offset[i]*(week_off-2+iter)))) - 1;
      request.set("copyPaste/source/range/endRowIndex", (people_list_i + (offset[i]*(week_off-2+iter))) + people_in_subgr[i]) - 1;
      request.set("copyPaste/source/range/startColumnIndex", columnLetterToIndex(charOffset(weekInfo_c, -1)));
      request.set("copyPaste/source/range/endColumnIndex", srcColEnd);

      if (!i) request.set("copyPaste/destination/range/sheetId", SHEET1_ID);
      else  request.set("copyPaste/destination/range/sheetId", SHEET2_ID);

      request.set("copyPaste/destination/range/startRowIndex", dstRowStart);
      request.set("copyPaste/destination/range/startColumnIndex", dstColStart);
      request.set("copyPaste/pasteType", "PASTE_NORMAL");
    
      requests.add(request);
      request.clear();

      if (!i) request.set("repeatCell/range/sheetId", SHEET1_ID);
      else request.set("repeatCell/range/sheetId", SHEET2_ID);

      request.set("repeatCell/range/startRowIndex", clearRowStart);
      request.set("repeatCell/range/endRowIndex", clearRowEnd);
      request.set("repeatCell/range/startColumnIndex", clearColStart);
      request.set("repeatCell/range/endColumnIndex", clearColEnd);
      request.set("repeatCell/cell/userEnteredValue/stringValue", "");
      request.set("repeatCell/fields", "userEnteredValue");
      requests.add(request);
      request.clear();

      FirebaseJson response;
      bool success = GSheet.batchUpdate(&response, spreadsheetId, &requests, "false", "", "false");
      response.clear();
      requests.clear();
    }
  }
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  EEPROM_PUT(0, week_off);
  return weeksToBuild;
  return 0;
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


