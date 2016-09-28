#include <base/component.h>
#include <base/log.h>

Genode::size_t Component::stack_size() { return 64*1024; }

void Component::construct(Genode::Env &env)
{
	Genode::log ("Hello GPU!");
}
