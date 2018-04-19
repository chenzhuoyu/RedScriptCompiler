#include <unordered_map>
#include "compiler/Tokenizer.h"

namespace RedScript::Compiler
{
static const std::unordered_map<std::string, Token::Keyword> Keywords = {
    { "if"      , Token::Keyword::If        },
    { "else"    , Token::Keyword::Else      },
    { "for"     , Token::Keyword::For       },
    { "while"   , Token::Keyword::While     },
    { "switch"  , Token::Keyword::Switch    },
    { "case"    , Token::Keyword::Case      },
    { "default" , Token::Keyword::Default   },

    { "break"   , Token::Keyword::Break     },
    { "continue", Token::Keyword::Continue  },
    { "return"  , Token::Keyword::Return    },

    { "try"     , Token::Keyword::Try       },
    { "except"  , Token::Keyword::Except    },
    { "finally" , Token::Keyword::Finally   },
    { "raise"   , Token::Keyword::Raise     },

    { "class"   , Token::Keyword::Class     },
    { "native"  , Token::Keyword::Native    },
    { "def"     , Token::Keyword::Function  },

    { "as"      , Token::Keyword::As        },
    { "from"    , Token::Keyword::From      },
    { "delete"  , Token::Keyword::Delete    },
    { "import"  , Token::Keyword::Import    },
};

static const std::unordered_map<std::string, Token::Operator> Operators = {
    { "("   , Token::Operator::BracketLeft          },
    { ")"   , Token::Operator::BracketRight         },
    { "["   , Token::Operator::IndexLeft            },
    { "]"   , Token::Operator::IndexRight           },
    { "{"   , Token::Operator::BlockLeft            },
    { "}"   , Token::Operator::BlockRight           },

    { ","   , Token::Operator::Comma                },
    { "."   , Token::Operator::Point                },
    { ":"   , Token::Operator::Colon                },
    { ";"   , Token::Operator::Semicolon            },
    { "\n"  , Token::Operator::NewLine              },

    { "<"   , Token::Operator::Less                 },
    { ">"   , Token::Operator::Greater              },
    { "<="  , Token::Operator::Leq                  },
    { ">="  , Token::Operator::Geq                  },
    { "=="  , Token::Operator::Equ                  },
    { "!="  , Token::Operator::Neq                  },

    { "and" , Token::Operator::BoolAnd              },
    { "or"  , Token::Operator::BoolOr               },
    { "not" , Token::Operator::BoolNot              },

    { "+"   , Token::Operator::Plus                 },
    { "-"   , Token::Operator::Minus                },
    { "/"   , Token::Operator::Divide               },
    { "*"   , Token::Operator::Multiply             },
    { "%"   , Token::Operator::Module               },
    { "**"  , Token::Operator::Power                },

    { "&"   , Token::Operator::BitAnd               },
    { "|"   , Token::Operator::BitOr                },
    { "~"   , Token::Operator::BitNot               },
    { "^"   , Token::Operator::BitXor               },
    { "<<"  , Token::Operator::ShiftLeft            },
    { ">>"  , Token::Operator::ShiftRight           },

    { "+="  , Token::Operator::InplaceAdd           },
    { "-="  , Token::Operator::InplaceSub           },
    { "*="  , Token::Operator::InplaceMul           },
    { "/="  , Token::Operator::InplaceDiv           },
    { "%="  , Token::Operator::InplaceMod           },
    { "**=" , Token::Operator::InplacePower         },

    { "&="  , Token::Operator::InplaceBitAnd        },
    { "|="  , Token::Operator::InplaceBitOr         },
    { "^="  , Token::Operator::InplaceBitXor        },
    { "<<=" , Token::Operator::InplaceShiftLeft     },
    { ">>=" , Token::Operator::InplaceShiftRight    },

    { "in"  , Token::Operator::In                   },
    { "="   , Token::Operator::Assign               },
    { "->"  , Token::Operator::Lambda               },
    { "@"   , Token::Operator::Decorator            },
};

template <typename T> static inline bool in(T c, T a, T b)  { return c >= a && c <= b; }
template <typename T> static inline bool isHex(T c)         { return in(c, '0', '9') || in(c, 'a', 'f') || in(c, 'A', 'F'); }
template <typename T> static inline long toInt(T c)         { return in(c, '0', '9') ? (c - '0') : in(c, 'a', 'f') ? (c - 'a' + 10) : (c - 'A' + 10); }

/****** Tokenizer ******/

Tokenizer::Tokenizer(const std::string &source) : _source(source)
{
    /* initial state */
    _stack.push_back(State {
        /* row   */ 1,
        /* col   */ 1,
        /* pos   */ 0,
        /* cache */ std::deque<Token::Ptr>(),
    });

    /* fast reference */
    _state = &(_stack.back());
}

char Tokenizer::peekChar(bool isRaw)
{
    /* check for overflow */
    if (_state->pos >= _source.size())
        return 0;

    /* peek next char */
    char result = _source[_state->pos];
    return !isRaw && (result == '\r') ? (char)'\n' : result;
}

char Tokenizer::nextChar(bool isRaw)
{
    /* check for overflow */
    if (_state->pos >= _source.size())
        return 0;

    /* peek next char */
    char result = _source[_state->pos];

    /* convert the line ending if not in raw mode */
    if (!isRaw && (result == '\r' || result == '\n'))
    {
        /* move to next line */
        _state->row++;
        _state->col = 0;

        /* '\r\n' or '\n\r' */
        if (_state->pos < _source.size() && _source[_state->pos + 1] == (result == '\n' ? '\r' : '\n'))
            _state->pos++;

        /* always convert to UNIX line ending */
        result = '\n';
    }

    _state->col++;
    _state->pos++;
    return result;
}

void Tokenizer::skipSpaces(void)
{
    char ch = peekChar();
    while (isspace(ch) && ch != '\n')
    {
        nextChar();
        ch = peekChar();
    }
}

void Tokenizer::skipComments(void)
{
    while (peekChar() == '#')
    {
        char ch;
        nextChar();
        while ((ch = peekChar()) && (ch != '\n')) nextChar();
        while ((ch = peekChar()) && (ch == '\n')) nextChar();
        skipSpaces();
    }
}

Token::Ptr Tokenizer::read(void)
{
    /* skip spaces and comments */
    skipSpaces();
    skipComments();

    /* read first char */
    switch (peekChar())
    {
        /* '\0' means EOF */
        case 0:
            return Token::createEof(_state->row, _state->col);

        /* strings can either be single or double quoted */
        case '\'':
        case '\"':
            return readString();

        /* number constants */
        case '0' ... '9':
            return readNumber();

        /* identifier or keywords */
        case '_':
        case 'a' ... 'z':
        case 'A' ... 'Z':
            return readIdentifier();

        /* operators */
        default:
            return readOperator();
    }
}

Token::Ptr Tokenizer::readString(void)
{
    int col = _state->col;
    int row = _state->row;
    char start = nextChar();
    char remains = nextChar();
    std::string result;

    while (start != remains)
    {
        if (!remains)
            throw Runtime::SyntaxError(this, "Unexpected EOF when scanning strings");

        if (remains == '\\')
        {
            switch ((remains = nextChar()))
            {
                case 0:
                    throw Runtime::SyntaxError(this, "Unexpected EOF when parsing escape sequence in strings");

                case '\'':
                case '\"':
                case '\\':
                    break;

                case 'a' : remains = '\a'; break;
                case 'b' : remains = '\b'; break;
                case 'f' : remains = '\f'; break;
                case 'n' : remains = '\n'; break;
                case 'r' : remains = '\r'; break;
                case 't' : remains = '\t'; break;
                case 'v' : remains = '\v'; break;
                case 'e' : remains = '\033'; break;

                case 'x':
                {
                    char msb = nextChar();
                    char lsb = nextChar();

                    if (!isHex(msb) || !isHex(lsb))
                        throw Runtime::SyntaxError(this, "Invalid '\\x' escape sequence");

                    remains = (char)((toInt(msb) << 4) | toInt(lsb));
                    break;
                }

                case '0' ... '7':
                {
                    /* first digit */
                    remains = (char)toInt(remains);

                    /* may have 2 more digits */
                    for (int i = 0; i < 2 && in(peekChar(), '0', '7'); i++)
                    {
                        remains <<= 3;
                        remains  |= toInt(nextChar());
                    }

                    break;
                }

                default:
                {
                    if (isprint(remains))
                        throw Runtime::SyntaxError(this, Utils::Strings::format("Invalid escape character '%c'", remains));
                    else
                        throw Runtime::SyntaxError(this, Utils::Strings::format("Invalid escape character '\\x%.2x'", remains));
                }
            }
        }

        result += remains;
        remains = nextChar();
    }

    return Token::createString(row, col, result);
}

Token::Ptr Tokenizer::readNumber(void)
{
    int col = _state->col;
    int row = _state->row;
    int base = 10;
    char number = nextChar();
    int64_t integer = toInt(number);

    if (number == '0')
    {
        switch (peekChar())
        {
            /* decimal number */
            case '.':
                break;

            /* binary number */
            case 'b':
            case 'B':
            {
                base = 2;
                nextChar();
                break;
            }

            /* hexadecimal number */
            case 'x':
            case 'X':
            {
                base = 16;
                nextChar();
                break;
            }

            /* octal number */
            case '0' ... '7':
            {
                base = 8;
                break;
            }

            /* simply integer zero */
            default:
                return Token::createValue(row, col, (int64_t)0);
        }
    }

    /* make charset */
    char charset[17] = "0123456789abcdef";
    charset[base] = 0;

    /* integer part */
    char follow = peekChar();
    while (follow && strchr(charset, tolower(follow)))
    {
        integer *= base;
        integer += toInt(follow);

        nextChar();
        follow = peekChar();
    }

    /* fraction part only makes sense when it's base 10 */
    if (base != 10 || follow != '.')
        return Token::createValue(row, col, integer);

    /* skip the decimal point */
    nextChar();

    /* the fraction part, but it may also be a "." or ".." opeartor */
    if (!in(peekChar(), '0', '9'))
    {
        _state->col--;
        _state->pos--;
        return Token::createValue(row, col, integer);
    }

    double factor = 1.0;
    double decimal = integer;

    /* merge to final result */
    do
    {
        factor *= 0.1;
        decimal += toInt(nextChar()) * factor;
    } while (in(peekChar(), '0', '9'));

    /* represent as "Float" token */
    return Token::createValue(row, col, decimal);
}

Token::Ptr Tokenizer::readOperator(void)
{
    int col = _state->col;
    int row = _state->row;
    char next = nextChar();

    switch (next)
    {
        /* single character operators */
        case '(' : return Token::createOperator(row, col, Token::Operator::BracketLeft    );
        case ')' : return Token::createOperator(row, col, Token::Operator::BracketRight   );
        case '[' : return Token::createOperator(row, col, Token::Operator::IndexLeft      );
        case ']' : return Token::createOperator(row, col, Token::Operator::IndexRight     );
        case '{' : return Token::createOperator(row, col, Token::Operator::BlockLeft      );
        case '}' : return Token::createOperator(row, col, Token::Operator::BlockRight     );
        case '~' : return Token::createOperator(row, col, Token::Operator::BitNot         );
        case '.' : return Token::createOperator(row, col, Token::Operator::Point          );
        case ',' : return Token::createOperator(row, col, Token::Operator::Comma          );
        case ':' : return Token::createOperator(row, col, Token::Operator::Colon          );
        case ';' : return Token::createOperator(row, col, Token::Operator::Semicolon      );
        case '\n': return Token::createOperator(row, col, Token::Operator::NewLine        );
        case '@' : return Token::createOperator(row, col, Token::Operator::Decorator      );

        /* != */
        case '!':
        {
            if (nextChar() != '=')
                throw Runtime::SyntaxError(this, "Invalid operator '!'");
            else
                return Token::createOperator(row, col, Token::Operator::Neq);
        }

        case '+': /* + += */
        case '/': /* / /= */
        case '%': /* % %= */
        case '&': /* & &= */
        case '|': /* | |= */
        case '^': /* ^ ^= */
        case '=': /* = == */
        {
            /* + / % & | ^ = */
            if (peekChar() != '=')
                return Token::createOperator(row, col, Operators.at(std::string(1, next)));

            /* += /= %= &= |= ^= == */
            nextChar();
            return Token::createOperator(row, col, Operators.at(next + std::string("=")));
        }

        /* - -= -> */
        case '-':
        {
            switch (peekChar())
            {
                /* -= */
                case '=':
                {
                    nextChar();
                    return Token::createOperator(row, col, Token::Operator::InplaceSub);
                }

                /* -> */
                case '>':
                {
                    nextChar();
                    return Token::createOperator(row, col, Token::Operator::Lambda);
                }

                /* - */
                default:
                    return Token::createOperator(row, col, Token::Operator::Minus);
            }
        }

        case '*': /* * ** *= **= */
        case '>': /* > >> >= >>= */
        case '<': /* < << <= <<= */
        {
            char follow = peekChar();

            /* *= >= <= */
            if (follow == '=')
            {
                nextChar();
                return Token::createOperator(row, col, Operators.at(next + std::string("=")));
            }
            else if (follow == next)
            {
                nextChar();

                /* ** << >> */
                if (peekChar() != '=')
                    return Token::createOperator(row, col, Operators.at(std::string(2, next)));

                /* **= <<= >>= */
                nextChar();
                return Token::createOperator(row, col, Operators.at(std::string(2, next) + "="));
            }
            else
            {
                /* * < > */
                return Token::createOperator(row, col, Operators.at(std::string(1, next)));
            }
        }

        /* other invalid operators */
        default:
        {
            if (isprint(next))
                throw Runtime::SyntaxError(this, Utils::Strings::format("Invalid operator '%c'", next));
            else
                throw Runtime::SyntaxError(this, Utils::Strings::format("Invalid character '\\x%.2x'", (uint8_t)next));
        }
    }
}

Token::Ptr Tokenizer::readIdentifier(void)
{
    int col = _state->col;
    int row = _state->row;
    char first = nextChar();
    char follow = peekChar();
    std::string token(1, first);

    while (follow)
    {
        switch (follow)
        {
            case '_':
            case '0' ... '9':
            case 'a' ... 'z':
            case 'A' ... 'Z':
            {
                token += nextChar();
                follow = peekChar();
                break;
            }

            default:
            {
                follow = 0;
                break;
            }
        }
    }

    if (Keywords.find(token) != Keywords.end())
        return Token::createKeyword(row, col, Keywords.at(token));
    else if (Operators.find(token) != Operators.end())
        return Token::createOperator(row, col, Operators.at(token));
    else
        return Token::createIdentifier(row, col, token);
}

Token::Ptr Tokenizer::next(void)
{
    for (;;)
    {
        /* fill the cache if empty */
        if (_state->cache.empty())
            _state->cache.emplace_back(read());

        /* pop one token from cache queue */
        Token::Ptr token = std::move(_state->cache.front());
        _state->cache.pop_front();

        /* skip all the `NewLine` operators */
        if (!(token->isOperator<Token::Operator::NewLine>()))
            return std::move(token);
    }
}

Token::Ptr Tokenizer::peek(void)
{
    /* find an non-`NewLine` token in queue */
    for (auto &token : _state->cache)
        if (!(token->isOperator<Token::Operator::NewLine>()))
            return token;

    /* not found, read new tokens */
    for (;;)
    {
        /* read one token, and add to cache */
        Token::Ptr token = read();
        _state->cache.push_back(token);

        /* skip all the `NewLine` operators */
        if (!(token->isOperator<Token::Operator::NewLine>()))
            return token;
    }
}

Token::Ptr Tokenizer::nextOrLine(void)
{
    /* fill the cache if empty */
    if (_state->cache.empty())
        _state->cache.emplace_back(read());

    /* pop one token from cache queue */
    Token::Ptr token = std::move(_state->cache.front());
    _state->cache.pop_front();

    /* move to prevent copy */
    return std::move(token);
}

Token::Ptr Tokenizer::peekOrLine(void)
{
    /* fill the cache if empty */
    if (_state->cache.empty())
        _state->cache.emplace_back(read());

    /* return the first element in queue */
    return _state->cache.front();
}
}
