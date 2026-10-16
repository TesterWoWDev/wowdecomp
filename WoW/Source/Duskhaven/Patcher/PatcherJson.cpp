#include <Patcher/PatcherJson.h>
#include <sstream>
#include <utility>

namespace Patcher::Json
{
    const Value* Value::Find(const std::string& key) const
    {
        const auto itr = object.find(key);
        return itr == object.end() ? nullptr : &itr->second;
    }

    namespace
    {
        class Parser
        {
          public:
            explicit Parser(const std::string& text) : m_text(text) {}

            bool Parse(Value& out)
            {
                SkipWhitespace();
                if (!ParseValue(out))
                    return false;

                SkipWhitespace();
                if (m_pos != m_text.size())
                    return Fail("unexpected trailing data");

                return true;
            }

            const std::string& Error() const
            {
                return m_error;
            }

          private:
            const std::string& m_text;
            size_t m_pos = 0;
            size_t m_depth = 0;
            size_t m_values = 0;
            std::string m_error;

            void SkipWhitespace()
            {
                while (m_pos < m_text.size())
                {
                    const unsigned char c = static_cast<unsigned char>(m_text[m_pos]);
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                        break;
                    ++m_pos;
                }
            }

            bool Fail(const char* message)
            {
                std::ostringstream stream;
                stream << message << " at byte " << m_pos;
                m_error = stream.str();
                return false;
            }

            bool ParseValue(Value& out)
            {
                // The release index/runtime manifest are shallow documents.
                // A byte limit alone does not bound recursive parser stack use.
                if (m_depth >= 64 || ++m_values > 100000)
                    return Fail("JSON structure exceeds parsing limits");
                struct DepthGuard
                {
                    size_t& depth;
                    explicit DepthGuard(size_t& value) : depth(value) { ++depth; }
                    ~DepthGuard() { --depth; }
                } depthGuard(m_depth);
                SkipWhitespace();
                if (m_pos >= m_text.size())
                    return Fail("expected value");

                const char c = m_text[m_pos];
                if (c == '{')
                    return ParseObject(out);
                if (c == '[')
                    return ParseArray(out);
                if (c == '"')
                {
                    out.type = Type::String;
                    return ParseString(out.string);
                }
                if (c == '-' || (c >= '0' && c <= '9'))
                {
                    out.type = Type::Number;
                    return ParseNumber(out.number);
                }
                if (m_text.compare(m_pos, 4, "true") == 0)
                {
                    m_pos += 4;
                    out.type    = Type::Boolean;
                    out.boolean = true;
                    return true;
                }
                if (m_text.compare(m_pos, 5, "false") == 0)
                {
                    m_pos += 5;
                    out.type    = Type::Boolean;
                    out.boolean = false;
                    return true;
                }
                if (m_text.compare(m_pos, 4, "null") == 0)
                {
                    m_pos += 4;
                    out.type = Type::Null;
                    return true;
                }

                return Fail("unexpected token");
            }

            static int HexValue(char c)
            {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            }

            bool ParseString(std::string& out)
            {
                if (m_pos >= m_text.size() || m_text[m_pos] != '"')
                    return Fail("expected string");
                ++m_pos;
                out.clear();

                while (m_pos < m_text.size())
                {
                    const char c = m_text[m_pos++];
                    if (c == '"')
                        return true;

                    if (static_cast<unsigned char>(c) < 0x20)
                        return Fail("control character in string");

                    if (c != '\\')
                    {
                        out.push_back(c);
                        continue;
                    }

                    if (m_pos >= m_text.size())
                        return Fail("unterminated escape sequence");

                    const char escaped = m_text[m_pos++];
                    switch (escaped)
                    {
                        case '"':
                        case '\\':
                        case '/':
                            out.push_back(escaped);
                            break;
                        case 'b':
                            out.push_back('\b');
                            break;
                        case 'f':
                            out.push_back('\f');
                            break;
                        case 'n':
                            out.push_back('\n');
                            break;
                        case 'r':
                            out.push_back('\r');
                            break;
                        case 't':
                            out.push_back('\t');
                            break;
                        case 'u':
                        {
                            if (m_pos + 4 > m_text.size())
                                return Fail("truncated unicode escape");
                            unsigned code = 0;
                            for (int i = 0; i < 4; ++i)
                            {
                                const int value = HexValue(m_text[m_pos++]);
                                if (value < 0)
                                    return Fail("invalid unicode escape");
                                code = (code << 4) | static_cast<unsigned>(value);
                            }
                            out.push_back(code < 0x80 ? static_cast<char>(code) : '?');
                            break;
                        }
                        default:
                            return Fail("invalid escape sequence");
                    }
                }

                return Fail("unterminated string");
            }

