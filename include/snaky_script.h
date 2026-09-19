#pragma once

#ifdef _WIN32
	#ifdef SNAKY_DLL
		#ifdef SNAKY_EXPORTS
			#define SNAKY_API __declspec(dllexport)
		#else
			#define SNAKY_API __declspec(dllimport)
		#endif
	#else
		#define SNAKY_API
	#endif
#else
	#define SNAKY_API
#endif

#include <stddef.h>

/**
  Define SNAKY_CUSTOM_BUF_SIZE to change
  the default buffer size of SnakyScript.
*/
#ifdef SNAKY_CUSTOM_BUF_SIZE
	#define SNAKY_BUF_SIZE SNAKY_CUSTOM_BUF_SIZE
#else
	/**
	  The default buffer size of SnakyScript.
	*/
	#define SNAKY_BUF_SIZE 64
#endif

/**
  Represents a map with argument names as the keys
  and argument values as the values.
*/
typedef struct snaky_arg_data
{
	/**
	  The keys of the map are the argument names in the string.
	*/
	char **keys;
	/**
	  The values of the map are the argument values in the string.
	*/
	char **values;
	/**
	  The current size of the map.
	*/
	size_t size;
	/**
	  The capacity of the map.
	*/
	size_t capacity;
	/**
	  Whether or not any allocations failed for this map.
	*/
	bool alloc_failure;
} snaky_arg_data;

/**
  Represents any kind of invalid value within SnakyScript.
*/
#define SNAKY_INVALID_VALUE -1

/**
  The different data types in SnakyScript.
*/
typedef enum snaky_data_type
{
	/**
	  Represents the character type.
	*/
	SNAKY_CHAR,
	/**
	  Represents the boolean type.
	*/
	SNAKY_BOOL,
	/**
	  Represents the integer type.
	*/
	SNAKY_INT,
	/**
	  Represents the float type.
	*/
	SNAKY_FLOAT,
	/**
	  Represents the double type.
	*/
	SNAKY_DOUBLE,
	/**
	  Represents the automatic type.

	  @note For this to work, the argument
	  being parsed must have a valid
	  data type attached to it. To provide
	  an argument with a data type, place
	  ":DATA_TYPE" directly after the name
	  of the argument. For example:
	  '<arg_name:int=30>.'
	*/
	SNAKY_AUTO
} snaky_data_type;

/**
  Searches the given string for a specific argument and tries to parse its value.

  Example strings and arguments:

  The string is: '<id=obj4,x=20,y=300>'

  The argument list will consist of 'id,' 'x,' and 'y.'

  The value list will consist of 'obj4,' '20,' and '300.'

  Arguments must be separated by the ',' character,
  and they must be assigned using the '=' character.

  @important All argument strings should be char arrays.

  @note Whitespace is allowed in argument strings. These
  two strings are equal when parsed: '<id=obj4,x=20,y=300>'
  and '< id = obj4, x = 20, y = 300 >'

  When attempting to place a nested argument string inside
  of an argument string, like this:

  '<arg_string=<...>>'

  The nested argument string MUST be surrounded by '\"'
  characters. So the proper way to write it would look
  like this:

  '<arg_string='<...>'>'

  Also, any arguments surrounded by the '\"' character are
  treated as strings and are copied exactly as they are typed.
  If any strings in the argument list contain nested argument
  lists, they are not parsed. Strings are skipped during the
  parsing stage.

  When parsing a target argument that contains a nested argument,
  you can use the '.' character to access a value in the nested
  argument. For example, with this string:

  '<player="<name=PLAYER1>">'

  The 'name' argument can be accessed using 'player.name' as the
  argument name. This only works for nested argument strings.
  Additionally, if multiple strings are nested, the names can
  be chained, like this 'arg1.arg2.arg2...'

  @param str The string to search.
  @param buffer Where to place the value of the found argument.
  @param buffer_size The size of 'buffer' in bytes.
  @param arg_name The name of the argument to search for.
  @param out_start_pos A pointer to a const char*. If a valid pointer
  is given, it will be equal to the position in the original
  string where the target argument's value was found. Example,
  in this argument string '<arg=ARG>' the out_start_pos pointer would
  point to the 'A' character after the '=.'
  @param out_data_type A pointer to a snaky_data_type. If a valid pointer
  is given, it will be equal to the data type of the parsed argument.

  @return 1 if the argument was successfully parsed, 0 if the
  function fails in any way.
*/
SNAKY_API int snaky_parse_target_arg(const char *str, char *buffer, size_t buffer_size, const char *arg_name, const char **out_start_pos, snaky_data_type *out_data_type);
/**
  Searches the given string for the very next argument and tries to parse its value.

  @note Because this function parses the next argument found, two buffers are required;
  one for the name of the argument parsed, as well as one for its value.

  To automatically walk a string and its arguments, pass a pointer into 'out_start_pos'
  and then on the next call use 'out_start_pos' as the argument string.

  @see snaky_parse_target_arg(const char*, char*, size_t, const char*, const char**)
*/
SNAKY_API int snaky_parse_arg(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos);

