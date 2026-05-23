#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cctype>

namespace {

enum class EResponseType {
    INVALID = 0,
    SUCCESS,
    ERROR,
    AUTH_MESSAGE,
};

enum class EAuthMessageType {
    INVALID = 0,
    VISIBLE,
    SECRET,
    INFO,
    ERROR,
};

struct SResponse {
    EResponseType    type        = EResponseType::INVALID;
    EAuthMessageType authType    = EAuthMessageType::INVALID;
    std::string      authMessage = "";
    std::string      errorType   = "";
    std::string      description = "";
};

std::string escapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const auto ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }

    return escaped;
}

std::string extractJsonString(const std::string& json, const std::string& key) {
    const auto keyPattern = std::format("\"{}\":", key);
    const auto keyPos     = json.find(keyPattern);
    if (keyPos == std::string::npos)
        return "";

    const auto colonPos = keyPos + keyPattern.size() - 1;
    const auto firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos)
        return "";

    std::string value;
    bool        escaped = false;
    for (size_t i = firstQuote + 1; i < json.size(); ++i) {
        const auto ch = json[i];
        if (escaped) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"')
            break;

        value.push_back(ch);
    }

    return value;
}

SResponse parseResponse(const std::string& json) {
    SResponse response;

    const auto type = extractJsonString(json, "type");
    if (type == "success")
        response.type = EResponseType::SUCCESS;
    else if (type == "error")
        response.type = EResponseType::ERROR;
    else if (type == "auth_message")
        response.type = EResponseType::AUTH_MESSAGE;
    else
        return response;

    response.errorType   = extractJsonString(json, "error_type");
    response.description = extractJsonString(json, "description");
    response.authMessage = extractJsonString(json, "auth_message");

    const auto authType = extractJsonString(json, "auth_message_type");
    if (authType == "visible")
        response.authType = EAuthMessageType::VISIBLE;
    else if (authType == "secret")
        response.authType = EAuthMessageType::SECRET;
    else if (authType == "info")
        response.authType = EAuthMessageType::INFO;
    else if (authType == "error")
        response.authType = EAuthMessageType::ERROR;

    return response;
}

static const char* responseTypeName(EResponseType type) {
    switch (type) {
        case EResponseType::SUCCESS: return "success";
        case EResponseType::ERROR: return "error";
        case EResponseType::AUTH_MESSAGE: return "auth_message";
        case EResponseType::INVALID:
        default: return "invalid";
    }
}

static const char* authMessageTypeName(EAuthMessageType type) {
    switch (type) {
        case EAuthMessageType::VISIBLE: return "visible";
        case EAuthMessageType::SECRET: return "secret";
        case EAuthMessageType::INFO: return "info";
        case EAuthMessageType::ERROR: return "error";
        case EAuthMessageType::INVALID:
        default: return "invalid";
    }
}

bool shouldRepromptUsername(const std::string& failText) {
    std::string text = failText;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text.contains("user unknown") || text.contains("unknown user") || text.contains("username") || text.contains("not exist");
}

bool isCooldownMessage(const std::string& failText) {
    std::string text = failText;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text.contains("left to unlock") || text.contains("locked") || text.contains("cooldown") || text.contains("wait");
}

std::string normalizeFailText(const std::string& failText) {
    if (isCooldownMessage(failText))
        return failText.empty() ? "Authentication cooldown active. Please wait before retrying." : failText;

    return "Invalid username or password. Try again.";
}

}

TEST(GreetdTest, EscapeJsonBackslash) {
    EXPECT_EQ(escapeJson("a\\b"), "a\\\\b");
}

TEST(GreetdTest, EscapeJsonQuote) {
    EXPECT_EQ(escapeJson("a\"b"), "a\\\"b");
}

TEST(GreetdTest, EscapeJsonNewline) {
    EXPECT_EQ(escapeJson("a\nb"), "a\\nb");
}

TEST(GreetdTest, EscapeJsonCarriageReturn) {
    EXPECT_EQ(escapeJson("a\rb"), "a\\rb");
}

