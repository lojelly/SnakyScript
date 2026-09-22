#define _USE_MATH_DEFINES

#include <math.h>
#include <ctype.h>
#include <string.h>
#include "vibrant_logs.h"
#include "dynamic_map_spellbook.h"
#include "snaky_script.h"

#define STRING_DELIM '"'

static bool init = false;

// map of constant names to constant values:
typedef struct constant_registry
{
	char **keys;
	float *values;
	size_t size, capacity;
	bool alloc_failure;
} constant_registry;
static constant_registry constants = {0};

// map of function names to generic pointers:
typedef struct func_registry
{
	char **keys;
	snaky_eval_func *values;
	size_t size, capacity;
	bool alloc_failure;
} func_registry;
static func_registry functions = {0};

static float factorial(float f)
{
	if(f == 0)
		return 1.0f;

	float ff = f;
	while(f > 1)
		ff *= (f -= 1);

	return ff;
}
static float rads(float f)
{
	return f * (M_PI / 180.0f);
}

int snaky_init()
{
	if(init)
	{
		vl_log(VL_ERROR, "Cannot re-initialize SnakyScript!\n");
		return 0;
	}

	dynmaps_init(&constants);
	if(constants.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for constants registry!\n");
		return 0;
	}

	dynmaps_init(&functions);
	if(functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for functions registry!\n");
		return 0;
	}

	// make 'init' true here since define_constant(...) will fail if it's false
	init = true;

	// define some constants
	if(!snaky_define_constant("E", M_E))
		return 0;
	if(!snaky_define_constant("PI", M_PI))
		return 0;
	if(!snaky_define_constant("INF", INFINITY))
		return 0;
	if(!snaky_define_constant("NAN", NAN))
		return 0;

	// define some functions
	if(!snaky_define_function("factorial", factorial))
		return 0;
	if(!snaky_define_function("sin", sinf))
		return 0;
	if(!snaky_define_function("cos", cosf))
		return 0;
	if(!snaky_define_function("tan", tanf))
		return 0;
	if(!snaky_define_function("sqrt", sqrtf))
		return 0;
	if(!snaky_define_function("rads", rads))
		return 0;
	if(!snaky_define_function("round", roundf))
		return 0;
	if(!snaky_define_function("floor", floorf))
		return 0;
	if(!snaky_define_function("ceil", ceilf))
		return 0;

	return 1;
}
bool snaky_is_init()
{
	return init;
}
int snaky_shutdown()
{
	if(!init)
	{
		vl_log(VL_ERROR, "SnakyScript was never initialized, cannot shutdown!\n");
		return 0;
	}

	dynmaps_free_strkey(&constants);
	dynmaps_free_strkey(&functions);

	return 1;
}

