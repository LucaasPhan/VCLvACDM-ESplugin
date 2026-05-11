#pragma once

#define CURL_STATICLIB 1
#include <curl/curl.h>
#include <json/json.h>

#include <list>
#include <mutex>
#include <string>
#include <chrono>
#include <set>

#include "types/Pilot.h"

namespace vacdm::com {
class Server {
   public:
    typedef struct ServerConfiguration_t {
        std::string name = "";
        bool allowMasterInSweatbox = false;
        bool allowMasterAsObserver = false;
    } ServerConfiguration;

    typedef struct AirportMetadata_t {
        std::string icao = "";
        std::string status = "FULL";
        std::string master = "";
        bool readOnly = false;
        bool lvo = false;
        std::vector<std::string> activeDelays;
    } AirportMetadata;

   private:
    Server();
    struct Communication {
        std::mutex lock;
        CURL* socket;

        Communication() : lock(), socket(curl_easy_init()) {}
    };

    std::string m_apiKey;
    std::string m_cid;
    Communication m_getRequest;
    Communication m_postRequest;
    Communication m_patchRequest;
    Communication m_deleteRequest;

    bool m_apiIsChecked;
    bool m_apiIsValid;
    bool m_backendOnline;
    bool m_lastPilotFetchOk;
    std::string m_baseUrl;
    std::set<std::string> m_masterAirports;
    std::set<std::string> m_supportedAirports;
    std::map<std::string, AirportMetadata> m_airportMetadata;
    std::string m_errorCode;
    ServerConfiguration m_serverConfiguration;
    mutable std::mutex m_stateLock;

   public:
    ~Server();
    Server(const Server&) = delete;
    Server(Server&&) = delete;

    Server& operator=(const Server&) = delete;
    Server& operator=(Server&&) = delete;

    static Server& instance();

    void changeServerAddress(const std::string& url);
    void setApiKey(const std::string& apiKey);
    void setCid(const std::string& cid);
    bool checkWebApi();
    bool backendOnline() const;
    bool lastPilotFetchOk() const;
    ServerConfiguration_t getServerConfig();
    std::list<types::Pilot> getPilots(const std::list<std::string> airports);
    void postPilot(types::Pilot);
    void patchPilot(const Json::Value& root);
    
    void refreshAirportMetadata(const std::string& icao);
    void refreshSupportedAirports();
    AirportMetadata getAirportMetadata(const std::string& icao);
    bool isReadOnlyAirport(const std::string& icao);
    bool isSupportedAirport(const std::string& icao);

    /// @brief Sends a post message to the specififed endpoint url with the root as content
    /// @param endpointUrl endpoint url to send the request to
    /// @param root message content
    void sendPostMessage(const std::string& endpointUrl, const Json::Value& root);

    /// @brief Sends a patch message to the specified endpoint url with the root as content
    /// @param endpointUrl endpoint url to send the request to
    /// @param root message content
    void sendPatchMessage(const std::string& endpointUrl, const Json::Value& root);
    void sendDeleteMessage(const std::string& endpointUrl);

    void updateExot(const std::string& pilot, const std::chrono::utc_clock::time_point& exot);
    void updateTobt(const types::Pilot& pilot, const std::chrono::utc_clock::time_point& tobt, bool manualTobt);
    void updateAsat(const std::string& callsign, const std::chrono::utc_clock::time_point& asat);
    void updateAsrt(const std::string& callsign, const std::chrono::utc_clock::time_point& asrt);
    void updateAobt(const std::string& callsign, const std::chrono::utc_clock::time_point& aobt);
    void updateAort(const std::string& callsign, const std::chrono::utc_clock::time_point& aort);
    void updateTsac(const std::string& callsign, const std::chrono::utc_clock::time_point& tsac);

    void resetTobt(const std::string& callsign, const std::chrono::utc_clock::time_point& tobt,
                   const std::string& tobtState);
    void deletePilot(const std::string& callsign);

    void toggleLvo(const std::string& icao, bool active);
    void postDelay(const std::string& icao, const std::string& runway, const std::string& type, const std::string& time);

    const std::string& errorMessage() const;
    void claimMaster(const std::string& icao, const std::string& cid, const std::string& name);
    void releaseMaster(const std::string& icao, const std::string& cid);
    void releaseAllMasters(const std::string& cid = "");
    bool isMaster(const std::string& icao);
    void sendHeartbeats(const std::string& cid);
    std::set<std::string> getMasterAirports();
};
}  // namespace vacdm::com
