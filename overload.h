/* -*- c -*- */

#ifndef __OVERLOAD_H__
#define __OVERLOAD_H__

#include "tuples.h"
#include "builtins.h"
#include "macros.h"

#define MAX_FUNCTION_LENGTH     63
#define MAX_BINDING_LENGTH      63

typedef struct _binding_t
{
    int is_bound;
    int value;

    struct _binding_t *next;
} binding_t;

typedef struct _named_binding_t
{
    char name[MAX_BINDING_LENGTH + 1];
    binding_t *binding;

    struct _named_binding_t *next;
} named_binding_t;

typedef struct _overload_arg_t
{
    binding_t *tag;
    binding_t *length;

    struct _overload_arg_t *next;
} overload_arg_t;

#define OVERLOAD_BUILTIN     1
#define OVERLOAD_MACRO       2

typedef struct _overload_entry_t
{
    char name[MAX_FUNCTION_LENGTH + 1];
    int type;
    overload_arg_t *result;
    overload_arg_t *args;
    int num_args;
    union
    {
	struct
	{
	    int sidefx;
	    builtin_function_t builtin;
	    generator_function_t generator;
	} builtin;
	macro_function_t macro;
    } v;

    struct _overload_entry_t *next;
} overload_entry_t;

typedef struct _function_arg_info_t
{
    tuple_info_t info;

    struct _function_arg_info_t *next;
} function_arg_info_t;

binding_t* new_free_variable_binding (void);
binding_t* new_constant_binding (int value);
overload_arg_t* new_overload_argument (binding_t *tag, binding_t *length, overload_arg_t *next);

void clear_bindings (void);
binding_t* free_binding_with_name (const char *name);

void register_overloaded_builtin (const char *name, const char *argstring, int sidefx,
				  builtin_function_t func, generator_function_t gen);
void register_overloaded_macro (const char *name, const char *argstring, macro_function_t func);

overload_entry_t* overloaded_builtin_with_function (builtin_function_t function);

overload_entry_t* resolve_function_call (const char *name, function_arg_info_t *args, tuple_info_t *result);

#endif