static int parse_target_arg(const char *args_start, char *buffer, size_t buffer_size, const char *arg_name, size_t arg_len, const char **out_start_pos, snaky_data_type *out_data_type)
{
	// default to invalid value
	if(out_data_type)
		*out_data_type = SNAKY_INVALID_VALUE;

	while(*args_start && *args_start != '=')
	{
		// while searching for the '=' try to also find the ':DATA_TYPE' string
		if(*args_start == ':')
		{
			// skip the ':'
			args_start++;

			// skip all whitespace after the ':'
			while(*args_start && *args_start == ' ')
				args_start++;

			// now find which data type the user provided
			if(strncmp(args_start, "char", 4) == 0)
			{
				if(out_data_type)
					*out_data_type = SNAKY_CHAR;
				args_start += 4;
			}
			else if(strncmp(args_start, "bool", 4) == 0)
			{
				if(out_data_type)
					*out_data_type = SNAKY_BOOL;
				args_start += 4;
			}
			else if(strncmp(args_start, "int", 3) == 0)
			{
				if(out_data_type)
					*out_data_type = SNAKY_INT;
				args_start += 3;
			}
			else if(strncmp(args_start, "float", 5) == 0)
			{
				if(out_data_type)
					*out_data_type = SNAKY_FLOAT;
				args_start += 5;
			}
			else if(strncmp(args_start, "double", 6) == 0)
			{
				if(out_data_type)
					*out_data_type = SNAKY_DOUBLE;
				args_start += 6;
			}
			else
			{
				vl_log(VL_ERROR, "Unknown data type: '%s'!\n", args_start);
				return 0;
			}
			continue;
		}

		args_start++;
	}

	if(!*args_start)
	{
		vl_log(VL_ERROR, "Expected '=' after argument: '%s'!\n", arg_name);
		return 0;
	}

	// go to the character after the '='
	args_start++;

	// skip all whitespace:
	while(*args_start && *args_start == ' ')
	{
		args_start++;
	}

	if(!*args_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
		return 0;
	}

	// if the start of the argument is an opening of another argument string, error out
	if(*args_start == '<')
	{
		vl_log(VL_ERROR, "Nested argument strings must be wrapped around opening and closing '\"'!\n");
		return 0;
	}

	// if start pos queried, return it
	if(out_start_pos)
		*out_start_pos = args_start;

	// see if the arg value is a string
	bool in_string = *args_start == STRING_DELIM;

	// determine if a nested argument or string is given
	if(in_string)
	{
		const char *start_of_str = args_start;

		// skip the string delimiter
		args_start++;

		// copy everything in the string exactly as is
		size_t i = 0;
		while(buffer && buffer_size > 0 && *args_start && *args_start != STRING_DELIM && i + 1 < buffer_size)
			buffer[i++] = *args_start++;

		if(!*args_start)
		{
			vl_log(VL_ERROR, "Expected closing '\"' after string: '%s'!\n", start_of_str);
			return 0;
		}

		if(buffer)
			buffer[i] = '\0';

		return 1;
	}

	// if not a nested string, copy the arg value normally
	size_t i = 0;
	while(buffer && buffer_size > 0 && *args_start && *args_start != '>' && *args_start != ',' && i + 1 < buffer_size)
		buffer[i++] = *args_start++;

	if(buffer)
		buffer[i] = '\0';

	return 1;
}
int snaky_parse_target_arg(const char *str, char *buffer, size_t buffer_size, const char *arg_name, const char **out_start_pos, snaky_data_type *out_data_type)
{
	size_t arg_len = arg_name ? strlen(arg_name) : 0;

	if(!str || strlen(str) == 0 || !arg_name || arg_len == 0)
		return 0;

	bool in_top_most_level = false;

	// see if user is trying to find a nested argument:

	// the resolved arg name is the final argument name after the last '.' character in the original argument name string
	const char *resolved_arg_name = arg_name;
	size_t resolved_arg_len = 0;
	size_t last_nested_arg_pos = 0;
	int i = 0;
	for(const char *p = arg_name; *p; ++p)
	{
		if(*p == '.')
			last_nested_arg_pos = i;
		i++;
	}

	resolved_arg_name = last_nested_arg_pos > 0 ? arg_name + last_nested_arg_pos + 1 : arg_name;
	if(!*resolved_arg_name)
	{
		vl_log(VL_ERROR, "Unexpected termination of argument name: '%s'!\n", arg_name);
		return 0;
	}
	resolved_arg_len = strlen(resolved_arg_name);

	// see if resolved arg name is invalid (starts with number of symbol)
	if(!isalpha(*resolved_arg_name))
	{
		vl_log(VL_ERROR, "Argument name cannot start with a symbol or number: '%s'!\n", resolved_arg_name);
		return 0;
	}

	for(const char *p = str; *p && *p != '>'; ++p)
	{
		// get char
		char c = *p;

		// if the first char is a string delimiter, skip the string entirely (unless user is searching for nested argument)
		if(c == STRING_DELIM)
		{
			const char *start_of_str = p;

			// skip the first string delimiter
			p++;

			while(*p && *p != STRING_DELIM)
			{
				if(last_nested_arg_pos > 0)
				{
					const char *args_start = p + 1;

					while(*args_start && *args_start == ' ')
						args_start++;

					if(!*args_start)
					{
						vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
						return 0;
					}

					if(strncmp(args_start, resolved_arg_name, resolved_arg_len) == 0)
						return parse_target_arg(args_start, buffer, buffer_size, resolved_arg_name, resolved_arg_len, out_start_pos, out_data_type);
				}
				++p;
			}

			if(!*p)
			{
				vl_log(VL_ERROR, "Expected closing '\"' after string: '%s'!\n", start_of_str);
				return 0;
			}

			continue;
		}

		// when the first '<' or ',' is encountered, compare arg to the target arg
		if(c == '<' && !in_top_most_level)
		{
			// now the parser is in the top-most level
			in_top_most_level = true;

			// compare the argument with the target argument
			const char *args_start = p + 1;

			while(*args_start && *args_start == ' ')
				args_start++;

			if(!*args_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
				return 0;
			}

			if(strncmp(args_start, arg_name, arg_len) == 0)
				return parse_target_arg(args_start, buffer, buffer_size, arg_name, arg_len, out_start_pos, out_data_type);

			continue;
		}

		// parse all subsequent arguments
		if(c == ',' && in_top_most_level)
		{
			// same logic as above
			const char *args_start = p + 1;

			// skip whitespace
			while(*args_start && *args_start == ' ')
				args_start++;

			if(!*args_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
				return 0;
			}

			if(strncmp(args_start, arg_name, arg_len) == 0)
				return parse_target_arg(args_start, buffer, buffer_size, arg_name, arg_len, out_start_pos, out_data_type);

			continue;
		}

		// if a nested argument string is found, skip it
		if(c == '<' && in_top_most_level)
		{
			vl_log(VL_ERROR, "Nested arguments must be wrapped in opening and closing '\"'\n");
			return 0;
		}
	}

	return 0;
}
static int parse_arg(const char *args_start, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos)
{
	// 'args_start' points to the first char in the arg name
	size_t i = 0;
	while(*args_start && *args_start != ' ' && *args_start != '=' && i + 1 < name_buffer_size)
		name_buffer[i++] = *args_start++;

	name_buffer[i] = '\0';

	// find the '=' for this arg name
	while(*args_start && *args_start != '=')
		args_start++;

	// was a '=' ever found?
	if(!*args_start)
	{
		vl_log(VL_ERROR, "Expected '=' after argument: '%s'!\n", name_buffer);
		return 0;
	}

	// if the start of the argument is an opening of another argument string, error out
	if(*args_start == '<')
	{
		vl_log(VL_ERROR, "Nested argument strings must be wrapped around opening and closing '\"'!\n");
		return 0;
	}

	// if '=' was found, skip it
	args_start++;

	// skip all whitespace:
	while(*args_start && *args_start == ' ')
		args_start++;

	if(!*args_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
		return 0;
	}

	// if user queried arg start pos, return it
	if(out_start_pos)
		*out_start_pos = args_start;

	// see if the arg value is a string
	bool in_string = *args_start == STRING_DELIM;

	// if inside a string, keep adding values until the matching string delimiter is found
	if(in_string)
	{
		const char *start_of_str = args_start;

		// skip the string delimiter
		args_start++;

		// copy everything in the string exactly as is
		i = 0;
		while(*args_start && *args_start != STRING_DELIM && i + 1 < value_buffer_size)
			value_buffer[i++] = *args_start++;

		if(!*args_start)
		{
			vl_log(VL_ERROR, "Expected closing '\"' after string: '%s'!\n", start_of_str);
			return 0;
		}

		value_buffer[i] = '\0';

		return 1;
	}

	// if not a nested string, copy the arg value normally
	i = 0;
	while(*args_start && *args_start != '>' && *args_start != ',' && i + 1 < value_buffer_size)
		value_buffer[i++] = *args_start++;

	value_buffer[i] = '\0';

	return 1;
}
int snaky_parse_arg(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos)
{
	if(!str || strlen(str) == 0 || !name_buffer || name_buffer_size == 0 || !value_buffer || value_buffer_size == 0)
		return 0;

	bool in_top_most_level = false;

	for(const char *p = str; *p && *p != '>'; ++p)
	{
		// get char
		char c = *p;

		// if the first char is a string delimiter, skip the string entirely
		if(c == STRING_DELIM)
		{
			const char *start_of_str = p;

			// skip the first string delimiter
			p++;

			while(*p && *p != STRING_DELIM)
				++p;

			if(!*p)
			{
				vl_log(VL_ERROR, "Expected closing '\"' after string: '%s'!\n", start_of_str);
				return 0;
			}

			continue;
		}

		// when the first '<' or ',' is encountered, see what the very next arg is
		if((c == '<' || c == ',') && !in_top_most_level)
		{
			in_top_most_level = true;

			const char *args_start = p + 1;

			while(*args_start && *args_start == ' ')
				args_start++;

			if(!*args_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
				return 0;
			}

			return parse_arg(args_start, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos);
		}

		// parse the very next arg found
		if(c == ',' && in_top_most_level)
		{
			const char *args_start = p + 1;

			while(*args_start && *args_start == ' ')
				args_start++;

			if(*args_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of argument string at '%c'!\n", *(args_start - 1));
				return 0;
			}

			return parse_arg(args_start, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos);
		}

		// nested arguments are skipped
		if(c == '<' && in_top_most_level)
		{
			while(*p && *p != '>')
				++p;

			if(!*p)
			{
				vl_log(VL_ERROR, "Expected '>' in nested argument string: '%s'!\n", str);
				return 0;
			}

			continue;
		}
	}

	return 0;
}

