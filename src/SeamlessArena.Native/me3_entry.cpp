// ME3 resolves this exported C ABI symbol from the profile's initializer field.
// Keep startup lightweight until the game adapter and version checks exist.
extern "C" __declspec(dllexport) bool SeamlessArena_Initialize() noexcept
{
    return true;
}
