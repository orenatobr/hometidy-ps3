// libcurl implementation of http::Client (console build; the host links it only for the network smoke test).
#pragma once

#include <memory>
#include <string>

#include "http.h"

namespace http {

/// Args:
///     caFile: Path of a PEM file with the root certificates to trust. Certificates are always verified.
///     userAgent: Sent with every request (GitHub requires one).
std::unique_ptr<Client> makeCurlClient(const std::string& caFile, const std::string& userAgent);

}  // namespace http