int snaky_remove_arg(char *str, const char *arg_name)
{
	size_t arg_name_len = arg_name ? strlen(arg_name) : 0;

	if(!str || strlen(str) == 0 || !arg_name || arg_name_len == 0)
		return 0;

	// see if the arg is found
	const char *start_pos = NULL;
	char arg_value[SNAKY_BUF_SIZE + 1];
	if(!snaky_parse_target_arg(str, arg_value, sizeof(arg_value), arg_name, &start_pos, NULL))
	{
		vl_log(VL_ERROR, "The '%s' argument was not found in this string: '%s'!\n", arg_name, str);
		return 0;
	}

	// the arg is present in the string so it can be removed:

	/*
	   the start_pos pointer points to the first character of the arg value,
	   so it can be moved backwards to obtain the total length of the sub-string

	   + 2 for the '=' and argument separator
	*/
	size_t sub_str_len = strlen(arg_value) + arg_name_len + 2;

	// now sub_str_len represents the total amount of characters that need to be removed

	// now get a pointer to the first character separating this argument
	char *sub_str_start_pos = (char*) (start_pos - arg_name_len - 1);

	memmove(sub_str_start_pos, sub_str_start_pos + sub_str_len, strlen(sub_str_start_pos + sub_str_len) + 1);

	// see if that was the last argument in the string, and if so, remove trailing ',' and replace with '>'
	size_t len = strlen(str);

	if(len > 0 && str[len - 1] == ',')
		str[len - 1] = '>';

	return 1;
}

