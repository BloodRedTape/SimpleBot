#pragma once

#include <functional>
#include <vector>
#include <unordered_map>
#include <bsl/format.hpp>
#include <tgbot/Bot.h>
#include <tgbot/net/TgLongPoll.h>

#undef SendMessage

struct KeyboardButton {
    std::string Text;
    std::string CallbackData;
    bool Enabled = true;

    KeyboardButton(std::string text, std::string callback_data, bool enabled = true) :
        Text(std::move(text)),
        CallbackData(std::move(callback_data)),
        Enabled(enabled)
    {}

    KeyboardButton(std::string text, bool enabled = true) :
        Text(text),
        CallbackData(text),
        Enabled(enabled)
    {}
};

using KeyboardLayout = std::vector<std::vector<KeyboardButton>>;

namespace Keyboard {
    KeyboardLayout ToKeyboard(const std::vector<std::string> &texts);

    KeyboardLayout ToNiceKeyboard(const std::vector<std::string> &texts, std::size_t row_size, std::function<std::string(std::string)> make_key = [](auto s){return s;});

    std::vector<KeyboardButton> ToKeyboardRow(const std::vector<std::string> &texts);
}

class SimpleBot: public TgBot::Bot{
    static constexpr const char *ParseMode = "HTML";
    static constexpr bool DisableWebpagePreview = true;
public:
    using LogHandler = std::function<void(const std::string&)>;

    using CommandHandler = std::function<void(TgBot::Message::Ptr)>;
    using MessageHandler = std::function<void(TgBot::Message::Ptr)>;
    using CallbackQueryHandler = std::function<void(TgBot::CallbackQuery::Ptr)>;
    using ChatMemberStatusHandler = std::function<void(TgBot::ChatMemberUpdated::Ptr)>;
private:
    LogHandler m_Log;

    std::unordered_map<std::string, CommandHandler> m_CommandHandlers;
    std::unordered_map<std::string, std::string> m_CommandDescriptions;

    std::string m_Username;
public:
    SimpleBot(const std::string &token);

    void LongPoll();

    void OnLog(LogHandler handler);

    template<typename Type>
    void OnLog(Type *object, void (Type::*handler)(const std::string &));

    void Log(const std::string &message);
    
    template<typename ...ArgsType>
	void Log(const char* fmt, const ArgsType&...args);

    void ClearOldUpdates();

    TgBot::Message::Ptr SendMessage(std::int64_t chat, std::int32_t topic, const std::string& message, std::int64_t reply_message = 0);

    TgBot::Message::Ptr SendMessage(TgBot::Message::Ptr source, const std::string& message, bool reply = false){return SendMessage(source->chat->id, source->isTopicMessage ? source->messageThreadId : 0, message, reply ? source->messageId : 0); }

    TgBot::Message::Ptr ReplyMessage(TgBot::Message::Ptr source, const std::string& message){return SendMessage(source, message, true); }

    //source->isTopicMessage ? source->messageThreadId : 0
    TgBot::Message::Ptr SendMessage(std::int64_t chat, std::int32_t topic, const std::string& message, TgBot::GenericReply::Ptr reply, std::int64_t reply_message = 0);

    TgBot::Message::Ptr SendKeyboard(std::int64_t chat, std::int32_t topic, const std::string& message, const KeyboardLayout& keyboard, std::int64_t reply_message = 0);

    TgBot::Message::Ptr SendKeyboard(TgBot::Message::Ptr source, const std::string& message, const KeyboardLayout &keyboard, bool reply = false){return SendKeyboard(source->chat->id, source->isTopicMessage ? source->messageThreadId : 0, message, keyboard, reply ? source->messageId : 0); }

    TgBot::Message::Ptr ReplyKeyboard(TgBot::Message::Ptr source, const std::string& message, const KeyboardLayout &keyboard){return SendKeyboard(source, message, keyboard, true);}

    TgBot::Message::Ptr SendPhoto(std::int64_t chat, std::int32_t topic, const std::string& text, TgBot::InputFile::Ptr photo, std::int64_t reply_message = 0);

    TgBot::Message::Ptr SendPhoto(TgBot::Message::Ptr source, const std::string& text, TgBot::InputFile::Ptr photo);

    TgBot::Message::Ptr ReplyPhoto(TgBot::Message::Ptr source, const std::string& text, TgBot::InputFile::Ptr photo);

    TgBot::Message::Ptr EditMessage(TgBot::Message::Ptr message, const std::string& text, TgBot::InlineKeyboardMarkup::Ptr reply);

