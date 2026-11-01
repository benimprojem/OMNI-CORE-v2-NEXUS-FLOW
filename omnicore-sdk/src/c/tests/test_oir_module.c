#include "oir_module.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void) {
    uint8_t payload[64] = {0};
    OirModuleInput input;
    OirModuleView view;
    size_t written = 0;
    memset(&input, 0, sizeof(input));
    assert(oir_module_write(payload, sizeof(payload), &input, &written) == OIR_STATUS_OK);
    assert(written == OIR_MODULE_HEADER_SIZE);
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_OK);
    assert(view.type_count == 0 && view.function_count == 0);
    assert(view.instruction_count == 0 && view.strings_length == 0);
    payload[4] = 1;
    assert(oir_module_open(payload, written, &view) == OIR_STATUS_UNSUPPORTED_VERSION);
    return 0;
}
