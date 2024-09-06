#include "simple_bot/simple_bot.hpp"
#include <tgbot/net/TgLongPoll.h>
#include <boost/range/algorithm.hpp>
#include <boost/algorithm/string.hpp>

KeyboardLayout Keyboard::ToKeyboard(const std::vector<std::string>& texts) {
    return {ToKeyboardRow(texts)};
}

KeyboardLayout Keyboard::ToNiceKeyboard(const std::vector<std::string>& texts, std::size_t row_size, std::function<std::string(std::string)> make_key)
{
    KeyboardLayout layout;
    std::vector<KeyboardButton> row;
    
    std::size_t i = 0;
    for (const auto& text : texts) {
        row.emplace_back(text, make_key(text));
        i++;

        if (i == row_size) {
            i = 0;
            layout.push_back(std::move(row));
        }
    }
    
    if(row.size())
        layout.push_back(std::move(row));

    return layout;
}

std::vector<KeyboardButton> Keyboard::ToKeyboardRow(const std::vector<std::string>& texts) {
    std::vector<KeyboardButton> row;

    for (const auto& text : texts) {
        row.emplace_back(text);
    }
    
    return row;
}

static TgBot::InlineKeyboardMarkup::Ptr ToInlineKeyboardMarkup(const KeyboardLayout &keyboard) {
    if(!keyboard.size())
        return nullptr;

    TgBot::InlineKeyboardMarkup::Ptr keyboard_markup(new TgBot::InlineKeyboardMarkup);

    for (const auto& row : keyboard) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> row_markup;

        for (const auto& button : row) {
            if(!button.Enabled)
                continue;

            TgBot::InlineKeyboardButton::Ptr button_markup(new TgBot::InlineKeyboardButton);
            button_markup->text = button.Text;
            button_markup->callbackData = button.CallbackData;

            row_markup.push_back(button_markup);
        }
        
        if(row_markup.size())
            keyboard_markup->inlineKeyboard.push_back(row_markup);
    }
    
    return keyboard_markup;
}

SimpleBot::SimpleBot(const std::string& token):
	TgBot::Bot(token)
{
    getEvents().onUnknownCommand([this](TgBot::Message::Ptr message) {
        std::string command = ParseCommand(message);

        if (!command.size()) {
            return;
        }
        
        try{
            BroadcastCommand(command, message);
        } catch (const std::exception& e) {
            Log("Caught exception on '%' command broadcast: %", command, e.what());
        }
    });

    try{
        m_Username = getApi().getMe()->username;
    } catch (const std::exception& e) {
        Log("Failed to get bot identity: %", e.what());
    }
}

void SimpleBot::LongPoll(){
	TgBot::TgLongPoll long_poll(*this);

    while(true){
        try{
            long_poll.start();
        } catch (const std::exception& e) {
			Log("LongPoolException: %", e.what());
        }
    }
}

void SimpleBot::OnLog(LogHandler handler){
	m_Log = handler;
}

void SimpleBot::Log(const std::string& message){
    if(!m_Log)
        return;

	m_Log(message);
}

void SimpleBot::ClearOldUpdates(){
    try{
        getApi().getUpdates(-1, 1);
    } catch (const std::exception& e) {
        Log("Failed to clear old updates: %", e.what());
    }
}

TgBot::Message::Ptr SimpleBot::SendMessage(std::int64_t chat, std::int32_t topic, const std::string& message, std::int64_t reply_message) {
    return SendMessage(chat, topic, message, nullptr, reply_message);
}

TgBot::Message::Ptr SimpleBot::SendMessage(std::int64_t chat, std::int32_t topic, const std::string& message, TgBot::GenericReply::Ptr reply, std::int64_t reply_message) {
    if (!message.size()) {
        Log("Can't send empty messages");
        return nullptr;
    }

    TgBot::Message::Ptr result = nullptr;

    try {
        TgBot::LinkPreviewOptions::Ptr link_preview(new TgBot::LinkPreviewOptions());
        link_preview->isDisabled = DisableWebpagePreview;

        TgBot::ReplyParameters::Ptr reply_params(new TgBot::ReplyParameters());
        reply_params->chatId = chat;
        reply_params->messageId = reply_message;

        result = getApi().sendMessage(chat, message, link_preview, reply_params, reply, ParseMode, false, {}, topic);
    }
    catch (const std::exception& exception) {
        auto chat_ptr = getApi().getChat(chat);
        
        std::string chat_name = chat_ptr->username.size() ? chat_ptr->username : chat_ptr->title;

        Log("Failed to send message to chat '%' id % reason %", chat_name, chat, exception.what());
    }

    return result;
}

TgBot::Message::Ptr SimpleBot::SendMessage(TgBot::Message::Ptr source, const std::string& message, bool reply = false){ 
    if(!source)
        return nullptr; 
        
    return SendMessage(source->chat->id, source->isTopicMessage ? source->messageThreadId : 0, message, reply ? source->messageId : 0); 
}

