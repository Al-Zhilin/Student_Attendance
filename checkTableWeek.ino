uint8_t checkTableWeek() {            //функция проверки и достроения недель в Google Sheet
  FB_Time realTime = bot.getTime(3);                            //структура реального времени

  //добавить в будущем проверку перехода через новый год и на разные даты последней недели в 2 листах, если нужно

  if (realTime.day == 0) {
    bot.sendMessage("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!", Admins[0]);
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
  
  for (byte i = 0; i < 2; i++) {                          //цикл для всех листов
    for (byte iter = 0; iter < weeksToBuild; iter++) {        //достраиваем weeksToBuild недель
      
    }
  }

  //---------------------------------------------------Дорисовываем недостающие недели---------------------------------------------------
  EEPROM_PUT(0, week_off);
  return weeksToBuild;
  return 0;
}
