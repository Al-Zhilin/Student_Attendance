void getNIndex() {
  byte weeks_ago = 0, days_ago = 0;
  int diff = 0;
  
  if (week[nka.subgroup]->pon_date.month == nka.date.month) {
    if (nka.date.day == week[nka.subgroup]->pon_date.day) {
      weeks_ago = 0;
      days_ago = 0;
    }

    else if (nka.date.day > week[nka.subgroup]->pon_date.day) {
      weeks_ago = 0;
      days_ago = nka.date.day - week[nka.subgroup]->pon_date.day;
    }

    else if (nka.date.day < week[nka.subgroup]->pon_date.day) {
      diff = week[nka.subgroup]->pon_date.day - nka.date.day;
      weeks_ago = (diff + 6) / 7;
      if (diff % 7 != 0) days_ago = 7 - (diff % 7);
    }
  }

  else if (nka.date.month < week[nka.subgroup]->pon_date.month) {
    int d = 0;
    for (byte i = nka.date.month+1; i < week[nka.subgroup]->pon_date.month; i++) {
      d += day_month[i - 1];
    }
    diff = (week[nka.subgroup]->pon_date.day + day_month[nka.date.month-1]) - nka.date.day + d;
    weeks_ago = (diff + 6) / 7;
    if (diff % 7 != 0) days_ago = 7 - (diff % 7);
  }

  else if (nka.date.month > week[nka.subgroup]->pon_date.month) {
    weeks_ago = 0;
    days_ago = (day_month[week[nka.subgroup]->pon_date.month-1] + nka.date.day) - week[nka.subgroup]->pon_date.day;
  }

  byte k = 0;
  bool found = false;

  if (nka.surn == "")  {
    nka.posI = (people_list_i + (offset[nka.subgroup] * (week_off-1 - weeks_ago)));
    found = true;
  }

  else {
    for (int i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
      if (students[i].surname == nka.surn)  {
        found = true;
        nka.posI = (people_list_i + (offset[nka.subgroup] * (week_off-1 - weeks_ago))) + k;
        break; 
      }
      if (students[i].subgroup == nka.subgroup) k++;
    }
  }

  if (!found) {
    bot.sendMessage(F("GetIndex: surname not found!"), error_chat);
    timer.add(bot.lastBotMsg(), 20, error_chat);
  }

  if (weeks_ago % 2 == 0) nka.parity = week[nka.subgroup]->parity;
  else nka.parity = !week[nka.subgroup]->parity;
  nka.dayWeek = days_ago+1;

  int sm = 1;

  for (int i = 0; i < days_ago; i++) {
    if (week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[i] == 0) continue;
    sm++;
    sm += week[nka.subgroup + ((week[nka.subgroup]->parity == nka.parity) ? 0 : 2)]->subj_num[i];
  }
  
  nka.posC = charOffset(String(people_list_c), sm);
}
