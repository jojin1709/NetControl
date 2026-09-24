#pragma once
#include "Models.h"

class NetworkProfileMonitor {
public:
    NetworkProfile query() const;
    bool isMetered() const;
    bool isPublic() const;
    bool isBlockAllowed(bool blockOnlyPublic, bool gateEnabled) const;
};