TgBot::Message::Ptr SimpleBot::EditMessage(TgBot::Message::Ptr message, const std::string& text, const KeyboardLayout& keyboard) {
    return EditMessage(message, text, ToInlineKeyboardMarkup(keyboard));
}

TgBot::Message::Ptr SimpleBot::SendKeyboard(std::int64_t chat, std::int32_t topic, const std::string& message, const KeyboardLayout& keyboard, std::int64_t reply_message) {
    return SendMessage(chat, topic, message, ToInlineKeyboardMarkup(keyboard), reply_message);
}

TgBot::Message::Ptr SimpleBot::SendPhoto(std::int64_t chat, std::int32_t topic, const std::string& text, TgBot::InputFile::Ptr photo, std::int64_t reply_message){
    try {
        TgBot::ReplyParameters::Ptr reply_params(new TgBot::ReplyParameters());
        reply_params->chatId = chat;
        reply_params->messageId = reply_message;
	    return getApi().sendPhoto(chat, photo, text, reply_params, nullptr, ParseMode, false, {}, false, false, topic);
    }catch (const std::exception& exception) {
        auto chat_ptr = getApi().getChat(chat);
        std::string chat_name = chat_ptr->username.size() ? chat_ptr->username : chat_ptr->title;

        Log("Failed to send photo in chat '%' id % reason %", chat_name, chat_ptr->id, exception.what());
    }
    return nullptr;
}

TgBot::Message::Ptr SimpleBot::SendPhoto(TgBot::Message::Ptr source, const std::string& text, TgBot::InputFile::Ptr photo){
    return SendPhoto(source->chat->id, source->isTopicMessage ? source->messageThreadId : 0, text, photo);
}

TgBot::Message::Ptr SimpleBot::ReplyPhoto(TgBot::Message::Ptr source, const std::string& text, TgBot::InputFile::Ptr photo){
    return SendPhoto(source->chat->id, source->isTopicMessage ? source->messageThreadId : 0, text, photo, source->messageId);
}

TgBot::Message::Ptr SimpleBot::EditMessage(TgBot::Message::Ptr message, const std::string& text, TgBot::InlineKeyboardMarkup::Ptr reply) {
    try{
        if (text.size() && message->text != text) {
            TgBot::LinkPreviewOptions::Ptr link_preview(new TgBot::LinkPreviewOptions());
            link_preview->isDisabled = DisableWebpagePreview;
            return getApi().editMessageText(text, message->chat->id, message->messageId, "", ParseMode, link_preview, reply);
        } else {
            return getApi().editMessageReplyMarkup(message->chat->id, message->messageId, "", reply);
        }
    }
    catch (const std::exception& exception) {
        auto chat_ptr = message->chat;
        std::string chat_name = chat_ptr->username.size() ? chat_ptr->username : chat_ptr->title;

        Log("Failed to edit message in chat '%' id % reason %", chat_name, chat_ptr->id, exception.what());
    }
    return nullptr;
}

TgBot::Message::Ptr SimpleBot::EditMessage(TgBot::Message::Ptr message, const KeyboardLayout& keyboard) {
    return EditMessage(message, "", ToInlineKeyboardMarkup(keyboard));
}

TgBot::Message::Ptr SimpleBot::EditMessage(TgBot::Message::Ptr message, const std::string& text) {
    return EditMessage(message, text, nullptr);
}

bool SimpleBot::AnswerCallbackQuery(const std::string& callbackQueryId, const std::string& text) {
    try{
        return getApi().answerCallbackQuery(callbackQueryId, text);
    }
    catch (const std::exception& exception) {
        Log("Failed to answer callback query %", callbackQueryId);
    }
    return false;
}

void SimpleBot::DeleteMessage(TgBot::Message::Ptr message) {
    if (!message)
        return;

    assert(message->chat);

    try{
        getApi().deleteMessage(message->chat->id, message->messageId);
    }
    catch (const std::exception& exception) {
        auto chat = message->chat;

        std::string chat_name = chat->username.size() ? chat->username : chat->title;

        Log("Failed to delete message % from chat %, id %, reason: %", message->messageId, chat_name, chat->id, exception.what());
    }
}

void SimpleBot::RemoveKeyboard(TgBot::Message::Ptr message)
{
    if(!message)
        return;

    if(message->replyMarkup)
        EditMessage(message, message->text);
}

TgBot::Message::Ptr SimpleBot::EnsureMessage(TgBot::Message::Ptr ensurable, std::int64_t chat, std::int32_t topic, const std::string& message, TgBot::InlineKeyboardMarkup::Ptr reply)
{
    if(!ensurable)
        return SendMessage(chat, topic, message, reply);
    else
        return EditMessage(ensurable, message, reply);
}

