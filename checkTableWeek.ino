uint8_t checkTableWeek() {
  FB_Time realTime = bot.getTime(3);                            //структура реального времени

  if (realTime.day == 0) {
    bot.sendMessage("Структура реального времени еше не подтянулась!\nНевозможно дополнить таблицу новыми неделями!", Admins[0]);
    timer.add(bot.lastBotMsg(), 10);
  }

  //-------------------------------------------Получаем дату понедельника из таблицы-------------------------------------------
  String range = charOffset(String(weekInfo_c), 1);             //символьная составляющая адреса ячейки понедельника в Таблицах
  range += (weekInfo_i + (offset[0]*(week_off-1)));             //численная составляющая ячейки
  Text answer(list.getCells(range));
  Text cell_data = answer.getSub(r_count, "\"");       //содержит именно дату, вынутую из мусора всего ответа в формате dd.mm
  
  byte pulled_day = 0, pulled_month = 0;
  for (byte iter = 0; iter < cell_data.count("."); iter++)  {
    Text cell = cell_data.getSub(iter, ".");
    for (byte i = 0; i < cell.length(); i++) {        //если сработает эта механика, перетянуть ее и в list.begin(), в момент получения даты понедельника этой недели
      if (iter == 0)  pulled_day = (pulled_day * 10 + cell[i] - '0');
      else if (iter == 1)   pulled_month = (pulled_month * 10 + cell[i] - '0');
    }
  }
  //-------------------------------------------Получаем дату понедельника из таблицы-------------------------------------------


  //------------------------------------------Проверяем, актуальна ли неделя в Таблице------------------------------------------
  if (pulled_month == realTime.month) {
    if (realTime.day - pulled_day < 7) {
      return 0;       //отлично, в таблице прописана актуальная неделя! Создание новой/-ых недели/недель не требуется!
    }
  }
  //------------------------------------------Проверяем, актуальна ли неделя в Таблице------------------------------------------


  //-------------------------------------------Вычисляем количество недостающих недель-------------------------------------------
  byte weeksToBuild = (realTime.day - pulled_day) / 7;
  bot.sendMessage("Недостает недель: " + String(weeksToBuild), Admins[0]);
  bot.sendMessage(String(realTime.day) + " " + String(pulled_day), Admins[0]);
  return weeksToBuild;
}
