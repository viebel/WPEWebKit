/* GStreamer OpenCDM decryptor
 *
 * Copyright (C) 2016-2017 TATA ELXSI
 * Copyright (C) 2016-2017 Metrological
 * Copyright (C) 2016-2017 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "config.h"
#include "CDMOpenCDM.h"

#if ENABLE(ENCRYPTED_MEDIA) && USE(OPENCDM)

#include "CDMPrivate.h"
#include "inspector/InspectorValues.h"
#include "MediaKeyMessageType.h"
#include "MediaKeyStatus.h"
#include "MediaKeysRequirement.h"
#include "WebKitClearKeyDecryptorGStreamer.h"
#include "WebKitOpenCDMPlayReadyDecryptorGStreamer.h"
#include "WebKitOpenCDMWidevineDecryptorGStreamer.h"
#include <open_cdm.h>
#include <wtf/text/Base64.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_media_opencdm_decrypt_debug_category);
#define GST_CAT_DEFAULT webkit_media_opencdm_decrypt_debug_category

using namespace Inspector;

namespace WebCore {

class CDMPrivateOpenCDM : public CDMPrivate {

    String m_openCdmKeySystem;
    static std::unique_ptr<media::OpenCdm> s_openCdm;

public:

    CDMPrivateOpenCDM(const String&);
    virtual ~CDMPrivateOpenCDM();

    bool supportsInitDataType(const AtomicString&) const override;
    bool supportsConfiguration(const MediaKeySystemConfiguration&) const override;
    bool supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const override;
    bool supportsSessionTypeWithConfiguration(MediaKeySessionType&, const MediaKeySystemConfiguration&) const override;
    bool supportsRobustness(const String&) const override;
    MediaKeysRequirement distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const override;
    MediaKeysRequirement persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const override;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) const override;
    RefPtr<CDMInstance> createInstance() override;
    void loadAndInitialize() override;
    bool supportsServerCertificates() const override;
    bool supportsSessions() const override;
    bool supportsInitData(const AtomicString&, const SharedBuffer&) const override;
    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const override;
    std::optional<String> sanitizeSessionId(const String&) const override;
    static media::OpenCdm* getOpenCdmInstance();
};

std::unique_ptr<media::OpenCdm> CDMPrivateOpenCDM::s_openCdm;

class CDMInstanceOpenCDM : public CDMInstance {
public:
    CDMInstanceOpenCDM(media::OpenCdm*, const String&);
    virtual ~CDMInstanceOpenCDM();

    ImplementationType implementationType() const final { return  ImplementationType::OpenCDM; }
    SuccessValue initializeWithConfiguration(const MediaKeySystemConfiguration&) override;
    SuccessValue setDistinctiveIdentifiersAllowed(bool) override;
    SuccessValue setPersistentStateAllowed(bool) override;
    SuccessValue setServerCertificate(Ref<SharedBuffer>&&) override;

    void requestLicense(LicenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback) override;
    void updateLicense(const String&, LicenseType, const SharedBuffer&, LicenseUpdateCallback) override;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback) override;
    void closeSession(const String&, CloseSessionCallback) override;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback) override;
    void storeRecordOfKeyUsage(const String&) override;

    void gatherAvailableKeys(AvailableKeysCallback) override;

    const String& keySystem() const override { return m_keySystem; }

private:
    MediaKeyStatus getKeyStatus(std::string &);
    SessionLoadFailure getSessionLoadStatus(std::string &);
    size_t checkMessageLength(std::string &, std::string &);
    media::OpenCdm* m_openCdmSession;
    HashMap<String, Ref<SharedBuffer>> sessionIdMap;
    String m_keySystem;
};

CDMPrivateOpenCDM::CDMPrivateOpenCDM(const String& keySystem)
    : m_openCdmKeySystem(keySystem)
{
}

CDMPrivateOpenCDM::~CDMPrivateOpenCDM() = default;

bool CDMPrivateOpenCDM::supportsInitDataType(const AtomicString& initDataType) const
{
    return equalLettersIgnoringASCIICase(initDataType, "cenc");
}

bool CDMPrivateOpenCDM::supportsConfiguration(const MediaKeySystemConfiguration& config) const
{
    for (auto& audioCapability : config.audioCapabilities) {
        if (!getOpenCdmInstance()->IsTypeSupported(m_openCdmKeySystem.utf8().data(), audioCapability.contentType.utf8().data()))
            return false;
    }
    for (auto& videoCapability : config.videoCapabilities) {
        if (!getOpenCdmInstance()->IsTypeSupported(m_openCdmKeySystem.utf8().data(), videoCapability.contentType.utf8().data()))
            return false;
    }
    return true;
}

bool CDMPrivateOpenCDM::supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration& config, const MediaKeysRestrictions&) const
{
    return supportsConfiguration(config);
}

bool CDMPrivateOpenCDM::supportsSessionTypeWithConfiguration(MediaKeySessionType&, const MediaKeySystemConfiguration& config) const
{
    return supportsConfiguration(config);
}

bool CDMPrivateOpenCDM::supportsRobustness(const String&) const
{
    return false;
}

media::OpenCdm* CDMPrivateOpenCDM::getOpenCdmInstance()
{
    if (!s_openCdm)
        s_openCdm = std::make_unique<media::OpenCdm>();
    return s_openCdm.get();
}

MediaKeysRequirement CDMPrivateOpenCDM::distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    return MediaKeysRequirement::Optional;
}

MediaKeysRequirement CDMPrivateOpenCDM::persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    return MediaKeysRequirement::Optional;
}

bool CDMPrivateOpenCDM::distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) const
{
    return false;
}

RefPtr<CDMInstance> CDMPrivateOpenCDM::createInstance()
{
    getOpenCdmInstance()->SelectKeySystem(m_openCdmKeySystem.utf8().data());
    return adoptRef(new CDMInstanceOpenCDM(getOpenCdmInstance(), m_openCdmKeySystem));
}

void CDMPrivateOpenCDM::loadAndInitialize()
{
}

bool CDMPrivateOpenCDM::supportsServerCertificates() const
{
    return true;
}

bool CDMPrivateOpenCDM::supportsSessions() const
{
    return true;
}

bool CDMPrivateOpenCDM::supportsInitData(const AtomicString& initDataType, const SharedBuffer&) const
{
    return equalLettersIgnoringASCIICase(initDataType, "cenc");
}

RefPtr<SharedBuffer> CDMPrivateOpenCDM::sanitizeResponse(const SharedBuffer& response) const
{
    return response.copy();
}

std::optional<String> CDMPrivateOpenCDM::sanitizeSessionId(const String& sessionId) const
{
    return sessionId;
}

CDMFactoryOpenCDM::CDMFactoryOpenCDM() = default;
CDMFactoryOpenCDM::~CDMFactoryOpenCDM() = default;

std::unique_ptr<CDMPrivate> CDMFactoryOpenCDM::createCDM(CDM&, const String& keySystem)
{
    return std::unique_ptr<CDMPrivate>(new CDMPrivateOpenCDM(keySystem));
}

bool CDMFactoryOpenCDM::supportsKeySystem(const String& keySystem)
{
    return equalLettersIgnoringASCIICase(keySystem, PLAYREADY_PROTECTION_SYSTEM_ID)
    || equalLettersIgnoringASCIICase(keySystem, PLAYREADY_YT_PROTECTION_SYSTEM_ID)
    || equalLettersIgnoringASCIICase(keySystem, WIDEVINE_PROTECTION_SYSTEM_ID);
}

CDMInstanceOpenCDM::CDMInstanceOpenCDM(media::OpenCdm* session, const String& keySystem)
    : m_openCdmSession(session)
    , m_keySystem(keySystem)
{
}
CDMInstanceOpenCDM::~CDMInstanceOpenCDM() = default;

CDMInstance::SuccessValue CDMInstanceOpenCDM::initializeWithConfiguration(const MediaKeySystemConfiguration&)
{
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceOpenCDM::setDistinctiveIdentifiersAllowed(bool)
{
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceOpenCDM::setPersistentStateAllowed(bool)
{
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceOpenCDM::setServerCertificate(Ref<SharedBuffer>&& certificate)
{
    CDMInstance::SuccessValue ret = WebCore::CDMInstance::SuccessValue::Failed;
    if (m_openCdmSession->SetServerCertificate(reinterpret_cast<unsigned char*>(const_cast<char*>(certificate->data())), certificate->size()))
        ret = WebCore::CDMInstance::SuccessValue::Succeeded;
    return ret;
}

void CDMInstanceOpenCDM::requestLicense(LicenseType licenseType, const AtomicString&, Ref<SharedBuffer>&& initData, LicenseCallback callback)
{   
    std::string sessionId;
    String sessionIdValue;
    String mimeType = "video/x-h264";
    if (equalLettersIgnoringASCIICase(m_keySystem, "com.microsoft.playready")
        || equalLettersIgnoringASCIICase(m_keySystem, "com.youtube.playready"))
        mimeType = "video/x-h264";
    else if (equalLettersIgnoringASCIICase(m_keySystem, "com.widevine.alpha"))
        mimeType = "video/mp4";
    m_openCdmSession->CreateSession(mimeType.utf8().data(), reinterpret_cast<unsigned char*>(const_cast<char*>(initData->data())),
        initData->size(), sessionId, (int)licenseType);
    if (!sessionId.size()) {
        callback(WTFMove(initData), sessionIdValue, false, Failed);
        return;
    }
    sessionIdValue = String::fromUTF8(sessionId.c_str());

    unsigned char temporaryUrl[1024] = {'\0'};
    std::string message;
    int messageLength = 0;
    int destinationUrlLength = 0;
    int returnValue = m_openCdmSession->GetKeyMessage(message,
        &messageLength, temporaryUrl, &destinationUrlLength);
    if (returnValue || !messageLength || !destinationUrlLength) {
        callback(WTFMove(initData), sessionIdValue, false, Failed);
        return;
    }
    bool needIndividualization = false;
    std::string delimiter = ":Type:";
    std::string requestType = message.substr(0, message.find(delimiter));
    size_t previousMessageSize = message.size();
    if (requestType.size() && (requestType.size() !=  message.size()))
        message.erase(0, message.find(delimiter) + delimiter.length());
    GST_TRACE("message.size before erase = %u, message.size after erase = %u, delimiter.size = %u, delimiter = %s, requestType.size = %u, requestType = %s", previousMessageSize, message.size(), delimiter.size(), delimiter.c_str(), requestType.size(), requestType.c_str());
    if ((requestType.size() == 1) && ((WebCore::MediaKeyMessageType)std::stoi(requestType) == CDMInstance::MessageType::IndividualizationRequest))
        needIndividualization = true;

    Ref<SharedBuffer> licenseRequestMessage = SharedBuffer::create(message.c_str(), message.size());
    callback(WTFMove(licenseRequestMessage), sessionIdValue, needIndividualization, Succeeded);
    sessionIdMap.add(sessionIdValue, WTFMove(initData));
}

void CDMInstanceOpenCDM::updateLicense(const String& sessionId, LicenseType, const SharedBuffer& response, LicenseUpdateCallback callback)
{
    std::string responseMessage;
    int ret = m_openCdmSession->Update(reinterpret_cast<unsigned char*>(const_cast<char*>(response.data())), response.size(), responseMessage);
    GST_DEBUG("session id %s, calling callback %s message", sessionId.utf8().data(), ret ? "with" : "without");
    if (ret) {
        std::string request = "message:";
        if (!responseMessage.compare(0, request.length(), request.c_str())) {
            size_t length = checkMessageLength(responseMessage, request);
            GST_TRACE("message length %u", length);
            Ref<SharedBuffer> nextMessage = SharedBuffer::create((responseMessage.c_str() + length), (responseMessage.length() - length));
            CDMInstance::Message message = std::make_pair(MediaKeyMessageType::LicenseRequest, WTFMove(nextMessage));
            callback(false, std::nullopt, std::nullopt, std::move(message), SuccessValue::Succeeded);
            return;
        }
    }
    SharedBuffer* initData = sessionIdMap.get(sessionId);
    KeyStatusVector changedKeys;
    MediaKeyStatus keyStatus = getKeyStatus(responseMessage);
    changedKeys.append(std::pair<Ref<SharedBuffer>, MediaKeyStatus>{*initData, keyStatus});
    callback(false, WTFMove(changedKeys), std::nullopt, std::nullopt, SuccessValue::Succeeded);
}

CDMInstance::SessionLoadFailure CDMInstanceOpenCDM::getSessionLoadStatus(std::string& loadStatus)
{
    if (loadStatus != "None")
        return CDMInstance::SessionLoadFailure::None;
    else if (loadStatus == "SessionNotFound")
        return CDMInstance::SessionLoadFailure::NoSessionData;
    else if (loadStatus == "MismatchedSessionType")
        return CDMInstance::SessionLoadFailure::MismatchedSessionType;
    else if (loadStatus == "QuotaExceeded")
        return CDMInstance::SessionLoadFailure::QuotaExceeded;
    else
        return CDMInstance::SessionLoadFailure::Other;
}

MediaKeyStatus CDMInstanceOpenCDM::getKeyStatus(std::string& keyStatus)
{
    if (keyStatus == "KeyUsable")
        return MediaKeyStatus::Usable;
    else if (keyStatus == "KeyExpired")
        return  MediaKeyStatus::Expired;
    else if (keyStatus == "KeyOutputRestricted")
        return MediaKeyStatus::OutputRestricted;
    else if (keyStatus == "KeyStatusPending")
        return MediaKeyStatus::OutputRestricted;
    else if (keyStatus == "KeyInternalError")
        return MediaKeyStatus::InternalError;
    else if (keyStatus == "KeyReleased")
        return MediaKeyStatus::Released;
    else
        return MediaKeyStatus::InternalError;
}

size_t CDMInstanceOpenCDM::checkMessageLength(std::string& message, std::string& request) {
    size_t length = 0;
    std::string delimiter = ":Type:";
    std::string requestType = message.substr(0, message.find(delimiter));
    if (requestType.size() && (requestType.size() == (request.size() + 1)))
       length = requestType.size() + delimiter.size();
    else
       length = request.length();
    GST_TRACE("delimiter.size = %u, delimiter = %s, requestType.size = %u, requestType = %s", delimiter.size(), delimiter.c_str(), requestType.size(), requestType.c_str());
    return length;
}

void CDMInstanceOpenCDM::loadSession(LicenseType, const String& sessionId, const String&, LoadSessionCallback callback)
{
    std::string responseMessage;
    SessionLoadFailure sessionFailure = SessionLoadFailure::None;
    int ret = m_openCdmSession->Load(responseMessage);
    if (!ret) {
        std::string request = "message:";
        if (!responseMessage.compare(0, request.length(), request.c_str())) {
            size_t length = checkMessageLength(responseMessage, request);
            GST_TRACE("message length %u", length);
            auto message = SharedBuffer::create((responseMessage.c_str() + length), (responseMessage.length() - length));
            callback(std::nullopt, std::nullopt, std::nullopt, SuccessValue::Succeeded, sessionFailure);
            // TODO : Maybe we need to send the message for partially remove scenario
            // callback(std::nullopt, std::nullopt, std::move(WTFMove(message)), SuccessValue::Succeeded, sessionFailure);
            return;
        }

        SharedBuffer* initData = sessionIdMap.get(sessionId);
        KeyStatusVector knownKeys;
        MediaKeyStatus keyStatus = getKeyStatus(responseMessage);
        knownKeys.append(std::pair<Ref<SharedBuffer>, MediaKeyStatus>{*initData, keyStatus});
        callback(WTFMove(knownKeys), std::nullopt, std::nullopt, SuccessValue::Succeeded, sessionFailure);
        return;
    }

    sessionFailure = getSessionLoadStatus(responseMessage);
    callback(std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed, sessionFailure);
}

void CDMInstanceOpenCDM::closeSession(const String&, CloseSessionCallback callback)
{
    m_openCdmSession->Close();
    callback();
}

void CDMInstanceOpenCDM::removeSessionData(const String& sessionId, LicenseType, RemoveSessionDataCallback callback)
{
    std::string responseMessage;
    KeyStatusVector keys;
    int ret = m_openCdmSession->Remove(responseMessage);
    if (!ret) {
        std::string request = "message:";
        if (!responseMessage.compare(0, request.length(), request.c_str())) {
            size_t length = checkMessageLength(responseMessage, request);
            GST_TRACE("message length %u", length);

            auto message = SharedBuffer::create((responseMessage.c_str() + length), (responseMessage.length() - length));
            SharedBuffer* initData = sessionIdMap.get(sessionId);
            std::string status = "KeyReleased";
            MediaKeyStatus keyStatus = getKeyStatus(status);
            keys.append(std::pair<Ref<SharedBuffer>, MediaKeyStatus>{*initData, keyStatus});
            callback(WTFMove(keys), std::move(WTFMove(message)), SuccessValue::Succeeded);
            return;
        }
    }

    SharedBuffer* initData = sessionIdMap.get(sessionId);
    MediaKeyStatus keyStatus = getKeyStatus(responseMessage);
    keys.append(std::pair<Ref<SharedBuffer>, MediaKeyStatus>{*initData, keyStatus});
    callback(WTFMove(keys), std::nullopt, SuccessValue::Failed);
    return;
}

void CDMInstanceOpenCDM::storeRecordOfKeyUsage(const String&)
{
}

void CDMInstanceOpenCDM::gatherAvailableKeys(AvailableKeysCallback)
{
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(OPENCDM)