TEST(GreetdTest, EscapeJsonTab) {
    EXPECT_EQ(escapeJson("a\tb"), "a\\tb");
}

TEST(GreetdTest, EscapeJsonAllSpecial) {
    EXPECT_EQ(escapeJson("\\\"\n\r\t"), "\\\\\\\"\\n\\r\\t");
}

TEST(GreetdTest, EscapeJsonPlainText) {
    EXPECT_EQ(escapeJson("hello world"), "hello world");
}

TEST(GreetdTest, EscapeJsonEmpty) {
    EXPECT_EQ(escapeJson(""), "");
}

TEST(GreetdTest, ExtractJsonStringSimple) {
    std::string json = R"({"type":"success"})";
    EXPECT_EQ(extractJsonString(json, "type"), "success");
}

TEST(GreetdTest, ExtractJsonStringMissingKey) {
    std::string json = R"({"type":"success"})";
    EXPECT_EQ(extractJsonString(json, "nonexistent"), "");
}

TEST(GreetdTest, ExtractJsonStringEmptyValue) {
    std::string json = R"({"type":""})";
    EXPECT_EQ(extractJsonString(json, "type"), "");
}

TEST(GreetdTest, ExtractJsonStringMultipleKeys) {
    std::string json = R"({"type":"auth_message","description":"failed"})";
    EXPECT_EQ(extractJsonString(json, "type"), "auth_message");
    EXPECT_EQ(extractJsonString(json, "description"), "failed");
}

TEST(GreetdTest, ExtractJsonStringEscapedChars) {
    std::string json = R"({"description":"line1\nline2"})";
    std::string result = extractJsonString(json, "description");
    EXPECT_EQ(result, "line1\nline2");
}

TEST(GreetdTest, ParseResponseSuccess) {
    std::string json = R"({"type":"success"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::SUCCESS);
}

TEST(GreetdTest, ParseResponseError) {
    std::string json = R"({"type":"error","error_type":"authentication_error","description":"bad password"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::ERROR);
    EXPECT_EQ(resp.errorType, "authentication_error");
    EXPECT_EQ(resp.description, "bad password");
}

TEST(GreetdTest, ParseResponseAuthMessageSecret) {
    std::string json = R"({"type":"auth_message","auth_message_type":"secret","auth_message":"Password:"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::AUTH_MESSAGE);
    EXPECT_EQ(resp.authType, EAuthMessageType::SECRET);
    EXPECT_EQ(resp.authMessage, "Password:");
}

TEST(GreetdTest, ParseResponseAuthMessageVisible) {
    std::string json = R"({"type":"auth_message","auth_message_type":"visible","auth_message":"Username:"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::AUTH_MESSAGE);
    EXPECT_EQ(resp.authType, EAuthMessageType::VISIBLE);
    EXPECT_EQ(resp.authMessage, "Username:");
}

TEST(GreetdTest, ParseResponseAuthMessageInfo) {
    std::string json = R"({"type":"auth_message","auth_message_type":"info","auth_message":"Logged in"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::AUTH_MESSAGE);
    EXPECT_EQ(resp.authType, EAuthMessageType::INFO);
}

TEST(GreetdTest, ParseResponseAuthMessageError) {
    std::string json = R"({"type":"auth_message","auth_message_type":"error","auth_message":"Auth failed"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::AUTH_MESSAGE);
    EXPECT_EQ(resp.authType, EAuthMessageType::ERROR);
    EXPECT_EQ(resp.authMessage, "Auth failed");
}

TEST(GreetdTest, ParseResponseUnknownType) {
    std::string json = R"({"type":"unknown"})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::INVALID);
}

TEST(GreetdTest, ParseResponseMalformed) {
    std::string json = R"(not json)";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::INVALID);
}

TEST(GreetdTest, ParseResponseEmptyObject) {
    std::string json = R"({})";
    auto resp = parseResponse(json);
    EXPECT_EQ(resp.type, EResponseType::INVALID);
}

TEST(GreetdTest, ResponseTypeNameSuccess) {
    EXPECT_STREQ(responseTypeName(EResponseType::SUCCESS), "success");
}

