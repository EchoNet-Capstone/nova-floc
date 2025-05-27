#pragma once

#define SETTER_NEW(y) new_ ## y
#define SETTER_SIG(x, y) void set_ ## y(x SETTER_NEW(y))
#define GETTER_SIG(x, y) x get_ ## y(void)

/**
 * For (`var_type`, `var_name`), this becomes:
 * `void set_var_name(var_type new_var_name);` and
 * `var_type get_var_name(void);`
 */
#define GET_SET_FUNC_PROTO(x, y) SETTER_SIG(x, y);\
                             GETTER_SIG(x, y);

/**
 * For (`var_type`, `var_name`, `var_inital_value`), this becomes:
 * `var_type var_name = var_inital_value` and
 * `void set_var_name(var_type new_var_name){ var_name = new_var_name ;}` and
 * `var_type get_var_name(void){ return var_name; }`
 */
#define GET_SET_FUNC_DEF(x, y, z)  x y = z; \
                            SETTER_SIG(x, y){\
                            y = SETTER_NEW(y);\
                            }\
                            GETTER_SIG(x, y){\
                            return y;\
                            }