            bool ParseNumber(std::string& out)
            {
                const size_t start = m_pos;
                if (m_text[m_pos] == '-')
                    ++m_pos;

                if (m_pos >= m_text.size())
                    return Fail("invalid number");

                if (m_text[m_pos] == '0')
                {
                    ++m_pos;
                }
                else if (m_text[m_pos] >= '1' && m_text[m_pos] <= '9')
                {
                    while (m_pos < m_text.size() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9')
                        ++m_pos;
                }
                else
                {
                    return Fail("invalid number");
                }

                if (m_pos < m_text.size() && m_text[m_pos] == '.')
                {
                    ++m_pos;
                    if (m_pos >= m_text.size() || m_text[m_pos] < '0' || m_text[m_pos] > '9')
                        return Fail("invalid number fraction");
                    while (m_pos < m_text.size() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9')
                        ++m_pos;
                }

                if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
                {
                    ++m_pos;
                    if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-'))
                        ++m_pos;
                    if (m_pos >= m_text.size() || m_text[m_pos] < '0' || m_text[m_pos] > '9')
                        return Fail("invalid number exponent");
                    while (m_pos < m_text.size() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9')
                        ++m_pos;
                }

                out = m_text.substr(start, m_pos - start);
                return true;
            }

            bool ParseArray(Value& out)
            {
                out.type = Type::Array;
                out.array.clear();
                ++m_pos;

                SkipWhitespace();
                if (m_pos < m_text.size() && m_text[m_pos] == ']')
                {
                    ++m_pos;
                    return true;
                }

                while (true)
                {
                    Value value;
                    if (!ParseValue(value))
                        return false;
                    out.array.push_back(std::move(value));

                    SkipWhitespace();
                    if (m_pos >= m_text.size())
                        return Fail("unterminated array");
                    if (m_text[m_pos] == ']')
                    {
                        ++m_pos;
                        return true;
                    }
                    if (m_text[m_pos] != ',')
                        return Fail("expected array comma");
                    ++m_pos;
                }
            }

            bool ParseObject(Value& out)
            {
                out.type = Type::Object;
                out.object.clear();
                ++m_pos;

                SkipWhitespace();
                if (m_pos < m_text.size() && m_text[m_pos] == '}')
                {
                    ++m_pos;
                    return true;
                }

                while (true)
                {
                    SkipWhitespace();
                    std::string key;
                    if (!ParseString(key))
                        return false;

                    SkipWhitespace();
                    if (m_pos >= m_text.size() || m_text[m_pos] != ':')
                        return Fail("expected object colon");
                    ++m_pos;

                    Value value;
                    if (!ParseValue(value))
                        return false;
                    if (!out.object.emplace(key, std::move(value)).second)
                        return Fail("duplicate object key");

                    SkipWhitespace();
                    if (m_pos >= m_text.size())
                        return Fail("unterminated object");
                    if (m_text[m_pos] == '}')
                    {
                        ++m_pos;
                        return true;
                    }
                    if (m_text[m_pos] != ',')
                        return Fail("expected object comma");
                    ++m_pos;
                }
            }
        };
    } // namespace

    bool Parse(const std::string& text, Value& out, std::string& error)
    {
        Parser parser(text);
        if (parser.Parse(out))
            return true;

        error = parser.Error();
        return false;
    }
} // namespace Patcher::Json
