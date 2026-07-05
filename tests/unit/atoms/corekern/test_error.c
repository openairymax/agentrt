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
    assert(AGENTRT_SUCCESS == 0);
    assert(AGENTRT_EINVAL > 0);
    assert(AGENTRT_ENOMEM > 0);
    assert(AGENTRT_EIO > 0);
    
    /* ﮔﭖﻟﺁﻠﻟﺁﺁﻛﭨ۲ﻝ ﮒﺁﻛﺕﮔ?*/
    assert(AGENTRT_SUCCESS != AGENTRT_EINVAL);
    assert(AGENTRT_EINVAL != AGENTRT_ENOMEM);
    assert(AGENTRT_ENOMEM != AGENTRT_EIO);
    
    return 0;
}

/**
 * @brief ﮔﭖﻟﺁﻠﻟﺁﺁﮒ­ﻝ؛۵ﻛﺕﺎﻟﺛ؛ﮔ?
 * @return 0ﻟ۰۷ﻝ۳ﭦﮔﮒﺅﺙﻠ0ﻟ۰۷ﻝ۳ﭦﮒ۳ﺎﻟﺑ۴
 */
int test_error_strings(void) {
    printf("  ﮔﭖﻟﺁﻠﻟﺁﺁﮒ­ﻝ؛۵ﻛﺕﺎﻟﺛ؛ﮔ?..\n");
    
    /* ﮔﭖﻟﺁﮒﺓﺎﻝ۴ﻠﻟﺁﺁﻛﭨ۲ﻝ ﻝﮒ­ﻝ؛۵ﻛﺕﺎﻟ۰۷ﻝ۳ﭦ */
    const char* success_str = agentrt_error_string(AGENTRT_SUCCESS);
    assert(success_str != NULL);
    assert(strstr(success_str, "ﮔﮒ") != NULL || strstr(success_str, "Success") != NULL);
    
    const char* einval_str = agentrt_error_string(AGENTRT_EINVAL);
    assert(einval_str != NULL);
    assert(strlen(einval_str) > 0);
    
    const char* enomem_str = agentrt_error_string(AGENTRT_ENOMEM);
    assert(enomem_str != NULL);
    assert(strlen(enomem_str) > 0);
    
    const char* eio_str = agentrt_error_string(AGENTRT_EIO);
    assert(eio_str != NULL);
    assert(strlen(eio_str) > 0);
    
    /* ﮔﭖﻟﺁﮔ۹ﻝ۴ﻠﻟﺁﺁﻛﭨ۲ﻝ  */
    const char* unknown_str = agentrt_error_string(9999);
    assert(unknown_str != NULL);
    assert(strstr(unknown_str, "ﮔ۹ﻝ۴") != NULL || strstr(unknown_str, "Unknown") != NULL);
    
    return 0;
}