int snaky_add_arg(char *str, size_t buffer_size, const char *arg_name, const char *new_arg_value)
{
	if(!str || strlen(str) == 0 || buffer_size == 0 || strlen(str) >= buffer_size || !arg_name || strlen(arg_name) == 0 || !new_arg_value || strlen(new_arg_value) == 0)
		return 0;

	// see if the arg is already present in the string
	char arg_value[SNAKY_BUF_SIZE + 1];
	if(snaky_parse_target_arg(str, arg_value, sizeof(arg_value), arg_name, NULL, NULL))
		// if so, just edit the argument value in the string
		return snaky_set_arg(str, buffer_size, arg_name, new_arg_value);

	/*
	   otherwise, go to end of arg list and append the new argument:

	   the end of the arg list should look like this:

	   '...arg_name=arg_value>'
	*/

	// go to the next '>' found
	while(*str && *str != '>')
		str++;

	if(*str != '>')
	{
		vl_log(VL_ERROR, "Expected '>' to terminate argument string: '%s'!\n", str);
		return 0;
	}

	/*
	   if string already contains at least one arg, append ',' to start another arg

	   if the user passes something like "<>" then the '>' is replaced with '\0' and
	   the new arg is appended like normal
	*/
	if(snaky_count_args(str) > 0)
	{
		*str = ',';
		*++str = '\0';
	}
	else
		*str = '\0';

	// append ',arg_name=new_arg_value' directly at end
	size_t i = 0;
	while(i < strlen(arg_name) && strlen(str) < buffer_size)
	{
		// immediately append the next char in arg_name
		*str++ = *(arg_name + (i++));
		// to make strlen(...) work the string must be null-terminated
		*str = '\0';
	}

	// append the '='
	if(strlen(str) < buffer_size)
		*str++ = '=';
	else
		return 0;

	i = 0;
	while(i < strlen(new_arg_value) && strlen(str) < buffer_size)
	{
		// append the next char in new_arg_value
		*str++ = *(new_arg_value + (i++));
		// to make strlen(...) work the string must be null-terminated
		*str = '\0';
	}

	// close the arg string
	if(strlen(str) < buffer_size)
	{
		*str++ = '>';
		*str = '\0';
	}
	else
		return 0;

	return 1;
}