TEST(GreetdTest, ResponseTypeNameError) {
    EXPECT_STREQ(responseTypeName(EResponseType::ERROR), "error");
}

TEST(GreetdTest, ResponseTypeNameAuthMessage) {
    EXPECT_STREQ(responseTypeName(EResponseType::AUTH_MESSAGE), "auth_message");
}

TEST(GreetdTest, ResponseTypeNameInvalid) {
    EXPECT_STREQ(responseTypeName(EResponseType::INVALID), "invalid");
}

TEST(GreetdTest, AuthMessageTypeNameVisible) {
    EXPECT_STREQ(authMessageTypeName(EAuthMessageType::VISIBLE), "visible");
}

TEST(GreetdTest, AuthMessageTypeNameSecret) {
    EXPECT_STREQ(authMessageTypeName(EAuthMessageType::SECRET), "secret");
}

TEST(GreetdTest, AuthMessageTypeNameInfo) {
    EXPECT_STREQ(authMessageTypeName(EAuthMessageType::INFO), "info");
}

TEST(GreetdTest, AuthMessageTypeNameError) {
    EXPECT_STREQ(authMessageTypeName(EAuthMessageType::ERROR), "error");
}

TEST(GreetdTest, AuthMessageTypeNameInvalid) {
    EXPECT_STREQ(authMessageTypeName(EAuthMessageType::INVALID), "invalid");
}

TEST(GreetdTest, ShouldRepromptUsernameUserUnknown) {
    EXPECT_TRUE(shouldRepromptUsername("User unknown"));
    EXPECT_TRUE(shouldRepromptUsername("user unknown"));
    EXPECT_TRUE(shouldRepromptUsername("USER UNKNOWN"));
}

TEST(GreetdTest, ShouldRepromptUsernameUnknownUser) {
    EXPECT_TRUE(shouldRepromptUsername("Unknown user"));
}

TEST(GreetdTest, ShouldRepromptUsernameContainsUsername) {
    EXPECT_TRUE(shouldRepromptUsername("Invalid username"));
}

TEST(GreetdTest, ShouldRepromptUsernameNotExist) {
    EXPECT_TRUE(shouldRepromptUsername("User does not exist"));
}

TEST(GreetdTest, ShouldRepromptUsernameFalse) {
    EXPECT_FALSE(shouldRepromptUsername("Invalid password"));
    EXPECT_FALSE(shouldRepromptUsername("Authentication failed"));
    EXPECT_FALSE(shouldRepromptUsername(""));
}

TEST(GreetdTest, IsCooldownMessageLeftToUnlock) {
    EXPECT_TRUE(isCooldownMessage("5 minutes left to unlock"));
}

TEST(GreetdTest, IsCooldownMessageLocked) {
    EXPECT_TRUE(isCooldownMessage("Account locked"));
}

TEST(GreetdTest, IsCooldownMessageCooldown) {
    EXPECT_TRUE(isCooldownMessage("Cooldown active"));
}

TEST(GreetdTest, IsCooldownMessageWait) {
    EXPECT_TRUE(isCooldownMessage("Please wait"));
}

TEST(GreetdTest, IsCooldownMessageFalse) {
    EXPECT_FALSE(isCooldownMessage("Invalid password"));
    EXPECT_FALSE(isCooldownMessage(""));
}

TEST(GreetdTest, NormalizeFailTextCooldown) {
    std::string result = normalizeFailText("5 minutes left to unlock");
    EXPECT_EQ(result, "5 minutes left to unlock");
}

TEST(GreetdTest, NormalizeFailTextRegular) {
    std::string result = normalizeFailText("Invalid password");
    EXPECT_EQ(result, "Invalid username or password. Try again.");
}

TEST(GreetdTest, NormalizeFailTextEmptyNonCooldown) {
    std::string result = normalizeFailText("");
    EXPECT_EQ(result, "Invalid username or password. Try again.");
}

TEST(GreetdTest, NormalizeFailTextCooldownEmpty) {
    std::string result = normalizeFailText("locked");
    EXPECT_EQ(result, "locked");
}
