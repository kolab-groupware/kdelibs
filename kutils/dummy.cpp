#include <kcmoduleproxy.h>
#include <kprintpreview.h>
#include <kemoticons/kemoticons.h>
#include <kidletime/kidletime.h>

Q_GLOBAL_STATIC_WITH_ARGS( KCModuleProxy, foo, ( 0 ) )
Q_GLOBAL_STATIC_WITH_ARGS( KPrintPreview, bar, ( 0, 0 ) )
Q_GLOBAL_STATIC( KEmoticons, foobar )
Q_GLOBAL_STATIC( KIdleTime*, baz )