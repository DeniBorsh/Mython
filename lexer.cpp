#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    ostream& operator<<(ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(istream& input) 
        : input_{input} {
        ReadToken();
    }

    const Token& Lexer::CurrentToken() const {
        if (tokens_.empty()) return eof_;
        
        return tokens_.back();
    }

    Token Lexer::NextToken() {
        bool added_token = ReadToken();
        return added_token ? tokens_.back() : eof_;
    }

    bool Lexer::CalculateIndent() {
        int counter = 0;
        for (; input_.peek() == ' '; ++counter) input_.get();

        int diff = counter - current_indent_;
        if (diff == 0) return false;
        else if (diff % 2 == 0 && diff > 0) { 
            tokens_.push_back(token_type::Indent{}); 
            current_indent_ += 2; 
        }
        else if (diff % 2 == 0 && diff < 0) {
            tokens_.push_back(token_type::Dedent{});
            current_indent_ -= 2;
        }
        else throw LexerError("Indents are not even");

        for (int i = 0; i < counter; ++i) input_.putback(' ');
        return true;
    }

    void Lexer::AddNumber() {
        int num;
        input_ >> num;
        tokens_.push_back(token_type::Number{num});
    }

    void Lexer::AddString() {
        string str;
        for (char beg = input_.get(), c; input_.get(c) && ((beg == '\'' && c != '\'') || (beg == '\"' && c != '\"'));) str.push_back(c);
        tokens_.push_back(token_type::String{ str });
    }


    void Lexer::AddDoubleChar() {
        char c = input_.get();
        char next;
        if (input_ >> next && next == '=') {
            if (c == '=') tokens_.push_back(token_type::Eq{});
            if (c == '<') tokens_.push_back(token_type::LessOrEq{});
            if (c == '>') tokens_.push_back(token_type::GreaterOrEq{});
            if (c == '!') tokens_.push_back(token_type::NotEq{});
        }
        else {
            input_.putback(next);
            tokens_.push_back(token_type::Char{ c });
        }
    }

    void Lexer::AddKeywordOrId() {
        string keyword;

        char c;
        while (input_.get(c) && (isdigit(c) || isalpha(c) || c == '_')) keyword.push_back(c);
        input_.putback(c);

        if (keyword == "class") tokens_.push_back(token_type::Class{});
        else if (keyword == "if") tokens_.push_back(token_type::If{});
        else if (keyword == "else") tokens_.push_back(token_type::Else{});
        else if (keyword == "or") tokens_.push_back(token_type::Or{});
        else if (keyword == "and") tokens_.push_back(token_type::And{});
        else if (keyword == "not") tokens_.push_back(token_type::Not{});
        else if (keyword == "True") tokens_.push_back(token_type::True{});
        else if (keyword == "False") tokens_.push_back(token_type::False{});
        else if (keyword == "None") tokens_.push_back(token_type::None{});
        else if (keyword == "return") tokens_.push_back(token_type::Return{});
        else if (keyword == "def") tokens_.push_back(token_type::Def{});
        else if (keyword == "print") tokens_.push_back(token_type::Print{});
        else tokens_.push_back(token_type::Id(keyword));
    }

    void Lexer::RemoveSpaces() {
        char c;
        while (input_.get(c) && isspace(c));
        input_.putback(c);
    }

    void Lexer::AddNewline() {
        input_.get();
        tokens_.push_back(token_type::Newline{});
    }

    void Lexer::AddChar(char c) {
        input_.get();
        tokens_.push_back(token_type::Char{ c });
    }

    bool Lexer::ReadToken() {
        if (input_.peek() == EOF) return false;
        if ((CurrentToken().Is<token_type::Newline>() || 
             CurrentToken().Is<token_type::Dedent>() ) && CalculateIndent()) return true;

        char c = input_.peek();
        if (!tokens_.empty() && c == '\n') AddNewline();
        else if (isalpha(c) || c == '_') AddKeywordOrId();
        else if (isdigit(c)) AddNumber();
        else if (c == '\'' || c == '\"') AddString();
        else if (isspace(c)) { RemoveSpaces(); return ReadToken(); }
        else if (c == '=' || c == '<' || c == '>' || c == '!') AddDoubleChar();
        else AddChar(c);
        return true;
    }

}  // namespace parse