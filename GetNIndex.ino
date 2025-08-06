void getNIndex() {
  byte weeks_ago = 0, days_ago = 0;
  int diff = 0;
  
  if (week[nka.subgroup]->pon_month == nka.month) {
    if (nka.day == week[nka.subgroup]->pon_day) {
      weeks_ago = 0;
      days_ago = 0;
    }

    else if (nka.day > week[nka.subgroup]->pon_day) {
      weeks_ago = 0;
      days_ago = nka.day - week[nka.subgroup]->pon_day;
    }

    else if (nka.day < week[nka.subgroup]->pon_day) {
      diff = week[nka.subgroup]->pon_day - nka.day;
      weeks_ago = (diff + 6) / 7;
      if (diff % 7 != 0) days_ago = 7 - (diff % 7);
    }
  }

  else if (nka.month < week[nka.subgroup]->pon_month) {
    int d = 0;
    for (byte i = nka.month+1; i < week[nka.subgroup]->pon_month; i++) {
      d += day_month[i - 1];
    }
    diff = (week[nka.subgroup]->pon_day + day_month[nka.month-1]) - nka.day + d;
    weeks_ago = (diff + 6) / 7;
    if (diff % 7 != 0) days_ago = 7 - (diff % 7);
  }

  else if (nka.month > week[nka.subgroup]->pon_month) {
    weeks_ago = 0;
    days_ago = (day_month[week[nka.subgroup]->pon_month-1] + nka.day) - week[nka.subgroup]->pon_day;
  }

  byte k = 0;
  bool found = false;

  if (nka.surn == "")  {
    nka.posI = (people_list_i + (offset[nka.subgroup] * (file_data.week_off-1 - weeks_ago)));
    found = true;
  }

  else {
    for (int i = 0; i < sizeof(students)/sizeof(students[0]); i++) {
      if (students[i].surname == nka.surn)  {
        found = true;
        nka.posI = (people_list_i + (offset[nka.subgroup] * (file_data.week_off-1 - weeks_ago))) + k;
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
