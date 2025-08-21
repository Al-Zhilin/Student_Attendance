int8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  FB_Time realTime = bot.getTime(3);                            //структура реального времени
  uint32_t Heap;

  editServiceMess("Проверяю актуальность недели в таблице...");

  //добавить в будущем проверку перехода через новый год и на разные даты последней недели в 2 листах, если нужно

  if (realTime.day == 0) {
    bot.sendMessage(F("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!"), error_chat);
    timer.add(bot.lastBotMsg(), 10, error_chat);
    return -1;
  }

  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  Date dateToWeek(week[0]->pon_date.day, week[0]->pon_date.month);
  
  byte pulled_day = dateToWeek.day + (realTime.dayWeek-1);         //далее сравниывем даты по дням недели. week[i].pon_day всегда дата понедельника, а прибавлением дня недели делаем дату, соответственно текущему дню недели. Упрощает дальнейшие расчеты
  byte pulled_month = dateToWeek.month;

  if (pulled_day > day_month[pulled_month-1]) {
    pulled_day -= day_month[pulled_month-1];
    pulled_month++;
  }

  byte weeksToBuild = 0;

  if (pulled_month == realTime.month) {       //если месяцы одинаковые
    if (realTime.day == pulled_day) {
      editServiceMess("В таблице записана актуальная неделя!");
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
    if (days_between % 7 != 0)  bot.sendMessage(F("WARNING! Возможна ошибка с расчетом количества недель к достариванию!"), error_chat);
  }
  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  
  editServiceMess("Нужно достроить недель: " + String(weeksToBuild));

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  byte tableLen[2] = {};        //длина таблицы для 2 четностей подгруппы, таблица в которой сейчас достраивается

  if (ESP.getFreeHeap()/1024 < 40)  {
    bot.sendMessage(F("Возможна нехватка свободной памяти!\nДостроение новых недель прервано!"), error_chat);
  }
  
  for (byte i = 0; i < 2; i++) {                          //цикл для листов 2 подгрупп
    // Получаем данные о парах кахдого дня недели противоположной и настоящей четности подгруппы (нужно для tableLen и дальнейшего заполнения)
    for (byte z = 0; z < 2; z++) {        //цикл получает нужные данные для данной недели и для недели прошлой четности
      // ищем 2 длины - для каждой четности недели у i подгруппы
      bool prev = false;
      tableLen[z] = 0;

      for (int s = 0; s < 7; s++) {                                         //ищем горизонтальную длину len строки, содержащей номера всех пар для обоих четностей недели подгруппы
        if (week[i+2*z]->subj_num[s] == 0) continue;
        if (prev) tableLen[z] += 1;
        tableLen[z] += week[i+2*z]->subj_num[s];
        prev = true;
      }
    }
    
    dateToWeek.day = week[i]->pon_date.day;
    dateToWeek.month = week[i]->pon_date.month;
    sumDate(&dateToWeek, 6);

    for (byte iter = 0; iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель

      FirebaseJsonArray requests;     //массив запросов
      FirebaseJson request;         //храним по очереди все запросы перед добавлением в массив запросовE

      editServiceMess("Достраиваю неделю " + String(iter+1) + "/" + String(weeksToBuild) + ", подгруппы " + String(i+1) + "/2");
      Heap = ESP.getFreeHeap();     //засекаем количество свободной памяти до сборки JSON`ов


      //---------------------------------------------------------Сopy-Paste запрос---------------------------------------------------------
      if (!i)
        request.set("copyPaste/source/sheetId", SHEET1_ID);
      else
        request.set("copyPaste/source/sheetId", SHEET2_ID);

      request.set("copyPaste/source/startRowIndex", (weekInfo_i + (offset[i] * (file_data.week_off - 2 + iter))) - 1);
      request.set("copyPaste/source/endRowIndex", (people_list_i + (offset[i] * (file_data.week_off - 2 + iter)) + people_in_subgr[i] - 1));
      request.set("copyPaste/source/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
      request.set("copyPaste/source/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      if (!i)
        request.set("copyPaste/destination/sheetId", SHEET1_ID);
      else
        request.set("copyPaste/destination/sheetId", SHEET2_ID);

      request.set("copyPaste/destination/startRowIndex", (weekInfo_i + (offset[i] * (file_data.week_off + iter)) - 1));
      request.set("copyPaste/destination/endRowIndex", (people_list_i + (offset[i] * (file_data.week_off + iter)) + people_in_subgr[i] - 1));
      request.set("copyPaste/destination/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), -1)));
      request.set("copyPaste/destination/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      request.set("copyPaste/pasteType", "PASTE_NORMAL");

      requests.add(request);
      request.clear();
      //---------------------------------------------------------Сopy-Paste запрос---------------------------------------------------------



      //------------------------------------------------------Запрос очистки диапазона------------------------------------------------------
      if (!i)
        request.set("repeatCell/range/sheetId", SHEET1_ID);
      else
        request.set("repeatCell/range/sheetId", SHEET2_ID);

      request.set("repeatCell/range/startRowIndex", (people_list_i + (offset[i] * (file_data.week_off + iter))) - 1);
      request.set("repeatCell/range/endRowIndex", (people_list_i + (offset[i] * (file_data.week_off + iter)) + people_in_subgr[i] - 1));
      request.set("repeatCell/range/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), 1)));
      request.set("repeatCell/range/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      request.set("repeatCell/cell/userEnteredValue/stringValue", "");
      request.set("repeatCell/fields", "userEnteredValue");

      requests.add(request);
      request.clear();
      //------------------------------------------------------Запрос очистки диапазона------------------------------------------------------



      //-----------------------------------------------Запрос обновления дат в заголовках дней-----------------------------------------------
      FirebaseJsonArray valuesArray;

      if (!i)
        request.set("updateCells/range/sheetId", SHEET1_ID);
      else
        request.set("updateCells/range/sheetId", SHEET2_ID);
      
      request.set("updateCells/range/startRowIndex", (weekInfo_i + (offset[i] * (file_data.week_off + iter)) - 1));
      request.set("updateCells/range/endRowIndex", (weekInfo_i + (offset[i] * (file_data.week_off + iter))));
      request.set("updateCells/range/startColumnIndex", columnLetterToIndex(charOffset(String(weekInfo_c), 1)));
      request.set("updateCells/range/endColumnIndex", columnLetterToIndex(charOffset(String(less_num_c), tableLen[iter % 2 == 0])));

      String Value = "";
      bool prev = false;

      for (byte j = 0; j < 7; j++) {     
        byte numSubjects = 0;               //введем для читаемости в отдельную переменную

        if (iter % 2 != 0) numSubjects = week[i]->subj_num[j];
        else  numSubjects = week[i+2]->subj_num[j];   

        if (!numSubjects) {
          sumDate(&dateToWeek, 1);      //+1, т.к. переходим к следующему дню
          continue;             //если пар в этот день нет - пропускаем
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
      //-----------------------------------------------Запрос обновления дат в заголовках дней-----------------------------------------------


      editServiceMess("Достраиваю неделю " + String(iter+1) + "/" + String(weeksToBuild) + ", подгруппы " + String(i+1) + "/2\n" + "Этот лист занимает " + String((Heap - ESP.getFreeHeap())/1024) + " кБ в RAM\nВсего - " + String(ESP.getHeapSize()/1024) + " кБ, Свободно - " + String(ESP.getFreeHeap()/1024) + " кБ");

      FirebaseJson response;
      bool success = GSheet.batchUpdate(&response, spreadsheetId, &requests, "false", "", "false");

      /*
      String responseStr;
      requests.toString(responseStr, true);                 //Вывод ответа от Google Sheets API для отладки
      bot.sendMessage(responseStr, error_chat);
      */

      response.clear();
      requests.clear();
    }
  }
  
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  editServiceMess("Достроено " + String(weeksToBuild) + " недель!");
  file_data.week_off += weeksToBuild;
  settings_file.update();

  if (weeksToBuild % 2 != 0) {                  //тогда меняем местами указатели. Настоящаая четность поменялась
    for (byte x = 0; x < 2; x++) {
      WeekInfo *temp = week[x];
      week[x] = week[x + 2];
      week[x + 2] = temp;
    }
  }

  sumDate(&dateToWeek, -6);

  for (byte k = 0; k < 2; k++) {                //делаем pon_day и pon_month актуальными под последние недели. Какая то сложная схема, мб нужно переделать на что то попроще или вообще без цикла
    if (k) sumDate(&dateToWeek, -7);
    week[0+2*k]->pon_date.day = dateToWeek.day;
    week[1+2*k]->pon_date.day = dateToWeek.day;
    week[0+2*k]->pon_date.month = dateToWeek.month;
    week[1+2*k]->pon_date.month = dateToWeek.month;
  }
  
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

void sumDate(Date *date, int day_offset) {              //функция суммирования (как в плюс, так и в минус) структуры Date с неким числом дней. Изменяет напрямую переданный обьект
  int total_day = date->day + day_offset;

  // Прибавление дней
  while (total_day > day_month[(date->month - 1) % 12]) {           //даже с проверкой перехода нового года
    total_day -= day_month[(date->month - 1) % 12];
    date->month++;
    if (date->month > 12) date->month = 1;
  }

  // Вычитание дней
  while (total_day <= 0) {
    date->month--;
    if (date->month < 1) date->month = 12;
    total_day += day_month[(date->month - 1) % 12];
  }

  date->day = total_day;
}