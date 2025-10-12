void commandList(int32_t reply_id) {
  String answer = "";
  answer += "/список - показывает общий список группы\n";
  answer += "/список1 - показыват список 1 подгруппы\n";
  answer += "/список2 - показывает список 2 подгруппы";
  bot.replyMessage(answer, reply_id);
}