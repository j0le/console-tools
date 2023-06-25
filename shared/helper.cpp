
#include "console-tools/helper.h"

#include <cassert>

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