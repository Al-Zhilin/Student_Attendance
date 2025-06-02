void commandList(int32_t reply_id) {
  String answer = "";
  answer += "Что я умею:\n";
  answer += "\"Бросить/Кинуть кубик\" - выдаю число [1; 6]\n";
  
  bot.replyMessage(answer, reply_id);
}