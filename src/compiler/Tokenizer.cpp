#include <unordered_map>
#include "compiler/Tokenizer.h"

namespace RedScript::Compiler
{
static const std::unordered_map<std::string, Token::Keyword> Keywords = {
    { "if"      , Token::If         },
    { "for"     , Token::For        },
    { "while"   , Token::While      },
    { "switch"  , Token::Switch     },
    { "case"    , Token::Case       },
    { "default" , Token::Default    },

    { "break"   , Token::Break      },
    { "continue", Token::Continue   },
    { "return"  , Token::Return     },

    { "try"     , Token::Try        },
    { "except"  , Token::Except     },
    { "finally" , Token::Finally    },
    { "raise"   , Token::Raise      },

    { "class"   , Token::Class      },
    { "func"    , Token::Function   },

    { "as"      , Token::As         },
    { "in"      , Token::In         },
    { "delete"  , Token::Delete     },
    { "import"  , Token::Import     },
};

static const std::unordered_map<std::string, Token::Operator> Operators = {
    { "("   , Token::BracketLeft        },
    { ")"   , Token::BracketRight       },
    { "["   , Token::IndexLeft          },
    { "]"   , Token::IndexRight         },
    { "{"   , Token::BlockLeft          },
    { "}"   , Token::BlockRight         },

    { ","   , Token::Comma              },
    { "."   , Token::Point              },
    { ":"   , Token::Colon              },
    { ";"   , Token::Semicolon          },
    { "\n"  , Token::NewLine            },

    { "<"   , Token::Less               },
    { ">"   , Token::Greater            },
    { "<="  , Token::Leq                },
    { ">="  , Token::Geq                },
    { "=="  , Token::Equ                },
    { "!="  , Token::Neq                },

    { "and" , Token::BoolAnd            },
    { "or"  , Token::BoolOr             },
    { "not" , Token::BoolNot            },
    { "xor" , Token::BoolXor            },

    { "+"   , Token::Plus               },
    { "-"   , Token::Minus              },
    { "/"   , Token::Divide             },
    { "*"   , Token::Multiply           },
    { "%"   , Token::Module             },
    { "**"  , Token::Power              },

    { "&"   , Token::BitAnd             },
    { "|"   , Token::BitOr              },
    { "~"   , Token::BitNot             },
    { "^"   , Token::BitXor             },
    { "<<"  , Token::ShiftLeft          },
    { ">>"  , Token::ShiftRight         },

    { "+="  , Token::InplaceAdd         },
    { "-="  , Token::InplaceSub         },
    { "*="  , Token::InplaceMul         },
    { "/="  , Token::InplaceDiv         },
    { "%="  , Token::InplaceMod         },
    { "**=" , Token::InplacePower       },

    { "&="  , Token::InplaceBitAnd      },
    { "|="  , Token::InplaceBitOr       },
    { "^="  , Token::InplaceBitXor      },
    { "<<=" , Token::InplaceShiftLeft   },
    { ">>=" , Token::InplaceShiftRight  },

    { "="   , Token::Assign             },
    { "=>"  , Token::Tuple              },
    { ".."  , Token::Range              },
    { "@"   , Token::Decorator          },
};

template <typename T> static inline bool in(T c, T a, T b)  { return c >= a && c <= b; }
template <typename T> static inline bool isHex(T c)         { return in(c, '0', '9') || in(c, 'a', 'f') || in(c, 'A', 'F'); }
template <typename T> static inline long toInt(T c)         { return in(c, '0', '9') ? (c - '0') : in(c, 'a', 'f') ? (c - 'a' + 10) : (c - 'A' + 10); }

/****** Token ******/

std::string Token::toString(void) const
{
    switch (_type)
    {
        case Eof            : return "<Eof>";
        case Float          : return Strings::format("<Float %f>"       , _float);
        case String         : return Strings::format("<String %s>"      , Strings::repr(_string.data(), _string.size()));
        case Integer        : return Strings::format("<Integer %ld>"    , _integer);
        case Identifiers    : return Strings::format("<Identifier %s>"  , _string);

        case Keywords:
        {
            switch (_keyword)
            {
                case If         : return "<Keyword if>";
                case For        : return "<Keyword for>";
                case While      : return "<Keyword while>";
                case Switch     : return "<Keyword switch>";
                case Case       : return "<Keyword case>";
                case Default    : return "<Keyword default>";

                case Break      : return "<Keyword break>";
                case Continue   : return "<Keyword continue>";
                case Return     : return "<Keyword return>";

                case Try        : return "<Keyword try>";
                case Except     : return "<Keyword except>";
                case Finally    : return "<Keyword finally>";
                case Raise      : return "<Keyword raise>";

                case Class      : return "<Keyword class>";
                case Function   : return "<Keyword func>";

                case As         : return "<Keyword as>";
                case In         : return "<Keyword in>";
                case Delete     : return "<Keyword delete>";
                case Import     : return "<Keyword import>";
            }
        }

        case Operators:
        {
            switch (_operator)
            {
                case BracketLeft        : return "<Operator '('>";
                case BracketRight       : return "<Operator ')'>";
                case IndexLeft          : return "<Operator '['>";
                case IndexRight         : return "<Operator ']'>";
                case BlockLeft          : return "<Operator '{'>";
                case BlockRight         : return "<Operator '}'>";

                case Comma              : return "<Operator ','>";
                case Point              : return "<Operator '.'>";
                case Colon              : return "<Operator ':'>";
                case Semicolon          : return "<Operator ';'>";
                case NewLine            : return "<Operator NewLine>";

                case Less               : return "<Operator '<'>";
                case Greater            : return "<Operator '>'>";
                case Leq                : return "<Operator '<='>";
                case Geq                : return "<Operator '>='>";
                case Equ                : return "<Operator '=='>";
                case Neq                : return "<Operator '!='>";

                case BoolAnd            : return "<Operator 'and'>";
                case BoolOr             : return "<Operator 'or'>";
                case BoolNot            : return "<Operator 'not'>";
                case BoolXor            : return "<Operator 'xor'>";

                case Plus               : return "<Operator '+'>";
                case Minus              : return "<Operator '-'>";
                case Divide             : return "<Operator '/'>";
                case Multiply           : return "<Operator '*'>";
                case Module             : return "<Operator '%'>";
                case Power              : return "<Operator '**'>";

                case BitAnd             : return "<Operator '&'>";
                case BitOr              : return "<Operator '|'>";
                case BitNot             : return "<Operator '~'>";
                case BitXor             : return "<Operator '^'>";
                case ShiftLeft          : return "<Operator '<<'>";
                case ShiftRight         : return "<Operator '>>'>";

                case InplaceAdd         : return "<Operator '+='>";
                case InplaceSub         : return "<Operator '-='>";
                case InplaceMul         : return "<Operator '*='>";
                case InplaceDiv         : return "<Operator '/='>";
                case InplaceMod         : return "<Operator '%='>";
                case InplacePower       : return "<Operator '**='>";

                case InplaceBitAnd      : return "<Operator '&='>";
                case InplaceBitOr       : return "<Operator '|='>";
                case InplaceBitXor      : return "<Operator '^='>";
                case InplaceShiftLeft   : return "<Operator '<<='>";
                case InplaceShiftRight  : return "<Operator '>>='>";

                case Assign             : return "<Operator '='>";
                case Tuple              : return "<Operator '=>'>";
                case Range              : return "<Operator '..'>";
                case Decorator          : return "<Operator '@'>";
            }
        }
    }

    /* never happens */
    abort();
}

/****** Tokenizer ******/

Tokenizer::Tokenizer(const std::string &source) : _source(source)
{
    /* initial state */
    _stack.push(State {
        .col = 0,
        .row = 0,
        .pos = 0,
        .cache = nullptr,
    });

    /* fast reference */
    _state = &(_stack.top());
}

char Tokenizer::peekChar(void)
{
    /* check for overflow */
    if (_state->pos >= _source.size())
        return 0;

    /* peek next char */
    char result = _source[_state->pos];
    return result == '\r' ? (char)'\n' : result;
}

char Tokenizer::nextChar(void)
{
    /* check for overflow */
    if (_state->pos >= _source.size())
        return 0;

    /* peek next char */
    char result = _source[_state->pos];

    switch (result)
    {
        case 0:
            return 0;

        case '\r':
        case '\n':
        {
            _state->row++;
            _state->col = 0;

            /* '\r\n' or '\n\r' */
            if (_state->pos < _source.size() && _source[_state->pos + 1] == (result == '\n' ? '\r' : '\n'))
                _state->pos++;

            result = '\n';
            break;
        }

        default:
            break;
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
    char start = nextChar();
    char remains = nextChar();
    std::string result;

    while (start != remains)
    {
        if (!remains)
            throw Runtime::SyntaxError(_state->row, _state->col, "Unexpected EOF when scanning strings");

        if (remains == '\\')
        {
            switch ((remains = nextChar()))
            {
                case 0:
                    throw Runtime::SyntaxError(_state->row, _state->col, "Unexpected EOF when parsing escape sequence in strings");

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
                        throw Runtime::SyntaxError(_state->row, _state->col, "Invalid '\\x' escape sequence");

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
                        throw Runtime::SyntaxError(_state->row, _state->col, Strings::format("Invalid escape character '%c'", remains));
                    else
                        throw Runtime::SyntaxError(_state->row, _state->col, Strings::format("Invalid escape character '\\x%.2x'", remains));
                }
            }
        }

        result += remains;
        remains = nextChar();
    }

    return Token::createString(_state->row, _state->col, result);
}

Token::Ptr Tokenizer::readNumber(void)
{
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
                return Token::createValue(_state->row, _state->col, 0ll);
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
        return Token::createValue(_state->row, _state->col, integer);

    /* skip the decimal point */
    nextChar();

    /* the fraction part, but it may also be a "." or ".." opeartor */
    if (!in(peekChar(), '0', '9'))
    {
        _state->col--;
        _state->pos--;
        return Token::createValue(_state->row, _state->col, integer);
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
    return Token::createValue(_state->row, _state->col, decimal);
}

Token::Ptr Tokenizer::readOperator(void)
{
    switch (char op = nextChar())
    {
        /* single character operators */
        case '(' : return Token::createOperator(_state->row, _state->col, Token::BracketLeft    );
        case ')' : return Token::createOperator(_state->row, _state->col, Token::BracketRight   );
        case '[' : return Token::createOperator(_state->row, _state->col, Token::IndexLeft      );
        case ']' : return Token::createOperator(_state->row, _state->col, Token::IndexRight     );
        case '{' : return Token::createOperator(_state->row, _state->col, Token::BlockLeft      );
        case '}' : return Token::createOperator(_state->row, _state->col, Token::BlockRight     );
        case '~' : return Token::createOperator(_state->row, _state->col, Token::BitNot         );
        case ',' : return Token::createOperator(_state->row, _state->col, Token::Comma          );
        case ':' : return Token::createOperator(_state->row, _state->col, Token::Colon          );
        case ';' : return Token::createOperator(_state->row, _state->col, Token::Semicolon      );
        case '\n': return Token::createOperator(_state->row, _state->col, Token::NewLine        );
        case '@' : return Token::createOperator(_state->row, _state->col, Token::Decorator      );

        /* != */
        case '!':
        {
            if (nextChar() == '=')
                return Token::createOperator(_state->row, _state->col, Token::Neq);
            else
                throw Runtime::SyntaxError(_state->row, _state->col, "Invalid operator '!'");
        }

        /* . .. */
        case '.':
        {
            /* . */
            if (peekChar() != '.')
                return Token::createOperator(_state->row, _state->col, Token::Point);

            /* .. */
            nextChar();
            return Token::createOperator(_state->row, _state->col, Token::Range);
        }

        case '+': /* + += */
        case '-': /* - -= */
        case '/': /* / /= */
        case '%': /* % %= */
        case '&': /* & &= */
        case '|': /* | |= */
        case '^': /* ^ ^= */
        {
            /* + - / % & | ^ */
            if (peekChar() != '=')
                return Token::createOperator(_state->row, _state->col, Operators.at(std::string(1, op)));

            /* += -= /= %= &= |= ^= */
            nextChar();
            return Token::createOperator(_state->row, _state->col, Operators.at(op + std::string("=")));
        }

        /* = == => */
        case '=':
        {
            switch (peekChar())
            {
                /* == */
                case '=':
                {
                    nextChar();
                    return Token::createOperator(_state->row, _state->col, Token::Equ);
                }

                /* => */
                case '>':
                {
                    nextChar();
                    return Token::createOperator(_state->row, _state->col, Token::Tuple);
                }

                /* = */
                default:
                    return Token::createOperator(_state->row, _state->col, Token::Assign);
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
                return Token::createOperator(_state->row, _state->col, Operators.at(op + std::string("=")));
            }
            else if (follow == op)
            {
                nextChar();

                /* ** << >> */
                if (peekChar() != '=')
                    return Token::createOperator(_state->row, _state->col, Operators.at(std::string(2, op)));

                /* **= <<= >>= */
                nextChar();
                return Token::createOperator(_state->row, _state->col, Operators.at(std::string(2, op) + "="));
            }
            else
            {
                /* * < > */
                return Token::createOperator(_state->row, _state->col, Operators.at(std::string(1, op)));
            }
        }

        /* other invalid operators */
        default:
        {
            if (isprint(op))
                throw Runtime::SyntaxError(_state->row, _state->col, Strings::format("Invalid operator '%c'", op));
            else
                throw Runtime::SyntaxError(_state->row, _state->col, Strings::format("Invalid character '\\x%.2x'", (uint8_t)op));
        }
    }
}

Token::Ptr Tokenizer::readIdentifier(void)
{
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
        return Token::createKeyword(_state->row, _state->col, Keywords.at(token));
    else if (Operators.find(token) != Operators.end())
        return Token::createOperator(_state->row, _state->col, Operators.at(token));
    else
        return Token::createIdentifier(_state->row, _state->col, token);
}

Token::Ptr Tokenizer::next(void)
{
    /* read from cache first */
    Token::Ptr token = std::move(_state->cache == nullptr ? read() : _state->cache);

    /* skip "\n" operator */
    while (token != nullptr && token->is<Token::Operators>() && (token->asOperator() == Token::NewLine))
        token = std::move(read());

    return std::move(token);
}

Token::Ptr Tokenizer::peek(void)
{
    if (_state->cache == nullptr)
        _state->cache = std::move(read());

    /* skip "\n" operator */
    while (_state->cache != nullptr && _state->cache->is<Token::Operators>() && (_state->cache->asOperator() == Token::NewLine))
        _state->cache = std::move(read());

    /* preserve cache */
    return _state->cache;
}

Token::Ptr Tokenizer::nextOrLine(void)
{
    if (_state->cache == nullptr)
        return std::move(read());
    else
        return std::move(_state->cache);
}

Token::Ptr Tokenizer::peekOrLine(void)
{
    if (_state->cache == nullptr)
        _state->cache = std::move(read());

    /* preserve cache */
    return _state->cache;
}
}