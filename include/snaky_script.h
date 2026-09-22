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

#include <stdio.h>
#include <stddef.h>
#include <string.h>

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
  Define SNAKY_CUSTOM_MAX_LINE_LEN to change the
  default maximum line length of SnakyScript files.
*/
#ifdef SNAKY_CUSTOM_MAX_LINE_LEN
	#define SNAKY_MAX_LINE_LEN SNAKY_CUSTOM_MAX_LINE_LEN
#else
	/**
	  The default maximum line length in a SnakyScript file.
	*/
	#define SNAKY_MAX_LINE_LEN 256
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
  Initializes the SnakyScript library.

  @important Make sure you call snaky_shutdown()
  later!

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_init(void);
/**
  Determines if SnakyScript has been initialized.
*/
SNAKY_API bool snaky_is_init(void);
/**
  Shuts down and frees any memory allocated
  by SnakyScript.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_shutdown(void);

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
  Modifies multiple argument values directly in a string.

  To set multiple arguments, the 'args' string must be formatted
  like this:

  "<arg_name=new_arg_value,arg_name2=new_arg_value,...>"

  For example, using this argument string:

  "<x=100,y=200>"

  To set both the 'x' and 'y' arguments at the same time, the function
  would be called like this:

  'snaky_set_args(str, sizeof(str), "<x=300,y=150>");'

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_set_args(char *str, size_t buffer_size, const char *args);

/**
  Counts the number of arguments within an argument string.
*/
SNAKY_API size_t snaky_count_args(const char *str);

/**
  Obtains argument data about an argument string.

  @important Do not initialize the argument data map.
  This function automatically initializes it and populates
  it with the necessary data. Later, you must use
  dynmaps_free_strkeyval(...) on the map to free its allocated
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

  @note Valid boolean arguments include: 'TRUE,' 'ON,' 'FALSE,' and 'OFF.'

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

  @param origin The start of the whole argument string.
  @param str The string holding the integer argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates
  whether or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API int snaky_parse_int(const char *origin, const char *str, int *out_success);
/**
  Reads a float argument value and obtains the
  actual float equivalent of it.

  @param origin The start of the whole argument string.
  @param str The string holding the float argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API float snaky_parse_float(const char *origin, const char *str, int *out_success);
/**
  Reads a double argument value and obtains the
  actual double equivalent of it.

  @param origin The start of the whole argument string.
  @param str The string holding the double argument value. This is not
  an argument string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API double snaky_parse_double(const char *origin, const char *str, int *out_success);
/**
  Similarly to other snaky_parse_[type] functions, this function
  parses a generic value based on a target data type.

  @important Because this function is generic and
  needs to be able to determine if parsing failed or not,
  the 'out_success' pointer cannot be NULL.

  @param origin The start of the whole argument string. This used
  for resolving argument names in expressions throughout the string.
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
SNAKY_API void snaky_parse_value(const char *origin, const char *str, snaky_data_type target_type, void *out_value, int *out_success);
/**
  Parses a generic value just like snaky_parse_value(...) but obtains
  the value from an argument from an argument string.

  @note The SNAKY_AUTO data type works in this function.

  @see snaky_parse_value(const char*, snaky_data_type, void*, int*)
  @see snaky_parse_target_arg(const char*, char*, size_t, const char*, const char**)
*/
SNAKY_API void snaky_parse_target_arg_value(const char *str, const char *arg_name, snaky_data_type target_type, void *out_value, const char **out_start_pos, int *out_success);

/**
  Creates an object template.

  Object templates allow for argument strings to be easily copied
  and re-implemented for future instances.

  @param name The name of the object template.
  @param str The string to use as the template.
  @param size The size of the string in bytes.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_create_object_template(const char *name, const char *str, size_t size);
/**
  Destroys and removes an object template.

  @important You must call this for every object template you create.
  When the last object template is destroyed, the internal memory
  associated with object templates is freed.

  @param name The name of the object template to destroy.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_destroy_object_template(const char *name);
/**
  Creates an instance of an object template using the
  name of an existing object template.

  @param name The name of the object template to create an instance of.
  @param buffer Where to put the cloned string.
  @param buffer_size The size of 'buffer' in bytes.

  @see snaky_create_object_template(const char*, const char*, size_t)

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_create_object(const char *name, char *buffer, size_t buffer_size);

/**
  Opens a file for reading SnakyScript from.

  @param file_path The file path.
  @param buffer Where the file's contents should be placed.
  @param buffer_size The size of 'buffer' in bytes.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_read_file(const char *file_path, char *buffer, size_t buffer_size);
/**
  Obtains the size of a file in bytes.
*/
SNAKY_API long snaky_get_file_size(const char *file_path);
/**
  Obtains the next line in a buffer that contains the lines from
  a read file.

  @param cursor A pointer to the string containing ALL of the file's lines.
  This is called the 'cursor' because as the file is read, this pointer
  is moved forward. Because of this, if you are using malloc() to create
  the char*, do not use that pointer here. Instead create another pointer and
  use that. Moving the malloc()-ed pointer forward will lead to undefined
  behavior.
  @param buffer Where the next line should be placed.
  @param buffer_size The size of 'buffer' in bytes.

  @return 1 on success, 0 on failure.

  @see snaky_read_file(const char*, char*, size_t)
*/
SNAKY_API int snaky_get_next_line(char **cursor, char *buffer, size_t buffer_size);

/**
  Single-argument functions that are evaluated in argument
  strings.

  They should take in a single float value, and return a
  single float value.
*/
typedef float (*snaky_eval_func) (float);

/**
  Registers a constant that can be used in argument strings later.

  Constants are single string values, such as 'PI' and should
  be equal to a single value.

  @note It is recommended to make all constants uppercase
  to keep them clearly separated from other values or functions.

  @param str The constant to define.
  @param value The value of the constant.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_define_constant(const char *str, float value);
/**
  Registers a function that can be used in argument strings later.

  Functions are single string values, such as 'sin' and should
  take in a single value, and return a single result.

  When using functions in argument strings, you must use the
  function name, then an opening '(' followed by the value, and then a closing
  ').'

  @param str The function to define.
  @param func A pointer to the function to use.

  @return 1 on success, 0 on failure.

  @see snaky_eval_func
*/
SNAKY_API int snaky_define_function(const char *str, snaky_eval_func func);
