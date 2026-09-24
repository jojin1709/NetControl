#include "NetworkProfileMonitor.h"
#include <windows.h>
#include <netlistmgr.h>
#pragma comment(lib, "ole32.lib")

NetworkProfile NetworkProfileMonitor::query() const {
    NetworkProfile p = NetworkProfile::Unknown;
    INetworkListManager* nlm = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nlm));
    if (FAILED(hr) || !nlm) return p;
    IEnumNetworks* en = nullptr;
    if (SUCCEEDED(nlm->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &en)) && en) {
        INetwork* net = nullptr;
        ULONG fetched = 0;
        if (en->Next(1, &net, &fetched) == S_OK && net) {
            NLM_NETWORK_CATEGORY cat = NLM_NETWORK_CATEGORY_PUBLIC;
            net->GetCategory(&cat);
            if (cat == NLM_NETWORK_CATEGORY_PRIVATE) p = NetworkProfile::Private;
            else if (cat == NLM_NETWORK_CATEGORY_PUBLIC) p = NetworkProfile::Public;
            else if (cat == NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED) p = NetworkProfile::Domain;
            net->Release();
        }
        en->Release();
    }
    nlm->Release();
    return p;
}

bool NetworkProfileMonitor::isMetered() const {
    INetworkListManager* nlm = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&nlm))) || !nlm)
        return false;
    bool metered = false;
    INetworkCostManager* cost = nullptr;
    if (SUCCEEDED(nlm->QueryInterface(IID_PPV_ARGS(&cost))) && cost) {
        DWORD dwCost = 0;
        NLM_SOCKADDR sa{};
        if (SUCCEEDED(cost->GetCost(&dwCost, &sa))) {
            metered = (dwCost & NLM_CONNECTION_COST_OVERDATALIMIT) != 0 ||
                      (dwCost & NLM_CONNECTION_COST_CONGESTED) != 0 ||
                      (dwCost & NLM_CONNECTION_COST_ROAMING) != 0;
        }
        cost->Release();
    }
    nlm->Release();
    return metered;
}

bool NetworkProfileMonitor::isPublic() const {
    return query() == NetworkProfile::Public;
}

bool NetworkProfileMonitor::isBlockAllowed(bool blockOnlyPublic, bool gateEnabled) const {
    if (!gateEnabled) return true;
    if (!blockOnlyPublic) return true;
    return isPublic();
}
