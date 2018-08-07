#include "modules/System.h"

namespace RedScript::Modules
{
/* sys module object */
Runtime::ModuleRef SystemModule;

System::System() : Runtime::ModuleObject("sys")
{

}
}
