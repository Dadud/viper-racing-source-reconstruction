#include "viper/pcar_control_update.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main()
{
    viper::PcarCallbackGlobals globals{};
    globals.value_0x5093f4 = 0.5F;
    globals.value_0x509320 = 0.4F;
    globals.value_0x5093b8 = 0.2F;
    globals.value_0x5093c8 = 0.5F;
    viper::PcarScalarFields fields{};

    viper::update_pcar_callback_scalar_prefix(fields, globals, 2.0F);
    if (std::abs(fields.value_0xde0 - 1.0F) > 1.0e-6F ||
        std::abs(fields.value_0xde4 - 0.4F) > 1.0e-6F ||
        std::abs(fields.value_0xde8 - 0.2F) > 1.0e-6F ||
        std::abs(fields.value_0xdec - 0.5F) > 1.0e-6F) {
        std::cerr << "PCAR scalar prefix did not preserve the observed field mapping.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