TgBot::Message::Ptr SimpleBot::EnsureKeyboard(TgBot::Message::Ptr ensurable, std::int64_t chat, std::int32_t topic, const std::string& message, const KeyboardLayout& keyboard)
{
    return EnsureMessage(ensurable, chat, topic, message, ToInlineKeyboardMarkup(keyboard));
}

void SimpleBot::OnCommand(const std::string& command, CommandHandler handler, std::string &&description){
    m_CommandHandlers[command] = std::move(handler);
    m_CommandDescriptions[command] = std::move(description);
}

void SimpleBot::BroadcastCommand(const std::string& command, TgBot::Message::Ptr message){
    auto it = m_CommandHandlers.find(command);

    if(it == m_CommandHandlers.end())
        return;

    const auto &handler = it->second;

    handler(message);
}

void SimpleBot::OnNonCommandMessage(MessageHandler handler){
    getEvents().onNonCommandMessage(handler);
}

void SimpleBot::OnCallbackQuery(CallbackQueryHandler handler){
    getEvents().onCallbackQuery(handler);
}

void SimpleBot::OnMyChatMember(ChatMemberStatusHandler chat_member){
    getEvents().onMyChatMember(chat_member);
}

void SimpleBot::UpdateCommandDescriptions() {
    std::vector<TgBot::BotCommand::Ptr> commands;
    for (const auto& [command, descr] : m_CommandDescriptions) {
        if(!descr.size())
            continue;

        auto bot_command = std::make_shared<TgBot::BotCommand>();
        bot_command->command = command;
        bot_command->description = descr;
        commands.push_back(bot_command);
    }
    
    try{
        getApi().setMyCommands(commands);
    } catch (const std::exception& e) {
        Log("Failed to set bot commands: %", e.what());
    }
}

std::string SimpleBot::ParseCommand(TgBot::Message::Ptr message){
    auto length = GetCommandLength(message);

    if(!length)
        return {};

    const char Space = ' ';
    const char At = '@';

    std::string command_name = message->text.substr(0, length);

    std::size_t at = command_name.find(At);
    std::size_t end = std::min(at, command_name.size());

    std::string command = command_name.substr(1, end - 1);

    if (at != std::string::npos) {
        std::string command_bot_name = command_name.substr(at + 1);
        
        if(command_bot_name != m_Username)
            return {};
    }

    if (boost::range::count(command, ' '))
        return {};
    
    return command;
}

std::size_t SimpleBot::GetCommandLength(TgBot::Message::Ptr message){
	if(!message->text.size() || message->text.front() != '/')
		return 0;

	for (auto i = 0; i < message->text.size(); i++) {
		char c = message->text[i];
		if (std::isalpha(c) || std::isdigit(c) || std::ispunct(c)){
            if(i == message->text.size() - 1)
                return message->text.size();
			continue;
        }

		return i;
	}

    return 0;
}

std::string SimpleBot::GetTextWithoutCommand(TgBot::Message::Ptr message) {
    auto length = GetCommandLength(message);

    if(!length)
        return {};

	return message->text.substr(length);
}


SimplePollBot::SimplePollBot(const std::string &token, std::int32_t limit, std::int32_t timeout):
    SimpleBot(token),
    m_Poll(*this, limit, timeout)
{}

void SimplePollBot::LongPollIteration() {
    try{
        m_Poll.start();
    } catch (const std::exception& e) {
		Log("LongPoolException: %", e.what());
    }
}


FastLongPoll::FastLongPoll(const TgBot::Api* api, const TgBot::EventHandler* eventHandler, std::int32_t limit, std::int32_t timeout, std::shared_ptr<std::vector<std::string>> allowUpdates)
    : _api(api), _eventHandler(eventHandler), _limit(limit), _timeout(timeout)
    , _allowUpdates(std::move(allowUpdates)) {

    const_cast<TgBot::HttpClient&>(_api->_httpClient)._timeout = _timeout + 5;

    for (TgBot::Update::Ptr& item : _api->getUpdates(-1, 1)) {
        if (item->updateId >= _lastUpdateId) {
            _lastUpdateId = item->updateId + 1;
        }
    }
}

FastLongPoll::FastLongPoll(const TgBot::Bot& bot, std::int32_t limit, std::int32_t timeout, const std::shared_ptr<std::vector<std::string>>& allowUpdates)
    : FastLongPoll(&bot.getApi(), &bot.getEventHandler(), limit, timeout, allowUpdates) {
}

void FastLongPoll::start() {
    _updates = _api->getUpdates(_lastUpdateId, _limit, _timeout, _allowUpdates);

    handleUpdates();
}

void FastLongPoll::handleUpdates()
{
    for (TgBot::Update::Ptr& item : _updates) {
        if (item->updateId >= _lastUpdateId) {
            _lastUpdateId = item->updateId + 1;
        }
        _eventHandler->handleUpdate(item);
    }
}


