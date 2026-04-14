/**
 * @file test_error.c
 * @brief ﻠﻟﺁﺁﮒ۳ﻝﮒﮒﮔﭖﻟﺁ
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/**
 * @brief ﮔﭖﻟﺁﮒﭦﮔ؛ﻠﻟﺁﺁﻛﭨ۲ﻝ 
 * @return 0ﻟ۰۷ﻝ۳ﭦﮔﮒﺅﺙﻠ0ﻟ۰۷ﻝ۳ﭦﮒ۳ﺎﻟﺑ۴
 */
int test_error_basic(void) {
    printf("  ﮔﭖﻟﺁﮒﭦﮔ؛ﻠﻟﺁﺁﻛﭨ۲ﻝ ...\n");
    
    /* ﮔﭖﻟﺁﻠﻟﺁﺁﻛﭨ۲ﻝ ﻟﮒﺑ */
    assert(AGENTOS_SUCCESS == 0);
    assert(AGENTOS_EINVAL > 0);
    assert(AGENTOS_ENOMEM > 0);
    assert(AGENTOS_EIO > 0);
    
    /* ﮔﭖﻟﺁﻠﻟﺁﺁﻛﭨ۲ﻝ ﮒﺁﻛﺕﮔ?*/
    assert(AGENTOS_SUCCESS != AGENTOS_EINVAL);
    assert(AGENTOS_EINVAL != AGENTOS_ENOMEM);
    assert(AGENTOS_ENOMEM != AGENTOS_EIO);
    
    return 0;
}

/**
 * @brief ﮔﭖﻟﺁﻠﻟﺁﺁﮒ­ﻝ؛۵ﻛﺕﺎﻟﺛ؛ﮔ?
 * @return 0ﻟ۰۷ﻝ۳ﭦﮔﮒﺅﺙﻠ0ﻟ۰۷ﻝ۳ﭦﮒ۳ﺎﻟﺑ۴
 */
int test_error_strings(void) {
    printf("  ﮔﭖﻟﺁﻠﻟﺁﺁﮒ­ﻝ؛۵ﻛﺕﺎﻟﺛ؛ﮔ?..\n");
    
    /* ﮔﭖﻟﺁﮒﺓﺎﻝ۴ﻠﻟﺁﺁﻛﭨ۲ﻝ ﻝﮒ­ﻝ؛۵ﻛﺕﺎﻟ۰۷ﻝ۳ﭦ */
    const char* success_str = agentos_error_string(AGENTOS_SUCCESS);
    assert(success_str != NULL);
    assert(strstr(success_str, "ﮔﮒ") != NULL || strstr(success_str, "Success") != NULL);
    
    const char* einval_str = agentos_error_string(AGENTOS_EINVAL);
    assert(einval_str != NULL);
    assert(strlen(einval_str) > 0);
    
    const char* enomem_str = agentos_error_string(AGENTOS_ENOMEM);
    assert(enomem_str != NULL);
    assert(strlen(enomem_str) > 0);
    
    const char* eio_str = agentos_error_string(AGENTOS_EIO);
    assert(eio_str != NULL);
    assert(strlen(eio_str) > 0);
    
    /* ﮔﭖﻟﺁﮔ۹ﻝ۴ﻠﻟﺁﺁﻛﭨ۲ﻝ  */
    const char* unknown_str = agentos_error_string(9999);
    assert(unknown_str != NULL);
    assert(strstr(unknown_str, "ﮔ۹ﻝ۴") != NULL || strstr(unknown_str, "Unknown") != NULL);
    
    return 0;
}