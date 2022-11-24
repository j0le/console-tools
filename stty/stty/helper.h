#pragma once
#include<optional>
#include <cstdint>
#include <string>

enum class result : bool { FAIL = false, SUCCESS = true };

constexpr std::string_view quote_open{ "\xC2\xBB" };  // >> U+00BB
constexpr std::string_view quote_close{ "\xC2\xAB" }; // << U+00AB

constexpr std::optional<std::uint32_t> string_to_uint32(const std::string_view& str);
constexpr std::optional<std::uint64_t> string_to_uint64(const std::string_view& str);

// ---------

#include <type_traits>
#include <limits>

template<class uint_type, std::size_t max_number_of_digits>
constexpr std::optional<uint_type> string_base10_to_integer(const uint_type(&digits_of_max_number)[max_number_of_digits], const std::string_view& str) {
    static_assert(sizeof(digits_of_max_number) == sizeof(uint_type) * max_number_of_digits);
    static_assert(std::is_unsigned_v<uint_type>);

    if (str.empty() || str.size() > max_number_of_digits)
        return std::nullopt;


    bool must_check_for_max_num = str.size() == max_number_of_digits;
    unsigned index = 0;

    uint_type out_number = 0;
    for (char c : str) {
        static_assert('0' < '9');
        if (c < '0' || c > '9')
            return std::nullopt;

        uint_type digit = c - '0';

        if (digit == 0 && out_number == 0)
            continue;

        if (must_check_for_max_num) {
            auto max_num_digit = digits_of_max_number[index];
            ++index;
            if (digit > max_num_digit)
                return std::nullopt;
            if (digit < max_num_digit)
                must_check_for_max_num = false;
        }

        out_number *= 10;
        out_number += digit;
    }
    return out_number;
}

template<class uint_type, std::size_t max_number_of_digits>
constexpr bool does_array_represent_max_number(const uint_type(&digits)[max_number_of_digits]) {
    uint_type value{ 0 };
    for (int i{ 0 }; i < max_number_of_digits; ++i) {
        value *= 10;
        value += digits[i];
    }
    return value == std::numeric_limits<uint_type>::max();
}

constexpr std::optional<std::uint32_t> string_to_uint32(const std::string_view& str) {

    static_assert(sizeof(uint32_t) == 4);
    // 2^32 - 1  == 4'294'967'295
    constexpr const uint32_t max_number_digits[10] = { 4,2,9,4,9,6,7,2,9,5 };
    static_assert(does_array_represent_max_number(max_number_digits));

    return string_base10_to_integer(max_number_digits, str);
}



constexpr std::optional<std::uint64_t> string_to_uint64(const std::string_view& str) {

    // 2^64 - 1 == 18446744073709551615
    constexpr const uint64_t digits_of_max_number[] = { 1,8,4,4,6,7,4,4,0,7,3,7,0,9,5,5,1,6,1,5 };
    static_assert(does_array_represent_max_number(digits_of_max_number));

    return string_base10_to_integer(digits_of_max_number, str);
}