static void insert_char_str(char *buffer, char c)
{
	// move all chars at pos one slot over to the right
	memmove(buffer + 1, buffer, strlen(buffer) + 1);
	*(buffer++) = c;
}
int snaky_set_arg(char *str, size_t buffer_size, const char *arg_name, const char *new_arg_value)
{
	if(!str || strlen(str) == 0 || strlen(str) >= buffer_size || buffer_size == 0 || !arg_name || strlen(arg_name) == 0 || !new_arg_value || strlen(new_arg_value) == 0)
		return 0;

	char *edit_pos = NULL;
	size_t edit_len = 0;
	int curr_len = -1;
	bool arg_found = false;
	const char *start_pos = NULL;

	char arg_value[SNAKY_BUF_SIZE + 1];
	if(snaky_parse_target_arg(str, arg_value, sizeof(arg_value), arg_name, &start_pos, NULL))
	{
		edit_pos = (char*) start_pos;
		curr_len = strlen(arg_value);
		arg_found = true;
	}

	// see if current value is a bool and user passed "OPPOSITE"
	if(strcmp(new_arg_value, "OPPOSITE") == 0)
	{
		if(strcmp(arg_value, "TRUE") == 0)
			new_arg_value = "FALSE";
		else if(strcmp(arg_value, "ON") == 0)
			new_arg_value = "OFF";
		else if(strcmp(arg_value, "FALSE") == 0)
			new_arg_value = "TRUE";
		else if(strcmp(arg_value, "OFF") == 0)
			new_arg_value = "ON";
		else
		{
			vl_log(VL_ERROR, "The 'OPPOSITE' arg value can only be used on boolean arguments!\n");
			return 0;
		}
	}

	// if the arg wasn't found, it must be added
	if(arg_found)
		edit_len = strlen(new_arg_value);
	else
		edit_len = strlen(arg_name) + strlen(new_arg_value) + 2;

	if(edit_len >= buffer_size || strlen(str) + edit_len >= buffer_size)
	{
		vl_log(VL_ERROR, "The given buffer is not large enough to fit the new argument string: current size = %zu, required size = %zu\n", buffer_size, strlen(str) + edit_len);
		return 0;
	}

	if(!arg_found)
	{
		// add ',arg_name=new_arg_value' to string:

		// walk forward in string until the next '>' is hit
		while(*str && *str != '>')
			str++;

		// insert ',' there
		insert_char_str(str++, ',');

		// now insert arg name
		for(size_t i = 0; i < strlen(arg_name); ++i)
			insert_char_str(str++, *(arg_name + i));

		// now insert the '='
		insert_char_str(str++, '=');

		// now insert the new arg value
		for(size_t i = 0; i < strlen(new_arg_value); ++i)
			insert_char_str(str++, *(new_arg_value + i));
	}
	else
	{
		// modify arg value directly within string:

		size_t new_len = strlen(new_arg_value);

		// first shift everything occupying that space to the right
		memmove(edit_pos + new_len, edit_pos + curr_len, strlen(edit_pos + curr_len) + 1);

		// then copy the new arg value into the new space
		memcpy(edit_pos, new_arg_value, new_len);
	}

	return 1;
}
int snaky_set_args(char *str, size_t buffer_size, const char *args)
{
	if(!str || strlen(str) == 0 || strlen(str) >= buffer_size || buffer_size == 0 || !args || strlen(args) == 0)
		return 0;

	// because 'args' should be its own argument string, get the arg data from it
	snaky_arg_data args_data = {0};
	if(snaky_get_arg_data(args, &args_data))
	{
		// go through the map
		for(size_t i = 0; i < args_data.size; ++i)
		{
			// get the arg name and new value
			const char *name = args_data.keys[i];
			const char *val = args_data.values[i];

			// set it
			if(!snaky_set_arg(str, buffer_size, name, val))
				return 0;
		}

		dynmaps_free_strkeyval(&args_data);
		return 1;
	}

	return 0;
}

