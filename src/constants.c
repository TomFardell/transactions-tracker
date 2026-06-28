#include "constants.h"

#include "base/date.h"
#include "base/string.h"

const char *PORT = "3490";
const String data_dir = string_literal("data/");
const String static_dir = string_literal("static/");
const String parsed_dir = string_literal("parsed/");
const String not_found_file = string_literal("error.html");
const DateFormat date_format = DATE_FORMAT_ALPHABETICAL_SHORT;
const DayOfWeekFormat day_of_week_format = DAY_OF_WEEK_FORMAT_HIDDEN;
const String parse_tag_opener = string_literal("<!--{{");
const String parse_tag_closer = string_literal("}}-->");
