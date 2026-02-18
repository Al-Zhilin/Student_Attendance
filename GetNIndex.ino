[[nodiscard]] bool getNIndex() {
  byte weeks_ago = 0, days_ago = 0;

  int diff = StampUtils::dateToDays2000(week[0]->pon_date.day, week[0]->pon_date.month, week[0]->pon_date.year) - StampUtils::dateToDays2000(nka.date.day, nka.date.month, nka.date.year);           // разница через кол-во дней с 01.01.2000

  if (diff < 0) {     // нка ставится наперед, !на текущую неделю! (слишком наперед низя, таких недель банально нет в таблице еще)

    Date now_date(realTime.day, realTime.month, realTime.year);
    Date week_date(week[0]->pon_date);
    sumDate(&week_date, 6);
    sumDate(&now_date, abs(diff));

    if (week_date < now_date) {           // проверяет: не переходит ли запрашиваемая дата за границы ТЕКУЩЕЙ недели, иначе не сможем найти позицию - недели просто нет в Sheet!
      bot.sendMessage(F("Попытка расчета индекса даты из будущего (getNIndex)! Ошибка, операция прервана!"), error_chat);
      return false;
    }

    days_ago = nka.date.day - week[0]->pon_date.day;
  } else {            // Нка ставится в прошлое
    weeks_ago = (diff + 6) / 7;
    if (diff % 7 != 0) days_ago = 7 - (diff % 7);
  }

  bool found = false;

  if (nka.surn == "")  {
    nka.posI = (people_list_i + (offset * (week_off-1 - weeks_ago)));
    found = true;
  }

  else {
    for (uint8_t i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
      if (students[i].surname == nka.surn)  {
        found = true;
        nka.posI = (people_list_i + (offset * (week_off-1 - weeks_ago))) + i;
        break;
      }
    }
  }

  if (!found) {
    bot.sendMessage(F("GetNIndex: surname not found!"), error_chat);
    timer.add(bot.lastBotMsg(), 20, error_chat);
    return false;
  }

  if (weeks_ago % 2 == 0) nka.parity = week[0]->parity;
  else nka.parity = !week[0]->parity;
  nka.dayWeek = days_ago+1;

  uint16_t sm = 1;
  
  for (int i = 0; i < days_ago; i++) {
    sm += 1+week[week[0]->parity != nka.parity]->days[i].subj_num;
  }   // P.S. алгоритм получил "магические числа" в процессе оптимизации, см. ранние коммиты (до февраля 2026), чтобы вникнуть в суть
  
  nka.posC = charOffset(String(people_list_c), sm);
  return true;
}