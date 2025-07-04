void getNIndex(bool subgr) {
  //FB_Time t = bot.getTime(3);
  byte weeks_ago = 0, days_ago = 0;
  int diff = 0;
  
  if (week[subgr].pon_month == nka.month) {
    if (nka.day == week[subgr].pon_day) {
      weeks_ago = 0;
      days_ago = 0;
    }

    else if (nka.day > week[subgr].pon_day) {
      weeks_ago = 0;
      days_ago = nka.day - week[subgr].pon_day;
    }

    else if (nka.day < week[subgr].pon_day) {
      diff = week[subgr].pon_day - nka.day;
      weeks_ago = (diff + 6) / 7;
      if (diff % 7 != 0) days_ago = 7 - (diff % 7);
    }
  }

  else if (nka.month < week[subgr].pon_month) {
    int d = 0;
    for (byte i = nka.month+1; i < week[subgr].pon_month; i++) {  // Начинаем с nka.month, а не nka.month+1.
      d += day_month[i - 1];
    }
    diff = (week[subgr].pon_day + day_month[nka.month-1]) - nka.day + d;
    weeks_ago = (diff + 6) / 7;
    if (diff % 7 != 0) days_ago = 7 - (diff % 7);
  }

  else if (nka.month > week[subgr].pon_month) {
    weeks_ago = 0;
    days_ago = (day_month[week[subgr].pon_month-1] + nka.day) - week[subgr].pon_day;
  }

  byte k = 0;
  bool found = false;
  for (int i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
    if (students[i].surname == nka.surn)  {
      found = true;
      nka.posI = (people_list_i + (offset[subgr] * (week_off-1 - weeks_ago))) + k;
      break;
    }
    if (students[i].subgroup == nka.subgroup) k++;
  }

  if (!found) {
    bot.sendMessage(F("GetIndex: surname not found!"), error_chat);
    timer.add(bot.lastBotMsg(), 15);
  }
  int sm = 1;
  bool prev = false;

  for (int i = 0; i < days_ago; i++) {
    if (week[subgr].subj_num[i] == 0) continue;

    if (prev) sm++;          // разделитель между непустыми днями
    else prev = true;

    sm += week[subgr].subj_num[i];
  }
  
  if (weeks_ago % 2 == 0) nka.parity = week[nka.subgroup].parity;
  else nka.parity = !week[nka.subgroup].parity;
  nka.dayWeek = days_ago+1;
  nka.posC = charOffset(String(people_list_c), sm);
}
