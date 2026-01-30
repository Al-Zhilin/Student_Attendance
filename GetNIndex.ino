void getNIndex() {
  byte weeks_ago = 0, days_ago = 0;

  int diff = StampUtils::dateToDays2000(realTime.day, realTime.month, realTime.year) - StampUtils::dateToDays2000(nka.date.day, nka.date.month, nka.date.year);           // разница через кол-во дней с 01.01.2000

  if (diff < 0) {     // нка ставится наперед, !на текущую неделю! (слишком наперед низя, таких недель банально нет в таблице еще)
    days_ago = nka.date.day - week[0]->pon_date.day;
  } else {            // в прошлое
    weeks_ago = (diff + 6) / 7;
    if (diff % 7 != 0) days_ago = 7 - (diff % 7);
  }

  byte k = 0;
  bool found = false;

  if (nka.surn == "")  {
    nka.posI = (people_list_i + (offset * (week_off-1 - weeks_ago)));
    found = true;
  }

  else {
    for (int i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
      if (students[i].surname == nka.surn)  {
        found = true;
        nka.posI = (people_list_i + (offset * (week_off-1 - weeks_ago))) + k;
        break; 
      }
      k++;
    }
  }

  if (!found) {
    bot.sendMessage(F("GetNIndex: surname not found!"), error_chat);
    timer.add(bot.lastBotMsg(), 20, error_chat);
  }

  if (weeks_ago % 2 == 0) nka.parity = week[0]->parity;
  else nka.parity = !week[0]->parity;
  nka.dayWeek = days_ago+1;

  int sm = 1;

  for (int i = 0; i < days_ago; i++) {
    if (week[week[0]->parity != nka.parity]->subj_num[i] == 0) continue;
    sm++;
    sm += week[week[0]->parity != nka.parity]->subj_num[i];
  }
  
  nka.posC = charOffset(String(people_list_c), sm);
}
