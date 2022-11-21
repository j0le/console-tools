#include <Windows.h>

#include <fmt/core.h>
#include <nowide/args.hpp>
#include <string>

//void print_bits(DWORD dw) {
//	constexpr const int number_of_bits = sizeof(dw) * 8;
//	for (int i = number_of_bits - 1; i >= 0; --i) {
//		bool one = 0 != (dw & (1 << i));
//
//		fmt::print("{}", one ? "1" : ".");
//
//		// space separators
//		if (i != 0 && (i % 4) == 0)
//			fmt::print(" ");
//	}
//}

#if !defined(UNICODE)
#error macro UNICODE is not defined
#endif

#if !defined(_UNICODE)
#error macro _UNICODE is not defined
#endif

constexpr const char8_t UTF_8_test_1[] = u8"ü";
static_assert(sizeof(UTF_8_test_1) == 3);
static_assert(UTF_8_test_1[0] == static_cast<char8_t>(0xC3u));
static_assert(UTF_8_test_1[1] == static_cast<char8_t>(0xBCu));
static_assert(UTF_8_test_1[2] == static_cast<char8_t>(0x0u));

constexpr const char UTF_8_test_2[] = "ü";
static_assert(sizeof(UTF_8_test_2) == 3);
static_assert(UTF_8_test_2[0] == static_cast<char>(0xC3u));
static_assert(UTF_8_test_2[1] == static_cast<char>(0xBCu));
static_assert(UTF_8_test_2[2] == static_cast<char>(0x0u));

bool get_error_message(DWORD error_code, ::std::string& message) {

	LPWSTR wstr_buffer = nullptr;
	bool return_value{ true };
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
	if (number_of_wchars_without_terminating_nul == 0) {
		return_value = false;
	}
	else {
		message = ::nowide::narrow(wstr_buffer,
			number_of_wchars_without_terminating_nul);
	}

	if (wstr_buffer != nullptr) {
		if (LocalFree(wstr_buffer) != nullptr) {
			return_value = false;
		}
	}
	return return_value;
}

void PrintFileType(std::string_view leading, HANDLE handle) {
	auto file_type = GetFileType(handle);
	switch (file_type) {
	case FILE_TYPE_CHAR:
		fmt::print("{}Filetype: FILE_TYPE_CHAR\n", leading);
		break;
	case FILE_TYPE_DISK:
		fmt::print("{}Filetype: FILE_TYPE_DISK\n", leading);
		break;
	case FILE_TYPE_PIPE:
		fmt::print("{}Filetype: FILE_TYPE_PIPE\n", leading);
		break;
	case FILE_TYPE_REMOTE:
		fmt::print("{}Filetype: FILE_TYPE_REMOTE\n", leading);
		break;
	case FILE_TYPE_UNKNOWN: {
		auto error = GetLastError();
		if (error == NO_ERROR) {
			fmt::print("{}Filetype: FILE_TYPE_UNKNOWN\n",leading);
		}
		else {
			std::string error_message{};
			bool got_msg = get_error_message(error, error_message);
			fmt::print("{0}Filetype: FILE_TYPE_UNKNOWN, error: {1:#x} {1:d} \"{2}\"\n",leading, error, got_msg?error_message:"");
		}
		break;
	}
	default:
		fmt::print("{}Filetype: {:#0x} - this is unexpected, undocumented and an error", leading, file_type);
		break;
	}
}

void PrintMode(std::string_view name, HANDLE hStd) {
	if (hStd == INVALID_HANDLE_VALUE || hStd == nullptr) {
		fmt::print("{}: invalid\n", name);
	}
	else {
		static_assert(sizeof(HANDLE) == sizeof(uintptr_t));

		fmt::print("{}: {:#x}\n", name, std::bit_cast<uintptr_t>(hStd));
		DWORD console_output_mode{};
		if (GetConsoleMode(hStd, &console_output_mode)) {
			fmt::print("  console mode: {:#0{}b}\n", console_output_mode, sizeof(console_output_mode) * 8 + 2);
		}
		else {
			auto error = GetLastError();
			std::string message{};
			get_error_message(error, message);
			fmt::print("  console mode: error {:#x} \"{}\"", error, message);
		}

		PrintFileType("  ", hStd);
	}
}

int main() {
	UINT acp = GetACP();
	UINT oem_cp = GetACP();
	UINT console_input_cp = GetConsoleCP();
	UINT console_output_cp = GetConsoleOutputCP();
	fmt::print("ACP:               {}\n", acp);
	fmt::print("OEM CP:            {}\n", oem_cp);
	fmt::print("Console Input CP:  {}\n", console_input_cp);
	fmt::print("Console Output CP: {}\n", console_output_cp);

	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	PrintMode("stdin", hStdIn);

	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	PrintMode("stdout", hStdOut);

	HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
	PrintMode("stderr", hStdErr);

	if (hStdOut != INVALID_HANDLE_VALUE && hStdErr != INVALID_HANDLE_VALUE) {
		if (hStdOut == hStdErr)
			fmt::print("The handles for stdout and stderr are equal.\n");
		if (CompareObjectHandles(hStdOut, hStdErr))
			fmt::print("The handles for stdout and stderr point to the same kernel object.\n");
		else
			fmt::print("The handles for stdout and stderr do not point to the same kernel object.{}\n",
				hStdOut == hStdErr ? " It seems like, the object they are pointing to is not a kernel object, but some other kind of object." : "");
	}

	return 0;
}