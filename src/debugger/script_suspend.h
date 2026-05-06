// Script suspension — holds remaining lines when a script resumes the CPU.
#pragma once

#include <string>
#include <string_view>
#include <vector>

// Holds the remaining lines of a suspended script.
// When a script command resumes the CPU (e.g. wait, continue),
// execution suspends here and resumes when the debugger next stops.
struct PendingScript
{
	std::vector<std::string> lines;
	int nextLine = 0;
	std::string sourceFile; // for error messages

	bool exhausted() const { return nextLine >= static_cast<int>(lines.size()); }
	std::string_view currentLine() const { return lines[nextLine]; }
	void advance() { ++nextLine; }
	void clear()
	{
		lines.clear();
		nextLine = 0;
		sourceFile.clear();
	}
};