size_t snaky_count_args(const char *str)
{
	if(!str || strlen(str) == 0)
		return 0;

	size_t count = 0;

	for(const char *p = str; *p && *p != '>'; ++p)
	{
		char c = *p;

		// an arg is counted when the 'name=value' format is found exactly
		char name[SNAKY_BUF_SIZE + 1];
		char value[SNAKY_BUF_SIZE + 1];
		if(snaky_parse_arg(p, name, sizeof(name), value, sizeof(value), NULL))
			count++;
	}

	return count;
}

int snaky_get_arg_data(const char *str, snaky_arg_data *data)
{
	if(!str || strlen(str) == 0 || !data)
		return 0;

	// init map
	dynmaps_init(data);
	if(data->alloc_failure)
		return 0;

	const char *start_pos = str;
	char name[SNAKY_BUF_SIZE + 1];
	char value[SNAKY_BUF_SIZE + 1];

	while(snaky_parse_arg(start_pos, name, sizeof(name), value, sizeof(value), &start_pos))
	{
		dynmaps_set_strkeyval(data, name, value);
		if(data->alloc_failure)
			return 0;
	}

	return 1;
}

char snaky_parse_char(const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) != 1)
		return '\0';

	char c = *str;

	if(out_success)
		*out_success = 1;

	return c;
}
bool snaky_parse_bool(const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return false;

	if(strcmp(str, "TRUE") == 0 || strcmp(str, "ON") == 0)
	{
		if(out_success)
			*out_success = 1;
		return true;
	}
	else if(strcmp(str, "FALSE") == 0 || strcmp(str, "OFF") == 0)
	{
		if(out_success)
			*out_success = 1;
		return false;
	}
	else
		return false;
}

