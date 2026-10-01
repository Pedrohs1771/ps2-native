#pragma once
namespace ps2native::ee_aot
{
    class Dispatcher;
    // Null in the diagnostic profile; selection is fixed at build time.
    const Dispatcher *compiledDispatcher();
}
