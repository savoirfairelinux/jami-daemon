# Compile
To compile the agent, one has to enable it while configuring the daemon.

```sh
./configure --enable-agent
```

then you need to recompile the contrib.  This will compile Guile, which can take
some time.

# Running the agent
The agent expects a Scheme file has its first parameter.  This scheme file will
be interpreted by Guile.  In the script, you can control the agent.

Usage:
```sh
./agent ./examples/passive-agent.scm
```

# Guile bindings
In order for Guile to control the agent, bindings have to be added to the global
environment where the configuration file is being interpreted.  This is done in
`main.cpp` in the function `install_scheme_primitive()`.  All scheme bindings
should have the prefix `agent:` to be clear that the procedure is one that
control the agent.

When a binding is called from Guile, the arguments passed are Scheme objects of
type `SCM`.  This is an opaque type that is generic.  In order to be clear on
what the underlying type needed by the primitive procedure is, one should add the
suffix of the type at the end.

For example, `my_primitive_procedure()` expects that `some_variable_str`
will be of type `string`.  This is enforced by using an assertion:
```c++
static SCM my_primitive_procedure(SCM some_variable_str)
{
   AGENT_ASSERT(scm_is_string(some_variable_str), "`some_variable_str` must be of type string");
   ...
}
```

Here is another example where `my_second_primitive()` expects that
`some_variable_vector_or_str` to be of type `string` or `vector`:
```c++
static SCM my_second_primitive(SCM some_variable_vector_or_str)
{
   AGENT_ASSERT(scm_is_string(some_variable_vector_or_str) ||
                scm_is_simple_vector(some_variable_vector_or_str),
                "`scm_some_variable_vector_or_str` must either be of type vector or string");
  ...
}
```

# Writing scenarios
See `examples/`