static char peek_next_char(const char **str)
{
	while(**str && **str == ' ')
		(*str)++;
	return **str;
}
static char get_next_char(const char **str)
{
	while(**str && **str == ' ')
		(*str)++;
	return *(*str)++;
}
static float parse_term(const char*, const char**, int*);
static float parse_expression(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_term(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;
	while(peek_next_char(str) == '+' || peek_next_char(str) == '-')
	{
		char op = get_next_char(str);
		float next_val = parse_term(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(op == '+')
			f += next_val;
		else
			f -= next_val;
	}
	return f;
}
static float parse_factor(const char *origin, const char **str, int *out_success)
{
	// find expression wrapped around '()'
	char c = peek_next_char(str);
	if(c == '(')
	{
		char next = get_next_char(str);
		int s = 0;
		float f = parse_expression(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(peek_next_char(str) == ')')
		{
			get_next_char(str);
			*out_success = 1;
			return f;
		}
		else
			return 0.0f;
	}

	// see if it's just a normal number
	if(isdigit(c) || c == '-' || c == '.')
	{
		char *end = NULL;
		float f = strtof(*str, &end);
		if(end == *str)
		{
			*out_success = 0;
			return 0.0f;
		}
		*str = end;
		*out_success = 1;
		return f;
	}

	// see if user provided a function or arg name
	if(isalpha(c))
	{
		char arg_name[SNAKY_BUF_SIZE + 1];
		size_t i = 0;
		while(**str && (isalpha(**str) || **str == '_') && i + 1 < sizeof(arg_name))
			arg_name[i++] = *(*str)++;

		arg_name[i] = '\0';

		// see what the value is:

		// try to parse as a target argument:
		char arg[SNAKY_BUF_SIZE + 1];
		if(snaky_parse_target_arg(origin, arg, sizeof(arg), arg_name, NULL, NULL))
		{
			float f = 0.0f;
			int s = 0;
			snaky_parse_value(origin, arg, SNAKY_FLOAT, &f, &s);
			*out_success = s;
			if(s == 1)
				return f;
		}

		// parse all matching constants:
		for(size_t i = 0; i < constants.size; ++i)
		{
			if(strcmp(arg_name, constants.keys[i]) == 0)
			{
				*out_success = 1;
				return constants.values[i];
			}
		}

		// parse all matching functions:
		for(size_t i = 0; i < functions.size; ++i)
		{
			if(strcmp(arg_name, functions.keys[i]) == 0)
			{
				if(peek_next_char(str) == '(')
				{
					char next = get_next_char(str);
					int s = 0;
					float f = parse_expression(origin, str, &s);
					*out_success = s;
					if(s == 0)
						return 0.0f;
					if(peek_next_char(str) == ')')
					{
						get_next_char(str);
						*out_success = 1;
						return functions.values[i](f);
					}
					else
					{
						vl_log(VL_ERROR, "Expected ')' after function: '%s'!\n", functions.keys[i]);
						return 0.0f;
					}
				}
			}
		}
	}

	*out_success = 0;
	return 0.0f;
}
static float parse_exponent(const char*, const char**, int*);
static float parse_term(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_exponent(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;

	while(peek_next_char(str) == '*' || peek_next_char(str) == '/')
	{
		char op = get_next_char(str);
		float next_val = parse_exponent(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(op == '*')
			f *= next_val;
		else
		{
			if(next_val == 0)
			{
				vl_log(VL_WARNING, "Dividing by 0 is undefined. Returning 0.0f.\n");
				return 0.0f;
			}
			f /= next_val;
		}

		*out_success = 1;
	}

	return f;
}
static float parse_exponent(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_factor(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;
	
	if(peek_next_char(str) == '^')
	{
		get_next_char(str);

		float exp = parse_exponent(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		f = powf(f, exp);
	}

	*out_success = 1;
	return f;
}
int snaky_parse_int(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	int i = strtol(str, &endptr, 10);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return i;
	}

	return 0;
}
float snaky_parse_float(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0.0f;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	float f = strtof(str, &endptr);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return f;
	}

	return 0.0f;
}
double snaky_parse_double(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0.0;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	double d = strtod(str, &endptr);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return d;
	}

	return 0.0;
}
void snaky_parse_value(const char *origin, const char *str, snaky_data_type target_type, void *out_value, int *out_success)
{
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it's 0 by default
	*out_success = 0;

	switch(target_type)
	{
		case SNAKY_CHAR:
			char c = snaky_parse_char(str, out_success);
			if(*out_success == 1 && out_value)
				*((char*) out_value) = c;
			break;
		case SNAKY_BOOL:
			bool b = snaky_parse_bool(str, out_success);
			if(*out_success == 1 && out_value)
				*((bool*) out_value) = b;
			break;
		case SNAKY_INT:
			int i = snaky_parse_int(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((int*) out_value) = i;
			break;
		case SNAKY_FLOAT:
			float f = snaky_parse_float(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((float*) out_value) = f;
			break;
		case SNAKY_DOUBLE:
			double d = snaky_parse_double(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((double*) out_value) = d;
			break;
		default:
			vl_log(VL_ERROR, "Invalid data type in snaky_parse_value(...): %d\n", target_type);
			break;
	}

	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse target string: '%s'!\n", str);
}
void snaky_parse_target_arg_value(const char *str, const char *arg_name, snaky_data_type target_type, void *out_value, const char **out_start_pos, int *out_success)
{
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it is 0 by default
	*out_success = 0;

	char arg[SNAKY_BUF_SIZE + 1];
	snaky_data_type resolved_type = SNAKY_INVALID_VALUE;
	if(snaky_parse_target_arg(str, arg, sizeof(arg), arg_name, out_start_pos, &resolved_type))
	{
		if(resolved_type == SNAKY_INVALID_VALUE)
			resolved_type = target_type;
		snaky_parse_value(str, arg, resolved_type, out_value, out_success);
	}

	// TODO should these functions emit these error messages? or leave that up to the user?
	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse target arg value: string: '%s', argument name: '%s'!\n", str, arg_name);
}

// storage of object templates:
typedef struct obj_template
{
	char str[512];
	char name[64];
	size_t size;
} obj_template;
typedef struct obj_template_map
{
	char **keys;
	obj_template *values;
	size_t size, capacity;
	bool alloc_failure;
} obj_template_map;

static obj_template_map obj_templates = {0};

int snaky_create_object_template(const char *name, const char *str, size_t size)
{
	// see if the map needs to be initialized
	if(obj_templates.size == 0)
	{
		dynmaps_init(&obj_templates);
		if(obj_templates.alloc_failure)
		{
			vl_log(VL_ERROR, "Failed to create object template map!\n");
			return 0;
		}
	}

	// create template
	obj_template temp = {0};
	snprintf(temp.name, sizeof(temp.name), "%s", name);
	snprintf(temp.str, sizeof(temp.str), "%s", str);
	temp.size = size;

	// save template info in map
	dynmaps_set_strkey(&obj_templates, name, temp);
	if(obj_templates.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for object template!\n");
		return 0;
	}

	vl_log(VL_SUCCESS, "Object template successfully created: '%s'!\n", name);
	return 1;
}
int snaky_destroy_object_template(const char *name)
{
	int result = -1;
	dynmaps_remove_strkey_result(&obj_templates, name, result);

	if(result == -1)
	{
		vl_log(VL_ERROR, "No object template exists with the name '%s'!\n", name);
		return 0;
	}

	vl_log(VL_SUCCESS, "Object template '%s' successfully destroyed!\n", name);

	// free the entire map if the number of objects is 0
	if(obj_templates.size == 0)
	{
		vl_log(VL_INFO, "No more object templates exist; freeing all associated memory now!\n");
		dynmaps_free_strkey(&obj_templates);
	}

	return 1;
}
int snaky_create_object(const char *name, char *buffer, size_t buffer_size)
{
	// try to find an object template with the given name
	obj_template *temp = NULL;
	dynmaps_get_strkey(&obj_templates, name, temp);

	if(temp)
	{
		snprintf(buffer, buffer_size, "%s", temp->str);
		return 1;
	}

	vl_log(VL_ERROR, "Failed to create object instance from object template: '%s'!\n", name);
	return 0;
}

int snaky_read_file(const char *file_path, char *buffer, size_t buffer_size)
{
	if(!file_path || strlen(file_path) == 0 || !buffer || buffer_size == 0)
		return 0;

	FILE *f = fopen(file_path, "r");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
		return 0;
	}

	// make sure buffer is valid:
	*buffer = '\0';

	char read[SNAKY_MAX_LINE_LEN];
	size_t total_size = 0;
	while(fgets(read, sizeof(read), f))
	{
		total_size += strlen(read);

		if(total_size >= buffer_size)
		{
			vl_log(VL_ERROR, "Not enough memory allocated for reading file's contents: '%s'!\n", file_path);
			return 0;
		}

		// append contents to user's buffer
		if(!strcat(buffer, read))
		{
			vl_log(VL_ERROR, "Failed to append strings while reading file's contents: '%s'!\n", file_path);
			return 0;
		}
	}

	fclose(f);

	return 1;
}
long snaky_get_file_size(const char *file_path)
{
	if(!file_path || strlen(file_path) == 0)
		return 0;

	FILE *f = fopen(file_path, "r");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at '%s'!\n", file_path);
		return 0;
	}

	fseek(f, 0, SEEK_END);

	long bytes = ftell(f);

	fclose(f);

	return bytes;
}
int snaky_get_next_line(char **cursor, char *buffer, size_t buffer_size)
{
	if(!cursor || !*cursor || strlen(*cursor) == 0 || !buffer || buffer_size == 0)
		return 0;

	// walk until a '\n' is found
	size_t i = 0;
	while(**cursor && **cursor != '\n' && i + 1 < buffer_size)
		buffer[i++] = *(*cursor)++;

	buffer[i] = '\0';

	if(**cursor == '\n')
		(*cursor)++;

	return 1;
}

int snaky_define_constant(const char *str, float value)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot define a constant without initializing SnakyScript!\n");
		return 0;
	}

	dynmaps_set_strkey(&constants, str, value);
	if(constants.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to define constant: '%s'. An allocation error occurred!\n", str);
		return 0;
	}

	return 1;
}
int snaky_define_function(const char *str, snaky_eval_func func)
{
	if(!func)
	{
		vl_log(VL_ERROR, "Cannot define a NULL function!\n");
		return 0;
	}

	dynmaps_set_strkey(&functions, str, func);
	if(functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to define function: '%s'. An allocation error occurred!\n", str);
		return 0;
	}

	return 1;
}
