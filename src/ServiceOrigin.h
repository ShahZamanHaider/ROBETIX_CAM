#pragma once
#include "ServiceConfig.h"
// Preview overrides contain only a public hostname, never a Vercel access credential.
#ifdef ROBETIX_PREVIEW_BUILD
#include "PreviewService.h"
#else
#define ROBETIX_SERVICE_ORIGIN ROBETIX_CUSTOMER_ORIGIN
#endif
namespace ServiceOrigin {
constexpr const char* url = ROBETIX_SERVICE_ORIGIN;
constexpr const char* customer = ROBETIX_CUSTOMER_ORIGIN;
#ifdef ROBETIX_PREVIEW_BUILD
constexpr const char* mode = "preview";
#else
constexpr const char* mode = "production";
#endif
}
