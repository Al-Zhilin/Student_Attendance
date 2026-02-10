void getNIndex() {
  byte weeks_ago = 0, days_ago = 0;

  int diff = StampUtils::dateToDays2000(realTime.day, realTime.month, realTime.year) - StampUtils::dateToDays2000(nka.date.day, nka.date.month, nka.date.year);           // разница через кол-во дней с 01.01.2000

  if (diff < 0) {     // нка ставится наперед, !на текущую неделю! (слишком наперед низя, таких недель банально нет в таблице еще)
    days_ago = nka.date.day - week[0]->pon_date.day;
  } else {            // в прошлое
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
  }

  if (weeks_ago % 2 == 0) nka.parity = week[0]->parity;
  else nka.parity = !week[0]->parity;
  nka.dayWeek = days_ago+1;

  uint16_t sm = 1;
  
  for (int i = 0; i < days_ago; i++) {
    sm += 1 + countLessonsInDay(week[week[0]->parity != nka.parity]->less_nums[0][i], week[week[0]->parity != nka.parity]->subj_num[0][i], week[week[0]->parity != nka.parity]->less_nums[1][i], week[week[0]->parity != nka.parity]->subj_num[1][i]);
  }   // P.S. алгоритм получил "магические числа" в процессе оптимизации, см. ранние коммиты (до февраля 2026), чтобы вникнуть в суть
  
  nka.posC = charOffset(String(people_list_c), sm);
}

uint16_t mergeArrays(const uint8_t *arr1, uint8_t n, const uint8_t *arr2, uint8_t m) {           // реализован алгоритм объединения (или подсчета) методом слияния
    size_t i = 0, j = 0;
    size_t count = 0;

    while (i < n && j < m) {
        if (arr1[i] < arr2[j]) {
            // Элемент в первом массиве меньше, берем его
            count++;
            i++;
        } else if (arr1[i] > arr2[j]) {
            // Элемент во втором массиве меньше, берем его
            count++;
            j++;
        } else {
            // Элементы равны. Учитываем только один раз и сдвигаем оба указателя
            count++;
            i++;
            j++;
        }
    }

    // Добавляем оставшиеся элементы, если один из массивов закончился раньше
    count += (n - i);
    count += (m - j);

    return count;
}