    TgBot::Message::Ptr EditMessage(TgBot::Message::Ptr message, const std::string& text, const KeyboardLayout& keyboard);

    TgBot::Message::Ptr EditMessage(TgBot::Message::Ptr message, const KeyboardLayout& keyboard);

    TgBot::Message::Ptr EditMessage(TgBot::Message::Ptr message, const std::string& text);

    void DeleteMessage(TgBot::Message::Ptr message);

    void RemoveKeyboard(TgBot::Message::Ptr message);

    TgBot::Message::Ptr EnsureMessage(TgBot::Message::Ptr ensurable, std::int64_t chat, std::int32_t topic, const std::string &message, TgBot::InlineKeyboardMarkup::Ptr reply = nullptr);

    TgBot::Message::Ptr EnsureKeyboard(TgBot::Message::Ptr ensurable, std::int64_t chat, std::int32_t topic, const std::string &message, const KeyboardLayout &keyboard);

    bool AnswerCallbackQuery(const std::string& callbackQueryId, const std::string& text = "");

    void OnCommand(const std::string &command, CommandHandler handler, std::string &&description = "");
    
    template<typename Type>
    void OnCommand(const std::string &command, Type *object, void (Type::*handler)(TgBot::Message::Ptr), std::string &&description = "");

    void BroadcastCommand(const std::string &command, TgBot::Message::Ptr message);

    void OnNonCommandMessage(MessageHandler message);

    template<typename Type>
    void OnNonCommandMessage(Type *object, void (Type::*handler)(TgBot::Message::Ptr));

    void OnCallbackQuery(CallbackQueryHandler handler);

    template<typename Type>
    void OnCallbackQuery(Type *object, void (Type::*handler)(TgBot::CallbackQuery::Ptr));

    void OnMyChatMember(ChatMemberStatusHandler chat_member);

    template<typename Type>
    void OnMyChatMember(Type *object, void (Type::*handler)(TgBot::ChatMemberUpdated::Ptr));

    void UpdateCommandDescriptions();

    std::string ParseCommand(TgBot::Message::Ptr message);

    static std::size_t GetCommandLength(TgBot::Message::Ptr message);

    static std::string GetTextWithoutCommand(TgBot::Message::Ptr message);
};

template<typename Type>
void SimpleBot::OnLog(Type* object, void (Type::* handler)(const std::string&)) {
    OnLog(std::bind(handler, object, std::placeholders::_1));
}

template<typename ...ArgsType>
void SimpleBot::Log(const char* fmt, const ArgsType&...args) {
	Log(Format(fmt, args...));
}

template<typename Type>
void SimpleBot::OnCommand(const std::string& command, Type *object, void (Type::* handler)(TgBot::Message::Ptr), std::string &&description) {
    OnCommand(command, std::bind(handler, object, std::placeholders::_1), std::move(description));
}

template<typename Type>
void SimpleBot::OnNonCommandMessage(Type* object, void (Type::* handler)(TgBot::Message::Ptr)) {
    OnNonCommandMessage(std::bind(handler, object, std::placeholders::_1));
}

template<typename Type>
void SimpleBot::OnCallbackQuery(Type* object, void (Type::* handler)(TgBot::CallbackQuery::Ptr)) {
    OnCallbackQuery(std::bind(handler, object, std::placeholders::_1));
}

template<typename Type>
void SimpleBot::OnMyChatMember(Type* object, void (Type::* handler)(TgBot::ChatMemberUpdated::Ptr)) {
    OnMyChatMember(std::bind(handler, object, std::placeholders::_1));
}

class SimplePollBot: public SimpleBot{
    TgBot::TgLongPoll m_Poll;
public:
    SimplePollBot(const std::string &token, std::int32_t limit = 100, std::int32_t timeout = 10);
    
    void LongPollIteration();
};


class FastLongPoll {
public:
    FastLongPoll(const TgBot::Api* api, const TgBot::EventHandler* eventHandler, std::int32_t limit, std::int32_t timeout, std::shared_ptr<std::vector<std::string>> allowUpdates);
    FastLongPoll(const TgBot::Bot& bot, std::int32_t limit = 100, std::int32_t timeout = 10, const std::shared_ptr<std::vector<std::string>>& allowUpdates = nullptr);

    void start();

private:

    void handleUpdates();

private:
    const TgBot::Api* _api;
    const TgBot::EventHandler* _eventHandler;
    std::int32_t _lastUpdateId = 0;
    std::int32_t _limit;
    std::int32_t _timeout;
    std::shared_ptr<std::vector<std::string>> _allowUpdates;

    std::vector<TgBot::Update::Ptr> _updates;
};

