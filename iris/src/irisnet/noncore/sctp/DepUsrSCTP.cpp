#define MS_CLASS "DepUsrSCTP"
// #define MS_LOG_DEV_LEVEL 3

#include "DepUsrSCTP.hpp"
// #include "DepLibUV.hpp"
// #include "Logger.hpp"
#include <usrsctp.h>

#include <QCoreApplication>
#include <QTimer>

#include <cstring>

#define MS_TRACE()
#define MS_WARN_TAG(args, ...)
#define MS_DEBUG_TAG(args, ...)
#define MS_ASSERT(cond, what) Q_ASSERT_X(cond, "sctp", what)

/* Static. */

static constexpr size_t CheckerInterval { 10u }; // In ms.

/* Static methods for usrsctp global callbacks. */

inline static int onSendSctpData(void *addr, void *data, size_t len, uint8_t /*tos*/, uint8_t /*setDf*/)
{
    auto *sctpAssociation = DepUsrSCTP::RetrieveSctpAssociation(reinterpret_cast<uintptr_t>(addr));

    if (!sctpAssociation) {
        MS_WARN_TAG(sctp, "no SctpAssociation found");

        return -1;
    }

    sctpAssociation->OnUsrSctpSendSctpData(data, len);

    // NOTE: Must not free data, usrsctp lib does it.

    return 0;
}

// Static method for printing usrsctp debug.
inline static void sctpDebug(const char *format, ...)
{
    char    buffer[10000];
    va_list ap;

    va_start(ap, format);
    vsprintf(buffer, format, ap);

    // Remove the artificial carriage return set by usrsctp.
    buffer[std::strlen(buffer) - 1] = '\0';

    MS_DEBUG_TAG(sctp, "%s", buffer);

    va_end(ap);
}

/* Static variables. */

DepUsrSCTP::Checker                                  *DepUsrSCTP::checker { nullptr };
uint64_t                                              DepUsrSCTP::numSctpAssociations { 0u };
uintptr_t                                             DepUsrSCTP::nextSctpAssociationId { 0u };
std::unordered_map<uintptr_t, RTC::SctpAssociation *> DepUsrSCTP::mapIdSctpAssociation;

/* Static methods. */

void DepUsrSCTP::ClassInit()
{
    MS_TRACE();

    MS_DEBUG_TAG(info, "usrsctp");

    usrsctp_init_nothreads(0, onSendSctpData, sctpDebug);

    // Disable explicit congestion notifications (ecn).
    usrsctp_sysctl_set_sctp_ecn_enable(0);

#ifdef SCTP_DEBUG
    usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);
#endif

    DepUsrSCTP::checker = new DepUsrSCTP::Checker();
}

void DepUsrSCTP::ClassDestroy()
{
    MS_TRACE();

    usrsctp_finish();

    delete DepUsrSCTP::checker;
}

uintptr_t DepUsrSCTP::GetNextSctpAssociationId()
{
    MS_TRACE();

    // NOTE: usrsctp_connect() fails with a value of 0.
    if (DepUsrSCTP::nextSctpAssociationId == 0u)
        ++DepUsrSCTP::nextSctpAssociationId;

    // In case we've wrapped around and need to find an empty spot from a removed
    // SctpAssociation. Assumes we'll never be full.
    while (DepUsrSCTP::mapIdSctpAssociation.find(DepUsrSCTP::nextSctpAssociationId)
           != DepUsrSCTP::mapIdSctpAssociation.end()) {
        ++DepUsrSCTP::nextSctpAssociationId;

        if (DepUsrSCTP::nextSctpAssociationId == 0u)
            ++DepUsrSCTP::nextSctpAssociationId;
    }

    return DepUsrSCTP::nextSctpAssociationId++;
}

void DepUsrSCTP::RegisterSctpAssociation(RTC::SctpAssociation *sctpAssociation)
{
    MS_TRACE();

    auto it = DepUsrSCTP::mapIdSctpAssociation.find(sctpAssociation->id);

    MS_ASSERT(it == DepUsrSCTP::mapIdSctpAssociation.end(), "the id of the SctpAssociation is already in the map");

    DepUsrSCTP::mapIdSctpAssociation[sctpAssociation->id] = sctpAssociation;

    if (++DepUsrSCTP::numSctpAssociations == 1u)
        DepUsrSCTP::checker->Start();
}

void DepUsrSCTP::DeregisterSctpAssociation(RTC::SctpAssociation *sctpAssociation)
{
    MS_TRACE();

    auto found = DepUsrSCTP::mapIdSctpAssociation.erase(sctpAssociation->id);

    MS_ASSERT(found > 0, "SctpAssociation not found");
    MS_ASSERT(DepUsrSCTP::numSctpAssociations > 0u, "numSctpAssociations was not higher than 0");

    if (--DepUsrSCTP::numSctpAssociations == 0u)
        DepUsrSCTP::checker->Stop();
}

RTC::SctpAssociation *DepUsrSCTP::RetrieveSctpAssociation(uintptr_t id)
{
    MS_TRACE();

    auto it = DepUsrSCTP::mapIdSctpAssociation.find(id);

    if (it == DepUsrSCTP::mapIdSctpAssociation.end())
        return nullptr;

    return it->second;
}

/* DepUsrSCTP::Checker instance methods. */

DepUsrSCTP::Checker::Checker()
{
    MS_TRACE();

    this->timer = new QTimer();
    QObject::connect(this->timer, &QTimer::timeout, qApp, [this]() { OnTimer(); });
}

DepUsrSCTP::Checker::~Checker()
{
    MS_TRACE();

    delete this->timer;
}

void DepUsrSCTP::Checker::Start()
{
    MS_TRACE();

    MS_DEBUG_TAG(sctp, "usrsctp periodic check started");

    this->elapsedTimer.invalidate();

    this->timer->setInterval(CheckerInterval);
    this->timer->start();
}

void DepUsrSCTP::Checker::Stop()
{
    MS_TRACE();

    MS_DEBUG_TAG(sctp, "usrsctp periodic check stopped");

    this->elapsedTimer.invalidate();

    this->timer->stop();
}

void DepUsrSCTP::Checker::OnTimer()
{
    MS_TRACE();

    qint64 elapsedMs = 0;
    if (elapsedTimer.isValid()) {
        elapsedMs = elapsedTimer.restart();
    } else {
        elapsedTimer.start();
    }

    usrsctp_handle_timers(elapsedMs);
}
