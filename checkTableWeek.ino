uint8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  FB_Time realTime = bot.getTime(3);                            //структура реального времени

  //добавить в будущем проверку перехода через новый год и на разные даты последней недели в 2 листах, если нужно

  if (realTime.day == 0) {
    bot.sendMessage("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!", Admins[0]);
    timer.add(bot.lastBotMsg(), 10);
  }

  //-------------------------------------------Получаем дату понедельника из таблицы-------------------------------------------
  
  String range = Sheet1;
  range += weekInfo_c;
  range += (weekInfo_i + (offset[0]*(week_off-1)));
  range += ":";
  range += charOffset(String(weekInfo_c), 1);             //символьная составляющая адреса ячейки понедельника в Таблицах
  range += (weekInfo_i + (offset[0]*(week_off-1)));             //численная составляющая ячейки
  Text answer(list.getCells(range));

  Text cell_data = answer.getSub(r_count+r_offset, "\"");       //содержит именно дату, вынутую из мусора всего ответа в формате dd.mm
  
  byte pulled_day = 0, pulled_month = 0;
  for (byte iter = 0; iter < cell_data.count("."); iter++)  {
    Text cell = cell_data.getSub(iter, ".");
    for (byte i = 0; i < cell.length(); i++) {        //если сработает эта механика, перетянуть ее и в list.begin(), в момент получения даты понедельника этой недели
      if (iter == 0)  pulled_day = (pulled_day * 10 + cell[i] - '0');
      else if (iter == 1)   pulled_month = (pulled_month * 10 + cell[i] - '0');
    }
  }
  //-------------------------------------------Получаем дату понедельника из таблицы-------------------------------------------


  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  pulled_day += (realTime.dayWeek-1);         //далее сравниывем даты по дням недели. pulled_day всегда дата понедельника, а прибавлением дня недели делаем дату, соответственно текущему дню недели. Упрощает дальнейшие расчеты
  byte weeksToBuild = 0;

  if (pulled_month == realTime.month) {       //если месяцы одинаковые
    if (realTime.day - pulled_day == 0) {
      return 0;       //отлично, в таблице прописана актуальная неделя! Создание новой/-ых недели/недель не требуется!
    }

    else {
      weeksToBuild = (realTime.day - pulled_day) / 7;      //количество недель, которые нужно достроить
    }
  }

  else {                                                            //рассчитываем количество дней, а в последствии недель, которые нужно достроить в случае, когда месяца дат не равны
    int days_between = day_month[pulled_month-1] - pulled_day;
    for (byte i = 1; i < realTime.month - pulled_month; i++) {
      days_between += day_month[pulled_month+i-1];
    }
    days_between += realTime.day;
    weeksToBuild = days_between / 7;
  }
  //---------------------Проверяем, актуальна ли неделя в Таблице, если нет - считаем количество отсутствующих недель---------------------
  

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  cell_data = answer.getSub(r_count, "\"");            //содержить данные из ячейки с краткой информацией. Начинаем доставать нужные нам величины
  bool last_parity;
  String get_cell = "";

  for (byte iter = 0; iter < ans.count("/"); iter++) {        //проходим по всем величинам
    ans.getSub(iter, "/").toString(get_cell);
    get_cell.toLowerCase();
    if (!iter)
  }

  for (byte i = 0; i < weeksToBuild; i++) {
    
  }
  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------

  return weeksToBuild;
}
