/*
	cmd_parser.cpp — Tokenizer and command dispatch implementation
*/

#include "debugger/cmd_parser.h"
#include "debugger/dbg_io.h"

#include <cctype>
#include <cstdio>
#include <cstring>

bool ParseNumber(std::string_view text, uint32_t &outVal)
{
	if (text.empty()) return false;

	/* $hex */
	if (text[0] == '$')
	{
		auto hex = text.substr(1);
		if (hex.empty()) return false;
		uint32_t val = 0;
		for (char c : hex)
		{
			uint32_t d;
			if (c >= '0' && c <= '9')
				d = c - '0';
			else if (c >= 'a' && c <= 'f')
				d = 10 + (c - 'a');
			else if (c >= 'A' && c <= 'F')
				d = 10 + (c - 'A');
			else
				return false;
			val = (val << 4) | d;
		}
		outVal = val;
		return true;
	}

	/* 0xhex */
	if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
	{
		auto hex = text.substr(2);
		if (hex.empty()) return false;
		uint32_t val = 0;
		for (char c : hex)
		{
			uint32_t d;
			if (c >= '0' && c <= '9')
				d = c - '0';
			else if (c >= 'a' && c <= 'f')
				d = 10 + (c - 'a');
			else if (c >= 'A' && c <= 'F')
				d = 10 + (c - 'A');
			else
				return false;
			val = (val << 4) | d;
		}
		outVal = val;
		return true;
	}

	/* decimal */
	uint32_t val = 0;
	for (char c : text)
	{
		if (c < '0' || c > '9') return false;
		val = val * 10 + (c - '0');
	}
	outVal = val;
	return true;
}

std::vector<Token> Tokenize(std::string_view line)
{
	std::vector<Token> tokens;
	int pos = 0;
	int len = static_cast<int>(line.size());

	while (pos < len)
	{
		/* Skip whitespace */
		while (pos < len && line[pos] == ' ')
			++pos;
		if (pos >= len) break;

		char c = line[pos];

		/* Multi-char operators */
		if (pos + 1 < len)
		{
			char c2 = line[pos + 1];
			if ((c == '=' && c2 == '=') || (c == '!' && c2 == '=') || (c == '<' && c2 == '=') ||
				(c == '>' && c2 == '=') || (c == '&' && c2 == '&'))
			{
				Token t;
				t.kind = Token::Kind::Operator;
				t.text = std::string(line.substr(pos, 2));
				t.numValue = 0;
				tokens.push_back(std::move(t));
				pos += 2;
				continue;
			}
		}

		/* Single-char operators */
		if (c == '<' || c == '>' || c == '&' || c == '+' || c == '-' || c == '=' || c == '*' ||
			c == '(' || c == ')' || c == '/' || c == '#')
		{
			Token t;
			t.kind = Token::Kind::Operator;
			t.text = std::string(1, c);
			t.numValue = 0;
			tokens.push_back(std::move(t));
			++pos;
			continue;
		}

		/* $hex literal */
		if (c == '$')
		{
			int start = pos;
			++pos;
			while (pos < len && std::isxdigit(static_cast<unsigned char>(line[pos])))
				++pos;
			auto text = std::string(line.substr(start, pos - start));
			Token t;
			t.kind = Token::Kind::Number;
			t.text = text;
			ParseNumber(text, t.numValue);
			tokens.push_back(std::move(t));
			continue;
		}

		/* 0x hex or plain decimal */
		if (std::isdigit(static_cast<unsigned char>(c)))
		{
			int start = pos;
			if (c == '0' && pos + 1 < len && (line[pos + 1] == 'x' || line[pos + 1] == 'X'))
			{
				pos += 2;
				while (pos < len && std::isxdigit(static_cast<unsigned char>(line[pos])))
					++pos;
			}
			else
			{
				while (pos < len && std::isdigit(static_cast<unsigned char>(line[pos])))
					++pos;
			}
			auto text = std::string(line.substr(start, pos - start));
			Token t;
			t.kind = Token::Kind::Number;
			t.text = text;
			ParseNumber(text, t.numValue);
			tokens.push_back(std::move(t));
			continue;
		}

		/* Word */
		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.')
		{
			int start = pos;
			while (pos < len && (std::isalnum(static_cast<unsigned char>(line[pos])) ||
								 line[pos] == '_' || line[pos] == '.'))
				++pos;
			Token t;
			t.kind = Token::Kind::Word;
			t.text = std::string(line.substr(start, pos - start));
			t.numValue = 0;
			tokens.push_back(std::move(t));
			continue;
		}

		/* Unknown character — skip */
		++pos;
	}

	/* Sentinel */
	Token end;
	end.kind = Token::Kind::End;
	end.numValue = 0;
	tokens.push_back(std::move(end));
	return tokens;
}

const CmdEntry *DispatchCommand(std::string_view input, const CmdEntry *table, int tableSize,
								DbgIO *io)
{
	if (input.empty()) return nullptr;

	std::vector<const CmdEntry *> matches;

	for (int i = 0; i < tableSize; ++i)
	{
		const auto &e = table[i];
		/* Exact match on name or shortcut */
		if (e.name == input || (!e.shortcut.empty() && e.shortcut == input))
		{
			return &e;
		}
		/* Prefix match on name */
		if (e.name.size() > input.size() && e.name.substr(0, input.size()) == input)
		{
			matches.push_back(&e);
		}
	}

	if (matches.size() == 1)
	{
		return matches[0];
	}
	if (matches.size() > 1)
	{
		if (io)
		{
			io->write("Ambiguous command '%.*s'. Candidates:", static_cast<int>(input.size()),
					  input.data());
			for (auto *m : matches)
				io->write(" %.*s", static_cast<int>(m->name.size()), m->name.data());
			io->write("\n");
		}
		else
		{
			std::printf("Ambiguous command '%.*s'. Candidates:", static_cast<int>(input.size()),
						input.data());
			for (auto *m : matches)
				std::printf(" %.*s", static_cast<int>(m->name.size()), m->name.data());
			std::printf("\n");
		}
		return nullptr;
	}

	if (io)
		io->write("Unknown command '%.*s'. Type 'help' for a list.\n",
				  static_cast<int>(input.size()), input.data());
	else
		std::printf("Unknown command '%.*s'. Type 'help' for a list.\n",
					static_cast<int>(input.size()), input.data());
	return nullptr;
}
