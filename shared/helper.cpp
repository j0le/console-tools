
#include "console-tools/helper.h"

#include <cassert>
#include <memory>
#include <sstream>
#include <fmt/core.h>
#include <nowide/args.hpp>

error_or<std::optional<ConsoleCtrlEvent>> parse_event_string(std::string_view event_name) {

	if (event_name == "" || event_name == "none") {
		return std::optional<ConsoleCtrlEvent>(std::nullopt);
	}
	else if (event_name == "ctrl-c") {
		return ConsoleCtrlEvent::ctrl_c_event;
	}
	else if (event_name == "ctrl-break") {
		return ConsoleCtrlEvent::ctrl_break_event;
	}
	else
		return error_t{};
}

std::string event_to_string(std::optional<ConsoleCtrlEvent> event) {
	if (!event.has_value()) {
		return "";
	}

	switch (*event) {
	case ConsoleCtrlEvent::ctrl_c_event:
		return "ctrl-c";
	case ConsoleCtrlEvent::ctrl_break_event:
		return "ctrl-break";
	default:
		assert(false && "this should not happen");
		return "invalid";
	}
}

std::optional<std::string> get_error_message(DWORD error_code) {

	LPWSTR wstr_buffer = nullptr;
	std::optional<std::string> message_opt{};
	DWORD number_of_wchars_without_terminating_nul = ::FormatMessageW(
		FORMAT_MESSAGE_IGNORE_INSERTS   // ignore paremeter "Arguments"
		| FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER, // function allocates buffer using
		// LocalAlloc. That means we have to
		// use local free
		nullptr,                        // lpSource
		error_code,                     // dwMessageId
		0, // dwLanguageId. Zero means: Try different languages until message is
		// found.
		reinterpret_cast<LPWSTR>(
			&wstr_buffer), // We have to cast, because the argument is interpreted
		// differently based on flags

		0,                 // nSize. We don't know the size yet
		nullptr);
	if (number_of_wchars_without_terminating_nul != 0) {
		message_opt = ::nowide::narrow(wstr_buffer,
			number_of_wchars_without_terminating_nul);
	}

	if (wstr_buffer != nullptr) {
		if (LocalFree(wstr_buffer) != nullptr) {
			message_opt.reset();
		}
	}
	return message_opt;
}

std::optional<std::string> GetProgPath(FILE* streamErr) {
	std::string ret{};
	for (DWORD buffer_size = 0x100u; buffer_size < 0x10000u && buffer_size >= 0x100u; buffer_size *= 2u) {
		auto prog_name = std::make_unique<CHAR[]>(buffer_size);
		DWORD result = GetModuleFileNameA(nullptr, prog_name.get(), buffer_size);

		if (result == 0) {
			auto error = GetLastError();
			fmt::print(streamErr, "Couldn't get name of the program - {}\n", get_error_message(error).value_or(""));
			return std::nullopt;
		}

		if (result < buffer_size) {
			return std::string(prog_name.get());
		}

		auto error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER)
			continue;

		fmt::print(streamErr, "Unexpected error: {}\n", get_error_message(error).value_or(""));
		return std::nullopt;
	}

	fmt::print(streamErr, "The path to this program is to long.\n");
	return std::nullopt;
}

std::string indent_message(const std::string_view& spaces, const std::string& str) {
	std::ostringstream oss{};
	size_t next_start_index_of_substr{ 0 };
	for (size_t i = 0; i < str.length(); ++i) {
		char c = str[i];
		if (c == '\n') {
			if (next_start_index_of_substr == 0 && i == str.length() - 1)
				return str;
			else {
				if (next_start_index_of_substr == 0 && i != 0)
					oss << '\n';
				oss << spaces << str.substr(next_start_index_of_substr, i - next_start_index_of_substr + 1);
				next_start_index_of_substr = i + 1;
			}
		}
	}

	if (str.length() == next_start_index_of_substr)
		return oss.str();

	oss << spaces << str.substr(next_start_index_of_substr, str.length() - next_start_index_of_substr) << '\n';
	return oss.str();
}