#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_modem_dce_service.h"
#include "esp_modem.h"

/**
 * @brief Create and initialize EXS82W object
 *
 * @param dte Modem DTE object
 * @return modem_dce_t* Modem DCE object
 */
modem_dce_t *exs82w_init(modem_dte_t *dte);

#ifdef __cplusplus
}
#endif
