#include <string.h>
#include "vibrant_logs.h"
#include "dynamic_map_spellbook.h"
#include "snaky_script.h"

static int parse_target_arg(const char *args_start, char *buffer, size_t buffer_size, const char *arg_name, size_t arg_len, const char **start_pos)
{
	// go to where the '=' should be
	const char *equals = args_start + arg_len;

	if(*equals != '=')
	{
		vl_log(VL_ERROR, "Expected '=' after argument: '%s'!\n", arg_name);
		return 0;
	}

	// go to the character after the '='
	equals++;

	// if start pos queried, return it
	if(start_pos)
		*start_pos = equals;

	// determine if this is a nested argument
	if(*equals == '<')
	{
		// put the entire nested argument into the buffer
		size_t i = 0;
		while(*equals && *equals != '>' && i + 1 < buffer_size)
			buffer[i++] = *equals++;

		// ensure the argument string ended properly
		if(*equals != '>')
		{
			vl_log(VL_ERROR, "Expected '>' in nested argument string: '%s'!\n", args_start);
			return 0;
		}

		// copy the closing '>' and '\0'
		if(i + 1 >= buffer_size)
		{
			vl_log(VL_ERROR, "The given buffer is not large enough (size=%zu) to fit the argument value of '%s'!\n", buffer_size, arg_name);
			return 0;
		}

		buffer[i++] = *equals++;
		buffer[i] = '\0';

		return 1;
	}

	// if not a nested string, copy the arg value normally
	size_t i = 0;
	while(*equals && *equals != '>' && *equals != ',' && i + 1 < buffer_size)
		buffer[i++] = *equals++;

	buffer[i] = '\0';

	return 1;
}
int snaky_parse_target_arg(const char *str, char *buffer, size_t buffer_size, const char *arg_name, const char **start_pos)
{
	size_t arg_len = arg_name ? strlen(arg_name) : 0;

	if(!str || strlen(str) == 0 || !buffer || buffer_size == 0 || !arg_name || arg_len == 0)
		return 0;

	bool in_top_most_level = false;

	for(const char *p = str; *p && *p != '>'; ++p)
	{
		// get char
		char c = *p;

		// when the first '<' or ',' is encountered, compare arg to the target arg
		if(c == '<' && !in_top_most_level)
		{
			// now the parser is in the top-most level
			in_top_most_level = true;

			// compare the argument with the target argument
			const char *args_start = p + 1;

			if(strncmp(args_start, arg_name, arg_len) == 0)
				return parse_target_arg(args_start, buffer, buffer_size, arg_name, arg_len, start_pos);

			continue;
		}

		// parse all subsequent arguments
		if(c == ',' && in_top_most_level)
		{
			// same logic as above
			const char *args_start = p + 1;

			if(strncmp(args_start, arg_name, arg_len) == 0)
				return parse_target_arg(args_start, buffer, buffer_size, arg_name, arg_len, start_pos);

			continue;
		}

		// if a nested argument string is found, skip it
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
static int parse_arg(const char *args_start, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **start_pos)
{
	// 'args_start' points to the first char in the arg name
	size_t i = 0;
	while(i + 1 < name_buffer_size && *args_start != '=')
		name_buffer[i++] = *args_start++;

	name_buffer[i] = '\0';

	// now args_start should be at the '=,' skip it
	args_start++;

	// if user queried arg start pos, return it
	if(start_pos)
		*start_pos = args_start;

	// now args_start points to the first char in the arg value
	i = 0;
	while(i + 1 < value_buffer_size && *args_start != '>' && *args_start != ',')
		value_buffer[i++] = *args_start++;

	value_buffer[i] = '\0';

	return 1;
}
int snaky_parse_arg(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **start_pos)
{
	if(!str || strlen(str) == 0 || !name_buffer || name_buffer_size == 0 || !value_buffer || value_buffer_size == 0)
		return 0;

	bool in_top_most_level = false;

	for(const char *p = str; *p && *p != '>'; ++p)
	{
		// get char
		char c = *p;

		// when the first '<' or ',' is encountered, see what the very next arg is
		if((c == '<' || c == ',') && !in_top_most_level)
		{
			in_top_most_level = true;

			return parse_arg(p + 1, name_buffer, name_buffer_size, value_buffer, value_buffer_size, start_pos);
		}

		// parse the very next arg found
		if(c == ',' && in_top_most_level)
			return parse_arg(p + 1, name_buffer, name_buffer_size, value_buffer, value_buffer_size, start_pos);

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

bool snaky_parse_bool(const char *str, int *out_success)
{
	if(!str || strlen(str) == 0)
	{
		if(out_success)
			*out_success = false;
		return false;
	}
	
	if(strcmp(str, "TRUE") == 0)
	{
		if(out_success)
			*out_success = true;
		return true;
	}
	else if(strcmp(str, "FALSE") == 0)
	{
		if(out_success)
			*out_success = true;
		return false;
	}
	else
	{
		if(out_success)
			*out_success = false;
		return false;
	}
}

int snaky_remove_arg(char *str, const char *arg_name)
{
	size_t arg_name_len = arg_name ? strlen(arg_name) : 0;

	if(!str || strlen(str) == 0 || !arg_name || arg_name_len == 0)
		return 0;

	// see if the arg is found
	const char *start_pos = NULL;
	char arg_value[SNAKY_BUF_SIZE + 1];
	if(!snaky_parse_target_arg(str, arg_value, sizeof(arg_value), arg_name, &start_pos))
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
	if(snaky_parse_target_arg(str, arg_value, sizeof(arg_value), arg_name, NULL))
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

	// find the arg in the string:
	char *p = str;

	char *edit_pos = NULL;
	size_t edit_len = 0;
	int curr_len = -1;
	bool arg_found = false;
	const char *start_pos = NULL;

	char arg_value[SNAKY_BUF_SIZE + 1];
	for(const char *t = p; *t; ++t)
	{
		if(snaky_parse_target_arg(t, arg_value, sizeof(arg_value), arg_name, &start_pos))
		{
			edit_pos = (char*) start_pos;
			curr_len = strlen(arg_value);
			arg_found = true;
			break;
		}
	}

	// see if current value is a bool and user passed "OPPOSITE"
	if(strcmp(new_arg_value, "OPPOSITE") == 0)
	{
		if(strcmp(arg_value, "TRUE") == 0)
			new_arg_value = "FALSE";
		else if(strcmp(arg_value, "FALSE") == 0)
			new_arg_value = "TRUE";
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
		vl_log(VL_DEBUG, "parsed name and value: '%s'='%s'!\n", name, value);
		dynmaps_set_strkeyval(data, name, value);
		if(data->alloc_failure)
			return 0;
	}

	return 1;
}
