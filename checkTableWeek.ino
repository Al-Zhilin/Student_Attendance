int8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  realTime = bot.getTime(3);         //обновили время
  serviceMess.edit("Проверяю актуальность недели в таблице...");

  if (!bot.timeSynced()) {
    bot.sendMessage(F("Структура реального времени еще не подтянулась!\nНевозможно дополнить таблицу новыми неделями!"), error_chat);
    timer.add(bot.lastBotMsg(), 10, error_chat);
    return -1;
  }

  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  Date dateToWeek(week[week_off == 1]->pon_date.day, week[week_off == 1]->pon_date.month, week[week_off == 1]->pon_date.year);    // Вопросы к индексу? Разбирайся с логикой построки для случая начала семестра:
                                                                                                                                  // 1 и !2! недели построены по умолчанию, но этот факт нам же нужно указать, для корректности увеличения даты в 3 и далее неделе
  sumDate(&dateToWeek, realTime.dayWeek-1);                        //временно сравняем дни недели реальной даты и последней недели в таблице, чтобы сделать все расчеты кратными и тем самым сильно упростить их

  int16_t days_between = StampUtils::dateToDays2000(realTime.day, realTime.month, realTime.year) - StampUtils::dateToDays2000(dateToWeek.day, dateToWeek.month, dateToWeek.year);         // разница в днях
  
  if (days_between < 0) {
    serviceMess.edit("");
    if (week_off == 1 && days_between == -7) return 0;                  // нормально, это исключительный случай
    bot.sendMessage(F("Внимание! Дата первого дня недели в таблице в будущем, относительно реальной!"), error_chat);
    CriticalError();
  }

  if (days_between == 0) {
    serviceMess.edit("В таблице записана актуальная неделя!", 5000);
    return 0;       //отлично, в таблице прописана актуальная неделя! Создание новой/-ых недели/недель не требуется!
  }

  byte weeksToBuild = days_between / 7;

  if (days_between % 7 != 0)  {           // т.к. мы ранее выравнивали вытянутую дату по дню недели с текущей, то такой остаток явно показывает ошибку в логике расчета
      bot.sendMessage(F("WARNING! Возможна ошибка с расчетом количества недель к достариванию!\nКритично!"), error_chat);
      serviceMess.edit("Достроение недель прервано в связи с некорректной работой алгоритма расчета!", 10000);
      return -1;
  }

  if (week_off + weeksToBuild < 3) {                                        // т.к. первые 2 недели в таблице всего построены изначально - их нет смысла рисовать, просто документируем этот факт и идем пить чяй
    serviceMess.edit("В таблице записана актуальная неделя!", 5000);        // ?????????????????  Нужно ли это  ???????????????
    week_off += weeksToBuild;
    week_file.update();
    return weeksToBuild;
  }

  sumDate(&dateToWeek, -(realTime.dayWeek-1));                              // сдвинули обратно (см. выше), нам теперь важны актуальные данные
  serviceMess.edit("Нужно достроить недель: " + String(weeksToBuild));

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  MemoryControl MemControl;             // для контроля свободной оперы во время тяжелый операций с Ohhh Sheet`ом и Json`ом Стетхэмом

  if (!MemControl.check())  {           // вот так кстати оно и проверяется. В будущем оформим в более компактную и красивую форму
    bot.sendMessage(F("Возможна нехватка свободной памяти!\nКритично!"), error_chat);
    return -1;                          // подумать! я правда уже забыл, о чем надо подумать....
  }
  
  sumDate(&dateToWeek, 6);

  for (byte iter = ((week_off == 1) ? 1 : 0); iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель. Учитывает, что первые 2 недели в таблице всегда построены

    MemControl.resetKeep();         // сбрасывает сохраненное значение памяти

    FirebaseJsonArray requests;     // массив запросов
    FirebaseJson request;           // здесь запросы собираются перед попаданием в массив

    serviceMess.edit("Достраиваю неделю " + String(iter+1) + "/" + String(weeksToBuild));

    //---------------------------------------------------------Сopy-Paste запрос---------------------------------------------------------
    request.set("copyPaste/source/sheetId", SHEET_ID);

    request.set("copyPaste/source/startRowIndex", (weekInfo_i + (offset * (week_off - 2 + iter))) - 1);
    request.set("copyPaste/source/endRowIndex", (people_list_i + (offset * (week_off - 2 + iter)) + sizeof(students)/sizeof(students[0]) - 1));
    request.set("copyPaste/source/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
    request.set("copyPaste/source/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), settings.table_width[iter % 2 == 0])));

    request.set("copyPaste/destination/sheetId", SHEET_ID);

    request.set("copyPaste/destination/startRowIndex", (weekInfo_i + (offset * (week_off + iter)) - 1));
    request.set("copyPaste/destination/endRowIndex", (people_list_i + (offset * (week_off + iter)) + sizeof(students)/sizeof(students[0]) - 1));
    request.set("copyPaste/destination/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
    request.set("copyPaste/destination/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), settings.table_width[iter % 2 == 0])));

    request.set("copyPaste/pasteType", "PASTE_NORMAL");

    requests.add(request);
    request.clear();

    if (!MemControl.check()) {
      bot.sendMessage("Достроение недель прервано! Нехватка RAM!", error_chat);
      requests.clear();
    }
    //---------------------------------------------------------Сopy-Paste запрос---------------------------------------------------------



    //------------------------------------------------------Запрос очистки диапазона------------------------------------------------------
    request.set("repeatCell/range/sheetId", SHEET_ID);

    request.set("repeatCell/range/startRowIndex", (people_list_i + (offset * (week_off + iter))) - 1);
    request.set("repeatCell/range/endRowIndex", (people_list_i + (offset * (week_off + iter)) + sizeof(students)/sizeof(students[0]) - 1));
    request.set("repeatCell/range/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), 1)));
    request.set("repeatCell/range/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), settings.table_width[iter % 2 == 0])));

    request.set("repeatCell/cell/userEnteredValue/stringValue", "");
    request.set("repeatCell/fields", "userEnteredValue");

    requests.add(request);
    request.clear();

    if (!MemControl.check()) {
      bot.sendMessage("Достроение недель прервано! Нехватка RAM!", error_chat);
      requests.clear();
    }
    //------------------------------------------------------Запрос очистки диапазона------------------------------------------------------



    //-----------------------------------------------Запрос для обновления дат в заголовках дней-----------------------------------------------
    FirebaseJsonArray valuesArray;

    request.set("updateCells/range/sheetId", SHEET_ID);
    
    request.set("updateCells/range/startRowIndex", (weekInfo_i + (offset * (week_off + iter)) - 1));
    request.set("updateCells/range/endRowIndex", (weekInfo_i + (offset * (week_off + iter))));
    request.set("updateCells/range/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), 1)));
    request.set("updateCells/range/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), settings.table_width[iter % 2 == 0])));

    String Value = "";
    bool prev = false;

    for (byte j = 0; j < 7; j++) {     
      byte numSubjects = 0;               //введем для читаемости в отдельную переменную

      if (iter % 2 != 0) numSubjects = week[0]->days[j].subj_num;
      else  numSubjects = week[1]->days[j].subj_num;   

      if (!numSubjects) {
        sumDate(&dateToWeek, 1);      //+1, т.к. переходим к следующему дню
        continue;                     //если пар в этот день нет - пропускаем
      }

      if (prev) valuesArray.add(FirebaseJson().set("userEnteredValue/stringValue", ""));
        
      sumDate(&dateToWeek, 1);

      Value = DaysOfWeek[j];                                               //день недели
      Value += ", ";
      if (dateToWeek.day < 10) Value += "0";
      Value += dateToWeek.day;
      Value += ".";
      if (dateToWeek.month < 10) Value += "0";
      Value += dateToWeek.month;
      Value += ".";
      Value += dateToWeek.year;
      
      for (byte n = 0; n < numSubjects; n++) {
        if (!n) valuesArray.add(FirebaseJson().set("userEnteredValue/stringValue", Value));
        else valuesArray.add(FirebaseJson().set("userEnteredValue/stringValue", ""));
      }

      prev = true;
    }

    FirebaseJson rowObject;
    rowObject.set("values", valuesArray);

    FirebaseJsonArray rowsArray;
    rowsArray.add(rowObject);

    request.set("updateCells/rows", rowsArray);
    request.set("updateCells/fields", "userEnteredValue");
    requests.add(request);
    request.clear();

    if (!MemControl.check()) {
      bot.sendMessage("Достроение недель прервано! Нехватка RAM!", error_chat);
      requests.clear();
      request.clear();
    }
    //-----------------------------------------------Запрос для обновления дат в заголовках дней-----------------------------------------------


    serviceMess.edit("Достраиваю неделю " + String(iter+1) + "/" + String(weeksToBuild) + "\n" + "Этот лист занимает " + String(MemControl.getDiff()/1024) + " кБ в RAM\nВсего - " + String(ESP.getHeapSize()/1024) + " кБ, Свободно - " + String(ESP.getFreeHeap()/1024) + " кБ");

    FirebaseJson response;
    bool success = GSheet.batchUpdate(&response, spreadsheetId, &requests, "false", "", "false");
    

    response.clear();
    requests.clear();
  }
  
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  serviceMess.edit("Достроено " + String(weeksToBuild) + " недель!", 5000);
  week_off += weeksToBuild;
  week_file.update();

  if (weeksToBuild % 2 != 0) {                  //тогда меняем местами указатели. Настоящаая четность поменялась
    WeekInfo *temp = week[0];
    week[0] = week[1];
    week[1] = temp;
  }

  sumDate(&dateToWeek, -6);

  week[0]->pon_date = dateToWeek;               //делаем pon_day и pon_month актуальными под последние недели
  sumDate(&dateToWeek, -7);
  week[1]->pon_date = dateToWeek;

  CriticalError();

  return weeksToBuild;
}

uint16_t columnLetterToIndex(const String& col) {         //  конвертация буквенной части адреса ячейки в абсолютное числовое значение (такой формат требует batchUpdate)
  uint16_t result = 0;
  for (uint16_t i = 0; i < col.length(); ++i) {
    char c = toupper(col[i]);
    if (c < 'A' || c > 'Z') break;
    result = result * 26 + (c - 'A' + 1);
  }
  return result - 1;
}

void sumDate(Date *date, int day_offset) {               // функция суммирования (как в плюс, так и в минус) структуры Date с неким числом дней. Изменяет напрямую переданный обьект
  int total_day = date->day + day_offset;

  // Прибавление дней
  while (total_day > getDayInMonth(date->month-1, date->year)) {
    total_day -= getDayInMonth(date->month-1, date->year);
    date->month++;
    if (date->month > 12) {
      date->month = 1;
      date->year++;
    }
  }

  // Вычитание дней
  while (total_day <= 0) {
    date->month--;
    if (date->month < 1) {
      date->month = 12;
      date->year--;
    }
    total_day += getDayInMonth(date->month-1, date->year);
  }

  date->day = total_day;
}