/**
  Searches the given string for a specific argument and removes it entirely.

  @important All argument strings should be char arrays.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_remove_arg(char *str, const char *arg_name);

/**
  Appends an argument to the given string.

  If the argument already exists in the string,
  its value will be modified instead.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_add_arg(char *str, size_t buffer_size, const char *arg_name, const char *new_arg_value);

/**
  Modifies an argument value directly in a string.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_set_arg(char *str, size_t buffer_size, const char *arg_name, const char *new_arg_value);

/**
  Counts the number of arguments within an argument string.
*/
SNAKY_API size_t snaky_count_args(const char *str);

/**
  Obtains argument data about an argument string.

  @important Do not initialize the argument data map.
  This function automatically initializes it and populates
  it with the necessary data. Later, you must use
  dynmaps_free_strkey(...) on the map to free its allocated
  memory.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_get_arg_data(const char *str, snaky_arg_data *data);

/**
  Reads a character argument value and obtains the actual
  char equivalent of it.

  @param str The string holding the character argument value. This is
  not an argument string.
  @param out_success A pointer to an int that indicates whether or
  not the function succeeded. It will be equal to 1 if it succeeded,
  and 0 if it failed.
*/
SNAKY_API char snaky_parse_char(const char *str, int *out_success);
/**
  Reads a boolean argument value and obtains
  the actual bool equivalent of it.

  @note The "OPPOSITE" argument value is not accepted
  here.

  @param str The string holding the boolean argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API bool snaky_parse_bool(const char *str, int *out_success);
/**
  Reads an integer argument value and obtains the
  actual int equivalent of it.

  @param str The string holding the integer argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates
  whether or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API int snaky_parse_int(const char *str, int *out_success);
/**
  Reads a float argument value and obtains the
  actual float equivalent of it.

  @param str The string holding the float argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API float snaky_parse_float(const char *str, int *out_success);
/**
  Reads a double argument value and obtains the
  actual double equivalent of it.

  @param str The string holding the double argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API double snaky_parse_double(const char *str, int *out_success);
/**
  Similarly to other snaky_parse_[type] functions, this function
  parses a generic value based on a target data type.

  @important Because this function is generic and
  needs to be able to determine if parsing failed or not,
  the 'out_success' pointer cannot be NULL.

  @param str The string holding the generic value. This is not
  an argument string.
  @param target_type The data type to try to parse
  the value as. For example, using SNAKY_DATA_TYPE_INT
  indicates the value should be parsed as an integer.
  Note that the SNAKY_AUTO type does not work in this
  function. SNAKY_AUTO only works in snaky_parse_target_arg_value(...).
  @param out_value A pointer to the actual variable
  that will hold the final parsed result.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.

  @see snaky_parse_target_arg_value(const char*, const char*, snaky_data_type, void*, const char**, int*)
*/
SNAKY_API void snaky_parse_value(const char *str, snaky_data_type target_type, void *out_value, int *out_success);
/**
  Parses a generic value just like snaky_parse_value(...) but obtains
  the value from an argument from an argument string.

  @note The SNAKY_AUTO data type works in this function.

  @see snaky_parse_value(const char*, snaky_data_type, void*, int*)
  @see snaky_parse_target_arg(const char*, char*, size_t, const char*, const char**)
*/
SNAKY_API void snaky_parse_target_arg_value(const char *str, const char *arg_name, snaky_data_type target_type, void *out_value, const char **out_start_pos, int *out_success);
